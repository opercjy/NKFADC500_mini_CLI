#include "BinaryDaqManager.hh"
#include "Fadc500Device.hh"
#include "ELog.hh"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

namespace {
std::string FormatTimestamp(std::time_t t) {
    struct tm tm_buf {};
    localtime_r(&t, &tm_buf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return std::string(buf);
}
} // namespace

BinaryDaqManager::BinaryDaqManager(RunInfo* runInfo)
    : fRunInfo(runInfo), fDevice(nullptr), fIsRunning(false)
{
    if (!fRunInfo) return;
    FadcBD* bdConfig = fRunInfo->GetFadcBD(0);
    if (!bdConfig) return;

    fDevice = new Fadc500Device(bdConfig->GetMID());
    fDevice->Initialize(bdConfig);

    // Pre-allocate 64 x 4 MB (256 MB) DMA ring pool to absorb OS/disk jitter
    for (int i = 0; i < 64; i++) {
        fFreeQueue.Push(new RawBuffer(4 * 1024 * 1024));
    }
}

BinaryDaqManager::~BinaryDaqManager() {
    Stop();
    delete fDevice;
    fDevice = nullptr;
}

void BinaryDaqManager::Start(const std::string& outFileName, int maxEvents, int maxTime) {
    if (fIsRunning.load() || !fDevice || !fRunInfo) return;

    // Clean up any previously completed worker threads before starting a new run
    if (fProducerThread.joinable()) fProducerThread.join();
    if (fConsumerThread.joinable()) fConsumerThread.join();
    fDataQueue.Reset();
    fFreeQueue.Reset();

    fIsRunning.store(true);

    auto now = std::chrono::system_clock::now();
    std::time_t start_time_t = std::chrono::system_clock::to_time_t(now);

    FadcBD* bd = fRunInfo->GetFadcBD(0);
    const size_t expected_evt_bytes = static_cast<size_t>(std::max(1, bd->GetRL())) * 512ULL;

    std::cout << "\n\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m       NKFADC500 Mini Binary DAQ is RUNNING\033[0m\n";
    std::cout << "       [Start Time]  " << FormatTimestamp(start_time_t) << "\n";
    std::cout << "       [Target File] " << outFileName << "\n";
    std::cout << "       [Config (1)]  RL: " << bd->GetRL() << " (" << expected_evt_bytes << " B/evt)"
              << " | TLT: 0x" << std::hex << bd->GetTLT() << std::dec
              << " | CW: " << bd->GetCW(0) << "\n";
    std::cout << "       [Config (2)]  POL: " << bd->GetPOL(0)
              << " | DLY: " << bd->GetDLY(0)
              << " | DACOFF: " << bd->GetDACOFF(0) << "\n";
    std::cout << "       [Config (3)]  THR: " << bd->GetTHR(0) << "\n";

    if (maxEvents > 0) std::cout << "       [Limit]       " << maxEvents << " Events\n";
    if (maxTime > 0)   std::cout << "       [Limit]       " << maxTime << " Seconds\n";
    std::cout << "\033[1;36m========================================================\033[0m\n\n";

    fConsumerThread = std::thread(&BinaryDaqManager::ConsumerWorker, this, outFileName, maxEvents);
    fProducerThread = std::thread(&BinaryDaqManager::ProducerWorker, this, maxTime);
}

void BinaryDaqManager::Stop() {
    fIsRunning.store(false);
    fDataQueue.Stop();
    fFreeQueue.Stop();
    if (fProducerThread.joinable()) fProducerThread.join();
    if (fConsumerThread.joinable()) fConsumerThread.join();
}

void BinaryDaqManager::ProducerWorker(int maxTime) {
    fDevice->StartDAQ();
    auto start_time = std::chrono::steady_clock::now();

    while (fIsRunning.load()) {
        if (maxTime > 0) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
            if (elapsed >= maxTime) {
                std::cout << "\n";
                ELog::Print(ELog::INFO, "Time limit reached (" + std::to_string(maxTime) + " sec). Stopping DAQ...");
                fIsRunning.store(false);
                break;
            }
        }

        unsigned int raw_bcount = fDevice->ReadBCOUNT();

        if (raw_bcount == 0xFFFFFFFFU) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        unsigned int bcount_kb = raw_bcount & 0x0000FFFFU;

        if (bcount_kb == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        if (bcount_kb > 4096) bcount_kb = 4096;

        const size_t total_bytes_to_read = static_cast<size_t>(bcount_kb) * 1024ULL;

        // Backpressure guard when disk writer falls >50 chunks behind
        if (fDataQueue.Size() > 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        RawBuffer* buffer = nullptr;
        if (!fFreeQueue.TryPop(buffer)) {
            buffer = new RawBuffer(4 * 1024 * 1024);
        }

        buffer->EnsureCapacity(total_bytes_to_read);

        fDevice->ReadDATA(bcount_kb, buffer->data);
        buffer->size = total_bytes_to_read;
        fDataQueue.Push(buffer);
    }

    fDevice->StopDAQ();
    fDataQueue.Stop();
}

void BinaryDaqManager::ConsumerWorker(const std::string& outFileName, int maxEvents) {
    FILE* fp = std::fopen(outFileName.c_str(), "wb");
    if (!fp) {
        std::cout << "\n";
        fIsRunning.store(false);
        ELog::Print(ELog::FATAL, "Cannot open output file: " + outFileName);
        return;
    }

    // 16 MB stdio write buffer to minimize syscall jitter
    std::setvbuf(fp, nullptr, _IOFBF, 16 * 1024 * 1024);

    // Compute exact 4-channel interleaved packet size from RECORD_LEN (RL * 128 B/ch * 4 ch = RL * 512 B/evt)
    size_t bytes_per_event = 4096ULL;
    if (FadcBD* bd = fRunInfo->GetFadcBD(0)) {
        if (bd->GetRL() > 0) {
            bytes_per_event = static_cast<size_t>(bd->GetRL()) * 512ULL;
        }
    }
    bool packet_size_verified = false;

    size_t total_written_bytes = 0;
    uint64_t current_events = 0;

    auto sys_start_time = std::chrono::system_clock::now();
    auto ui_timer = std::chrono::steady_clock::now();
    auto perf_start_time = ui_timer;

    uint64_t last_print_events = 0;
    size_t last_print_bytes = 0;

    while (fIsRunning.load() || fDataQueue.Size() > 0) {
        RawBuffer* popBuffer = nullptr;

        if (fDataQueue.WaitAndPopFor(popBuffer, std::chrono::milliseconds(50))) {
            if (popBuffer) {
                if (popBuffer->size > 0) {
                    // Self-verify packet size from the first valid 128-byte FPGA header
                    if (!packet_size_verified && popBuffer->size >= 128) {
                        uint32_t ch_data_len =
                            static_cast<uint32_t>(popBuffer->data[0]) |
                            (static_cast<uint32_t>(popBuffer->data[4]) << 8) |
                            (static_cast<uint32_t>(popBuffer->data[8]) << 16) |
                            (static_cast<uint32_t>(popBuffer->data[12]) << 24);
                        if (ch_data_len >= 128U && ch_data_len <= 1048576U && (ch_data_len % 128U == 0U)) {
                            bytes_per_event = static_cast<size_t>(ch_data_len) * 4ULL;
                            packet_size_verified = true;
                        }
                    }

                    size_t written = std::fwrite(popBuffer->data, 1, popBuffer->size, fp);
                    total_written_bytes += written;
                    if (written != popBuffer->size) {
                        ELog::Print(ELog::ERROR,
                            "Disk write error on " + outFileName + " (wrote " +
                            std::to_string(written) + " / " + std::to_string(popBuffer->size) +
                            " bytes). Stopping DAQ safely.");
                        fIsRunning.store(false);
                    }

                    current_events = total_written_bytes / bytes_per_event;

                    if (maxEvents > 0 && current_events >= static_cast<uint64_t>(maxEvents)) {
                        std::cout << "\n\n";
                        ELog::Print(ELog::INFO,
                            "Target reached! (" + std::to_string(current_events) + " events). Stopping DAQ...");
                        fIsRunning.store(false);
                    }
                }
                // Always recycle popped buffer back into free pool (prevents leak when size == 0)
                popBuffer->size = 0;
                fFreeQueue.Push(popBuffer);
            }
        }

        auto current_time = std::chrono::steady_clock::now();
        double ui_elapsed_sec = std::chrono::duration<double>(current_time - ui_timer).count();

        if (ui_elapsed_sec >= 0.5) {
            double speed_mbps = ((total_written_bytes - last_print_bytes) / 1048576.0) / ui_elapsed_sec;
            double evt_rate = static_cast<double>(current_events - last_print_events) / ui_elapsed_sec;
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

    std::fflush(fp);
    std::fclose(fp);
    std::cout << "\n\n";

    auto sys_end_time = std::chrono::system_clock::now();
    auto perf_end_time = std::chrono::steady_clock::now();

    std::time_t start_c = std::chrono::system_clock::to_time_t(sys_start_time);
    std::time_t end_c = std::chrono::system_clock::to_time_t(sys_end_time);

    double total_sec = std::chrono::duration<double>(perf_end_time - perf_start_time).count();
    double avg_rate = (total_sec > 0.0) ? (static_cast<double>(current_events) / total_sec) : 0.0;

    std::cout << "\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m   [ Run Summary ]\033[0m\n";
    std::cout << "   Start Time    : " << FormatTimestamp(start_c) << "\n";
    std::cout << "   End Time      : " << FormatTimestamp(end_c) << "\n";
    std::cout << "   Elapsed Time  : " << std::fixed << std::setprecision(2) << total_sec << " sec\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << "   Total Events  : " << current_events << "\n";
    std::cout << "   Total Written : " << std::fixed << std::setprecision(2) << (total_written_bytes / 1048576.0) << " MB\n";
    std::cout << "   Avg Trig Rate : " << std::fixed << std::setprecision(2) << avg_rate << " Hz\n";
    std::cout << "\033[1;36m========================================================\033[0m\n";
}
