#include "BinaryDaqManager.hh"
#include "ELog.hh"
#include <chrono>
#include <cstdio> 

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
    fConsumerThread = std::thread(&BinaryDaqManager::ConsumerWorker, this, outFileName, maxEvents);
    fProducerThread = std::thread(&BinaryDaqManager::ProducerWorker, this, maxTime);
}

void BinaryDaqManager::Stop() {
    // 💡 [수정] fIsRunning 상태와 무관하게 무조건 스레드 회수 절차 진행
    fIsRunning = false; 
    
    // 큐를 강제로 깨워서 대기 중인 스레드들이 빠져나오게 함
    fDataQueue.Stop();
    fFreeQueue.Stop();
    
    // 스레드가 살아있다면 안전하게 병합(Join)
    if (fProducerThread.joinable()) {
        fProducerThread.join();
    }
    if (fConsumerThread.joinable()) {
        fConsumerThread.join();
    }
}

void BinaryDaqManager::ProducerWorker(int maxTime) {
    fDevice->StartDAQ();
    ELog::Print(ELog::INFO, "Producer Thread Started: Polling Hardware...");

    auto start_time = std::chrono::steady_clock::now();

    while (fIsRunning) {
        if (maxTime > 0) {
            auto current_time = std::chrono::steady_clock::now();
            int elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
            if (elapsed >= maxTime) {
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
    ELog::Print(ELog::INFO, "Producer Thread Stopped.");
}

void BinaryDaqManager::ConsumerWorker(const std::string& outFileName, int maxEvents) {
    FILE* fp = fopen(outFileName.c_str(), "wb");
    if (!fp) {
        ELog::Print(ELog::FATAL, Form("Cannot open file %s", outFileName.c_str()));
        fIsRunning = false;
        return;
    }

    ELog::Print(ELog::INFO, Form("Consumer Thread Started: Writing to %s", outFileName.c_str()));
    size_t total_written_bytes = 0;
    int current_events = 0;
    RawBuffer* popBuffer = nullptr;

    while (fDataQueue.WaitAndPop(popBuffer)) {
        if (popBuffer && popBuffer->size > 0) {
            
            size_t written = fwrite(popBuffer->data, 1, popBuffer->size, fp);
            total_written_bytes += written;

            current_events = total_written_bytes / 4096;

            if (maxEvents > 0 && current_events >= maxEvents) {
                ELog::Print(ELog::INFO, Form("Target reached! (Est. %d events). Stopping DAQ...", current_events));
                fIsRunning = false; // 메인 스레드에 종료 알림
                break;
            }
            
            popBuffer->size = 0;
            fFreeQueue.Push(popBuffer); 
        }
    }
    
    fclose(fp);
    ELog::Print(ELog::INFO, Form("Consumer Thread Stopped. Total Written: %.2f MB, Est. Events: %d", total_written_bytes / 1048576.0, current_events));
}