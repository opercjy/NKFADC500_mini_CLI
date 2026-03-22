#ifndef RAWBUFFERPOOL_HH
#define RAWBUFFERPOOL_HH

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

// 순수 바이너리 데이터를 담을 구조체
struct RawBuffer {
    unsigned char* data;
    size_t size;
    size_t capacity;

    RawBuffer(size_t cap) : size(0), capacity(cap) {
        data = new unsigned char[capacity];
    }
    ~RawData() { delete[] data; }
};

class RawBufferPool {
public:
    RawBufferPool() : _stop(false) {}
    ~RawBufferPool() {
        RawBuffer* buf;
        while (TryPop(buf)) delete buf;
    }

    void Push(RawBuffer* item) {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push(item);
        _cv.notify_one();
    }

    bool WaitAndPop(RawBuffer*& popped_item) {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this]() { return !_queue.empty() || _stop.load(); });
        if (_queue.empty() && _stop.load()) return false;
        popped_item = _queue.front();
        _queue.pop();
        return true;
    }

    bool TryPop(RawBuffer*& popped_item) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_queue.empty()) return false;
        popped_item = _queue.front();
        _queue.pop();
        return true;
    }

    void Stop() { _stop.store(true); _cv.notify_all(); }
    size_t Size() const { std::lock_guard<std::mutex> lock(_mutex); return _queue.size(); }

private:
    std::queue<RawBuffer*> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _stop;
};

#endif
