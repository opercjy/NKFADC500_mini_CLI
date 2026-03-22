#ifndef BINARYDAQMANAGER_HH
#define BINARYDAQMANAGER_HH

#include <thread>
#include <atomic>
#include <string>

#include "Fadc500Device.hh"
#include "RawBufferPool.hh"
#include "RunInfo.hh"

class BinaryDaqManager {
public:
    BinaryDaqManager(RunInfo* runInfo);
    ~BinaryDaqManager();

    void Start(const std::string& outFileName);
    void Stop();
    bool IsRunning() const { return fIsRunning.load(); }

private:
    void ProducerWorker();
    void ConsumerWorker(const std::string& outFileName);

    RunInfo* fRunInfo;
    Fadc500Device* fDevice;

    std::atomic<bool> fIsRunning;
    std::thread fProducerThread;
    std::thread fConsumerThread;

    // [핵심] 메모리 파편화를 막는 두 개의 풀(Pool)
    RawBufferPool fDataQueue; // 꽉 찬 데이터가 대기하는 큐
    RawBufferPool fFreeQueue; // 쓰기가 끝나 비어있는 버퍼가 대기하는 큐
};

#endif
