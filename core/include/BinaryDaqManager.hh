#ifndef BINARYDAQMANAGER_HH
#define BINARYDAQMANAGER_HH

#include "RunInfo.hh"
#include "Fadc500Device.hh"
#include "RawBufferPool.hh"
#include <thread>
#include <atomic>
#include <string>

// 💡 [Phase 6] ZMQ C++ 헤더 포함 (ZeroMQ)
#include <zmq.hpp> 

class BinaryDaqManager {
private:
    RunInfo* fRunInfo;
    Fadc500Device* fDevice;
    
    std::atomic<bool> fIsRunning;
    std::thread fProducerThread;
    std::thread fConsumerThread;

    ThreadSafeQueue fDataQueue;
    ThreadSafeQueue fFreeQueue;

    // 💡 [Phase 6] ZMQ IPC 브로드캐스팅용 컨텍스트 및 소켓
    zmq::context_t fZmqContext;
    zmq::socket_t fZmqPublisher;

    // 💡 [Phase 6] Torn Event 방어를 위한 잔여 버퍼 (Residual Buffer)
    unsigned char* fResidualBuffer;
    uint32_t fResidualSize;
    uint32_t fResidualCapacity;

    void ProducerWorker(int maxTime);
    void ConsumerWorker(const std::string& outFileName, int maxEvents);

public:
    BinaryDaqManager(RunInfo* runInfo);
    ~BinaryDaqManager();

    void Start(const std::string& outFileName, int maxEvents = 0, int maxTime = 0);
    void Stop();
    bool IsRunning() const { return fIsRunning; }
};

#endif
