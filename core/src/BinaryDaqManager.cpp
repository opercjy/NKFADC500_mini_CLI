#include "BinaryDaqManager.hh"
#include "ELog.hh"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <ctime>

BinaryDaqManager::BinaryDaqManager(RunInfo* runInfo) 
    : fRunInfo(runInfo), fDevice(nullptr), fIsRunning(false) 
{
    FadcBD* bdConfig = fRunInfo->GetFadcBD(0);
    if (!bdConfig) return;

    fDevice = new Fadc500Device(bdConfig->GetMID());
    fDevice->Initialize(bdConfig);

    for (int i = 0; i < 10; i++) {
        fFreeQueue.Push(new RawBuffer(4 * 1024 * 1024));
    }
}

BinaryDaqManager::~BinaryDaqManager() {
    Stop();
    if (fDevice) delete fDevice;
}

void BinaryDaqManager::Start(const std::string& outFileName, int maxEvents, int maxTime) {
    if (fIsRunning) return;
    fIsRunning = true;
    
    // 💡 [UI 개선 1] 시작 시간을 상단 배너 직전에 출력 (frontend_main에서 출력하던 부분을 이쪽으로 일원화하면 더 깔끔하지만, 현재 구조상 Monitor 배너에 추가)
    auto now = std::chrono::system_clock::now();
    std::time_t start_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::cout << "\n\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m       NKFADC500 Mini Binary DAQ is RUNNING\033[0m\n";
    std::cout << "       [Start Time]  " << std::put_time(std::localtime(&start_time_t), "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "       [Target File] " << outFileName << "\n";
    if (maxEvents > 0) std::cout << "       [Limit]       " << maxEvents << " Events\n";
    if (maxTime > 0)   std::cout << "       [Limit]       " << maxTime << " Seconds\n";
    std::cout << "\033[1;36m========================================================\033[0m\n\n";

    fConsumerThread = std::thread(&BinaryDaqManager::ConsumerWorker, this, outFileName, maxEvents);
    fProducerThread = std::thread(&BinaryDaqManager::ProducerWorker, this, maxTime);
}

void BinaryDaqManager::Stop() {
    fIsRunning = false; 
    fDataQueue.Stop();
    fFreeQueue.Stop();
    if (fProducerThread.joinable()) fProducerThread.join();
    if (fConsumerThread.joinable()) fConsumerThread.join();
}

void BinaryDaqManager::ProducerWorker(int maxTime) {
    fDevice->StartDAQ();
    auto start_time = std::chrono::steady_clock::now();

    while (fIsRunning) {
        if (maxTime > 0) {
            auto current_time = std::chrono::steady_clock::now();
            int elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
            if (elapsed >= maxTime) {
                std::cout << "\n"; 
                ELog::Print(ELog::INFO, Form("Time limit reached (%d sec). Stopping DAQ...", maxTime));
                fIsRunning = false;
                break;
            }
        }

        unsigned int bcount_kb = fDevice->ReadBCOUNT();

        if (bcount_kb == 0xFFFFFFFF || bcount_kb > 16384) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (bcount_kb == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100)); 
            continue;
        }
        if (bcount_kb > 4096) bcount_kb = 4096;
        
        uint32_t total_bytes_to_read = bcount_kb * 1024; 

        if (fDataQueue.Size() > 8) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        RawBuffer* buffer = nullptr;
        if (!fFreeQueue.TryPop(buffer)) {
            buffer = new RawBuffer(4 * 1024 * 1024);
        }

        if (total_bytes_to_read > buffer->capacity) {
            delete[] buffer->data;
            buffer->capacity = total_bytes_to_read + (1024 * 1024); 
            buffer->data = new unsigned char[buffer->capacity];
        }

        fDevice->ReadDATA(bcount_kb, buffer->data);
        buffer->size = total_bytes_to_read;
        fDataQueue.Push(buffer);
    }
    
    fDevice->StopDAQ();
    fDataQueue.Stop(); 
}

