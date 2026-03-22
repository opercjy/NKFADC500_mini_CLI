#include "BinaryDaqManager.hh"
#include "ELog.hh"
#include <iostream>
#include <chrono>
#include <cstdio> // 고속 fwrite 용

BinaryDaqManager::BinaryDaqManager(RunInfo* runInfo) 
    : fRunInfo(runInfo), fDevice(nullptr), fIsRunning(false) 
{
    // 단일 보드(Standalone) 환경 기준 세팅
    FadcBD* bdConfig = fRunInfo->GetFadcBD(0);
    if (!bdConfig) {
        ELog::Print(ELog::FATAL, "No FADC Board configuration found in RunInfo!");
        return;
    }

    // 1. 하드웨어 장치 객체 생성 및 초기화
    fDevice = new Fadc500Device(bdConfig->GetMID());
    fDevice->Initialize(bdConfig);

    // 2. [최적화] 메모리 풀에 4MB 크기의 빈 버퍼 10개를 미리 생성해 둡니다.
    // 수집 중에는 이 10개의 버퍼가 빙글빙글 돌며 재활용됩니다 (Zero-Malloc)
    for (int i = 0; i < 10; i++) {
        fFreeQueue.Push(new RawBuffer(4 * 1024 * 1024)); // 4MB Capacity
    }
}

BinaryDaqManager::~BinaryDaqManager() {
    Stop();
    if (fDevice) {
        delete fDevice;
    }
}

void BinaryDaqManager::Start(const std::string& outFileName) {
    if (fIsRunning) return;
    fIsRunning = true;
    
    // 소비자를 먼저 켜서 디스크를 준비시키고 생산자를 켭니다.
    fConsumerThread = std::thread(&BinaryDaqManager::ConsumerWorker, this, outFileName);
    fProducerThread = std::thread(&BinaryDaqManager::ProducerWorker, this);
}

void BinaryDaqManager::Stop() {
    if (!fIsRunning) return;
    fIsRunning = false;
    
    fDataQueue.Stop();
    fFreeQueue.Stop();

    if (fProducerThread.joinable()) fProducerThread.join();
    if (fConsumerThread.joinable()) fConsumerThread.join();
}

void BinaryDaqManager::ProducerWorker() {
    fDevice->StartDAQ();
    ELog::Print(ELog::INFO, "Producer Thread Started: Polling Hardware...");

    while (fIsRunning) {
        // 1. 하드웨어의 남은 데이터 양(BCOUNT) 확인
        unsigned int bcount = fDevice->FastReadBCOUNT();
        if (bcount == 0) {
            std::this_thread::yield(); // CPU 점유율 양보
            continue;
        }

        uint32_t total_bytes_to_read = bcount * 1024; // 1 bcount = 256 words = 1024 bytes

        // 2. 꽉 찬 큐가 너무 많으면(디스크 병목) 잠시 대기 (Backpressure 방어)
        if (fDataQueue.Size() > 8) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        // 3. 빈 버퍼 풀에서 버퍼 하나를 꺼내옴
        RawBuffer* buffer = nullptr;
        if (!fFreeQueue.TryPop(buffer)) {
            // 극한의 상황: 모든 버퍼가 사용 중이면 새로 하나 만듦
            buffer = new RawBuffer(4 * 1024 * 1024);
        }

        // 혹시 모를 오버플로우 대비 동적 확장
        if (total_bytes_to_read > buffer->capacity) {
            delete[] buffer->data;
            buffer->capacity = total_bytes_to_read + (1024 * 1024); // 여유 1MB 추가
            buffer->data = new unsigned char[buffer->capacity];
        }

        // 4. 하드웨어에서 버퍼로 직접 데이터 복사 (Zero-Copy)
        fDevice->FastReadData(bcount, buffer->data);
        buffer->size = total_bytes_to_read;

        // 5. 꽉 찬 데이터를 큐에 삽입하여 소비자에게 전달
        fDataQueue.Push(buffer);
    }

    fDevice->StopDAQ();
    ELog::Print(ELog::INFO, "Producer Thread Stopped.");
}

void BinaryDaqManager::ConsumerWorker(const std::string& outFileName) {
    // [고속 I/O] C++ fstream 보다 압도적으로 빠른 C 표준 I/O 사용
    FILE* fp = fopen(outFileName.c_str(), "wb");
    if (!fp) {
        ELog::Print(ELog::FATAL, Form("Cannot open file %s for binary writing!", outFileName.c_str()));
        fIsRunning = false;
        return;
    }

    ELog::Print(ELog::INFO, Form("Consumer Thread Started: Writing to %s", outFileName.c_str()));

    size_t total_written_bytes = 0;
    RawBuffer* popBuffer = nullptr;

    while (fDataQueue.WaitAndPop(popBuffer)) {
        if (popBuffer && popBuffer->size > 0) {
            // 1. 하드디스크에 바이너리 덤프
            size_t written = fwrite(popBuffer->data, 1, popBuffer->size, fp);
            total_written_bytes += written;

            // 2. 데이터 크기 리셋 후 빈 버퍼 풀에 반납 (재활용)
            popBuffer->size = 0;
            fFreeQueue.Push(popBuffer);
        }
    }

    fclose(fp);
    ELog::Print(ELog::INFO, Form("Consumer Thread Stopped. Total written: %.2f MB", total_written_bytes / 1048576.0));
}
