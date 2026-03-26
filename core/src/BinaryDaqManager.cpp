#include "BinaryDaqManager.hh" // 💡 누락되었던 클래스 정의 헤더 추가
#include "Fadc500Device.hh"
#include "ELog.hh"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <thread>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstring>

BinaryDaqManager::BinaryDaqManager(RunInfo* runInfo) 
    : fRunInfo(runInfo), fDevice(nullptr), fIsRunning(false) 
{
    FadcBD* bdConfig = fRunInfo->GetFadcBD(0);
    if (!bdConfig) return;

    fDevice = new Fadc500Device(bdConfig->GetMID());
    fDevice->Initialize(bdConfig);

    // 💡 [병목 픽스 1] 큐(Pool) 사이즈 10개(40MB) -> 64개(256MB)로 대폭 확장하여 버퍼링 Jitter 흡수
    for (int i = 0; i < 64; i++) { 
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
    
    FadcBD* bd = fRunInfo->GetFadcBD(0);
    
    std::cout << "\n\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m       NKFADC500 Mini Binary DAQ is RUNNING\033[0m\n";
    std::cout << "       [Start Time]  " << std::put_time(std::localtime(&start_time_t), "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "       [Target File] " << outFileName << "\n";
    std::cout << "       [Config (1)]  RL: " << bd->GetRL() << " | TLT: 0x" << std::hex << bd->GetTLT() << std::dec << " | CW: " << bd->GetCW(0) << "\n";
    std::cout << "       [Config (2)]  POL: " << bd->GetPOL(0) << " | DLY: " << bd->GetDLY(0) << " | DACOFF: " << bd->GetDACOFF(0) << "\n";
    std::cout << "       [Config (3)]  THR: " << bd->GetTHR(0) << "\n";
    
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
                // 💡 ROOT의 Form 대신 순수 C++ 문자열 합치기 사용
                ELog::Print(ELog::INFO, "Time limit reached (" + std::to_string(maxTime) + " sec). Stopping DAQ...");
                fIsRunning = false;
                break;
            }
        }

        unsigned int raw_bcount = fDevice->ReadBCOUNT();

        if (raw_bcount == 0xFFFFFFFF) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        unsigned int bcount_kb = raw_bcount & 0x0000FFFF;

        if (bcount_kb == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100)); 
            continue;
        }
        
        if (bcount_kb > 4096) bcount_kb = 4096;
        
        uint32_t total_bytes_to_read = bcount_kb * 1024; 

        // 💡 [병목 픽스 2] 큐 사이즈 백프레셔(Backpressure) 허용치 대폭 상향 (8 -> 50)
        if (fDataQueue.Size() > 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
        ELog::Print(ELog::FATAL, "Cannot open file " + outFileName);
        fIsRunning = false;
        return;
    }

    // 💡 [병목 픽스 4] 디스크 I/O Jitter 방지를 위해 16MB C표준 커널 버퍼링 설정
    setvbuf(fp, NULL, _IOFBF, 16 * 1024 * 1024);

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
                    ELog::Print(ELog::INFO, "Target reached! (Est. " + std::to_string(current_events) + " events). Stopping DAQ...");
                    fIsRunning = false; 
                }
                
                popBuffer->size = 0;
                fFreeQueue.Push(popBuffer); 
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        auto current_time = std::chrono::steady_clock::now();
        double ui_elapsed_sec = std::chrono::duration<double>(current_time - ui_timer).count();
        
        if (ui_elapsed_sec >= 0.5) {
            double speed_mbps = (((total_written_bytes - last_print_bytes) / 1048576.0) / ui_elapsed_sec);
            double evt_rate = (current_events - last_print_events) / ui_elapsed_sec;
            double total_elapsed = std::chrono::duration<double>(current_time - perf_start_time).count();
            
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
    std::cout << "   Total Events  : " << current_events << "\n";
    std::cout << "   Total Written : " << std::fixed << std::setprecision(2) << (total_written_bytes / 1048576.0) << " MB\n";
    std::cout << "   Avg Trig Rate : " << std::fixed << std::setprecision(2) << avg_rate << " Hz\n";
    std::cout << "\033[1;36m========================================================\033[0m\n";
}