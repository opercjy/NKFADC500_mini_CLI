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

        // 💡 [필수 추가] 128KB 강제 정렬! 하드웨어 뻗음 방지
        bcount_kb = (bcount_kb / 128) * 128;

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
            double total_elapsed = std::chrono::duration<double>(current_time - perf_start_time).count();
            
            // 💡 [버그 픽스] \r을 빼고 \n을 넣어서 OS 파이프 버퍼링 체증을 해소!
            std::cout << "[LIVE DAQ] "
                      << "Time: \033[1;32m" << std::fixed << std::setprecision(1) << total_elapsed << "s\033[0m | "
                      << "Events: " << current_events << " | "
                      << "Size: " << std::fixed << std::setprecision(2) << (total_written_bytes / 1048576.0) << " MB | "
                      << "Rate: " << std::fixed << std::setprecision(1) << evt_rate << " Hz | "
                      << "Speed: " << std::fixed << std::setprecision(2) << speed_mbps << " MB/s | "
                      << "DataQ: " << fDataQueue.Size() << " | "
                      << "Pool: " << fFreeQueue.Size() << "\n" << std::flush;
            
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