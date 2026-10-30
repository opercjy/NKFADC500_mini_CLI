#ifndef RAWBUFFERPOOL_HH
#define RAWBUFFERPOOL_HH

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <utility>

// Contiguous binary DMA buffer with strict Rule-of-5 ownership (no accidental shallow copy)
struct RawBuffer {
    unsigned char* data;
    size_t size;
    size_t capacity;

    explicit RawBuffer(size_t cap)
        : data(new unsigned char[cap]), size(0), capacity(cap) {}

    ~RawBuffer() {
        delete[] data;
    }

    RawBuffer(const RawBuffer&) = delete;
    RawBuffer& operator=(const RawBuffer&) = delete;

    RawBuffer(RawBuffer&& other) noexcept
        : data(std::exchange(other.data, nullptr)),
          size(std::exchange(other.size, 0)),
          capacity(std::exchange(other.capacity, 0)) {}

    RawBuffer& operator=(RawBuffer&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = std::exchange(other.data, nullptr);
            size = std::exchange(other.size, 0);
            capacity = std::exchange(other.capacity, 0);
        }
        return *this;
    }

    void EnsureCapacity(size_t required) {
        if (required > capacity) {
            delete[] data;
            capacity = required;
            data = new unsigned char[capacity];
        }
    }
};

class RawBufferPool {
public:
    RawBufferPool() : _stop(false) {}
    ~RawBufferPool() {
        RawBuffer* buf = nullptr;
        while (TryPop(buf)) {
            delete buf;
        }
    }

    RawBufferPool(const RawBufferPool&) = delete;
    RawBufferPool& operator=(const RawBufferPool&) = delete;

    void Push(RawBuffer* item) {
        if (!item) return;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(item);
        }
        _cv.notify_one();
    }

    bool WaitAndPop(RawBuffer*& popped_item) {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this]() { return !_queue.empty() || _stop.load(); });
        if (_queue.empty()) return false;
        popped_item = _queue.front();
        _queue.pop();
        return true;
    }

    bool WaitAndPopFor(RawBuffer*& popped_item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_cv.wait_for(lock, timeout, [this]() { return !_queue.empty() || _stop.load(); })) {
            return false;
        }
        if (_queue.empty()) return false;
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

    void Stop() {
        _stop.store(true);
        _cv.notify_all();
    }

    void Reset() {
        _stop.store(false);
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

private:
    std::queue<RawBuffer*> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic<bool> _stop;
};

#endif
