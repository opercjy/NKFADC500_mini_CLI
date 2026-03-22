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

    // 💡 maxEvents 파라미터 부활
    void Start(const std::string& outFileName, int maxEvents = 0, int maxTime = 0);
    void Stop();
    bool IsRunning() const { return fIsRunning.load(); }

private:
    void ProducerWorker(int maxTime);
    void ConsumerWorker(const std::string& outFileName, int maxEvents); // 💡 인자 추가

    RunInfo* fRunInfo;
    Fadc500Device* fDevice;

    std::atomic<bool> fIsRunning;
    std::thread fProducerThread;
    std::thread fConsumerThread;

    RawBufferPool fDataQueue; 
    RawBufferPool fFreeQueue; 
};

#endif