void BinaryDaqManager::ConsumerWorker(const std::string& outFileName, int maxEvents) {
    FILE* fp = fopen(outFileName.c_str(), "wb");
    if (!fp) {
        std::cout << "\n";
        ELog::Print(ELog::FATAL, Form("Cannot open file %s", outFileName.c_str()));
        fIsRunning = false;
        return;
    }

    size_t total_written_bytes = 0;
    int current_events = 0;
    
    auto sys_start_time = std::chrono::system_clock::now();
    auto ui_timer = std::chrono::steady_clock::now();
    auto perf_start_time = std::chrono::steady_clock::now(); 
    
    int last_print_events = 0;
    size_t last_print_bytes = 0;

    // 초기 모니터 배너 출력
    std::cout << "\033[1;36m[  DAQ Real-time Monitor  ]\033[0m\n";

    while (fIsRunning || fDataQueue.Size() > 0) {
        RawBuffer* popBuffer = nullptr;
        
        if (fDataQueue.TryPop(popBuffer)) {
            if (popBuffer && popBuffer->size > 0) {
                size_t written = fwrite(popBuffer->data, 1, popBuffer->size, fp);
                total_written_bytes += written;
                
                current_events = total_written_bytes / 4096;

                if (maxEvents > 0 && current_events >= maxEvents) {
                    std::cout << "\n\n"; 
                    ELog::Print(ELog::INFO, Form("Target reached! (Est. %d events). Stopping DAQ...", current_events));
                    fIsRunning = false; 
                }
                
                popBuffer->size = 0;
                fFreeQueue.Push(popBuffer); 
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto current_time = std::chrono::steady_clock::now();
        double ui_elapsed_sec = std::chrono::duration<double>(current_time - ui_timer).count();
        
        if (ui_elapsed_sec >= 0.5) {
            double speed_mbps = (((total_written_bytes - last_print_bytes) / 1048576.0) / ui_elapsed_sec);
            double evt_rate = (current_events - last_print_events) / ui_elapsed_sec;
            
            // 💡 [UI 개선 2] 현재까지의 총 진행 시간(Total Elapsed) 계산
            double total_elapsed = std::chrono::duration<double>(current_time - perf_start_time).count();
            
            // 터미널 줄 맨 위로 올라가서 배너 내용 덮어쓰기 (\033[F 로 이전 줄 이동)
            std::cout << "\r\033[F\033[K" << "\033[1;36m[  DAQ Real-time Monitor  ]\033[0m"
                      << "  ( Elapsed: \033[1;32m" << std::fixed << std::setprecision(1) << total_elapsed << " s\033[0m )\n"
                      << "\r\033[K" 
                      << "   \033[1;33mEvents:\033[0m " << std::setw(7) << current_events << " | "
                      << "\033[1;32mSize:\033[0m " << std::setw(6) << std::fixed << std::setprecision(2) << (total_written_bytes / 1048576.0) << " MB | "
                      << "\033[1;35mRate:\033[0m " << std::setw(6) << std::fixed << std::setprecision(1) << evt_rate << " Hz | "
                      << "\033[1;34mSpeed:\033[0m " << std::setw(6) << std::fixed << std::setprecision(2) << speed_mbps << " MB/s" 
                      << std::flush;
            
            ui_timer = current_time;
            last_print_events = current_events;
            last_print_bytes = total_written_bytes;
        }
    }
    
    fclose(fp);
    std::cout << "\n\n";
    
    auto sys_end_time = std::chrono::system_clock::now();
    auto perf_end_time = std::chrono::steady_clock::now();
    
    std::time_t start_c = std::chrono::system_clock::to_time_t(sys_start_time);
    std::time_t end_c = std::chrono::system_clock::to_time_t(sys_end_time);
    
    std::chrono::duration<double> total_elapsed = perf_end_time - perf_start_time;
    double total_sec = total_elapsed.count();
    double avg_rate = (total_sec > 0) ? (current_events / total_sec) : 0.0;
    
    std::cout << "\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m   [ Run Summary ]\033[0m\n";
    std::cout << "   Start Time    : " << std::put_time(std::localtime(&start_c), "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "   End Time      : " << std::put_time(std::localtime(&end_c), "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "   Elapsed Time  : " << std::fixed << std::setprecision(2) << total_sec << " sec\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << Form("   Total Events  : %d\n", current_events);
    std::cout << Form("   Total Written : %.2f MB\n", total_written_bytes / 1048576.0);
    std::cout << "   Avg Trig Rate : " << std::fixed << std::setprecision(2) << avg_rate << " Hz\n";
    std::cout << "\033[1;36m========================================================\033[0m\n";
}