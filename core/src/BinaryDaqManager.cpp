#include "BinaryDaqManager.hh"
#include "ELog.hh"
#include "Fadc500Device.hh"
#include "NoticeFadc500Spec.hh"

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

size_t ExpectedEventBytes(const FadcBD* bd) {
    if (!bd) return 4096ULL;
    if (bd->GetZEROSUP() != 0) return 128ULL;
    return static_cast<size_t>(std::max(1, bd->GetRL())) * 512ULL;
}
} // namespace

BinaryDaqManager::BinaryDaqManager(RunInfo* runInfo)
    : fRunInfo(runInfo), fDevice(nullptr), fIsRunning(false), fProducerActive(false)
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

    // Guarantee zero residual buffer bleed-over across runs by recycling any leftover queue items
    RawBuffer* leftover = nullptr;
    while (fDataQueue.TryPop(leftover)) {
        if (leftover) {
            leftover->size = 0;
            fFreeQueue.Push(leftover);
        }
    }
    fDataQueue.Reset();
    fFreeQueue.Reset();

    fIsRunning.store(true);
    fProducerActive.store(true);

    auto now = std::chrono::system_clock::now();
    std::time_t start_time_t = std::chrono::system_clock::to_time_t(now);

    FadcBD* bd = fRunInfo->GetFadcBD(0);
    const size_t expected_evt_bytes = ExpectedEventBytes(bd);

    std::cout << "\n\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m       NKFADC500 Mini Binary DAQ is RUNNING\033[0m\n";
    std::cout << "       [Start Time]  " << FormatTimestamp(start_time_t) << "\n";
    std::cout << "       [Target File] " << outFileName << "\n";
    std::cout << "       [Config (1)]  RL: " << bd->GetRL() << " (" << expected_evt_bytes << " B/evt)"
              << " | DSR: /" << bd->GetSAMPLING()
              << " | TLT: 0x" << std::hex << bd->GetTLT() << std::dec
              << " | CW: " << bd->GetCW(0) << "\n";
    std::cout << "       [Config (2)]  POL: " << bd->GetPOL(0)
              << " | DLY: " << bd->GetDLY(0)
              << " | DACOFF: " << bd->GetDACOFF(0)
              << " | TMODE: " << bd->GetTMODE(0) << "\n";
    std::cout << "       [Config (3)]  THR: " << bd->GetTHR(0) << "\n";

    if (maxEvents > 0) std::cout << "       [Limit]       " << maxEvents << " Events\n";
    if (maxTime > 0)   std::cout << "       [Limit]       " << maxTime << " Seconds\n";
    std::cout << "\033[1;36m========================================================\033[0m\n\n";

    fConsumerThread = std::thread(&BinaryDaqManager::ConsumerWorker, this, outFileName, maxEvents);
    fProducerThread = std::thread(&BinaryDaqManager::ProducerWorker, this, maxTime);
}

void BinaryDaqManager::Stop() {
    fIsRunning.store(false);
    if (fProducerThread.joinable()) fProducerThread.join();
    fProducerActive.store(false);
    fDataQueue.Stop();
    fFreeQueue.Stop();
    if (fConsumerThread.joinable()) fConsumerThread.join();
}

void BinaryDaqManager::ProducerWorker(int maxTime) {
    const FadcBD* bd = fRunInfo ? fRunInfo->GetFadcBD(0) : nullptr;
    const size_t event_bytes = ExpectedEventBytes(bd);
    // Minimum complete-event KB quantum (1 KB for RL=1,2 [2,1 evts]; 2..128 KB for RL=4..256)
    const uint32_t event_align_kb = static_cast<uint32_t>(std::max<size_t>(1ULL, (event_bytes + 1023ULL) / 1024ULL));
    // Active streaming minimum transfer quantum: multiple of BOTH 16 KB FX3 burst (usb3com.c) AND event_align_kb
    const uint32_t burst_align_kb = std::max(NoticeFadc500Spec::kUsbBurstAlignKb, event_align_kb);
    // Preferred high-throughput slice alignment from vendor nkfadc500_4_test.C:262 (128 KB)
    const uint32_t stream_align_kb = std::max(NoticeFadc500Spec::kStreamSliceAlignKb, burst_align_kb);
    constexpr uint32_t kMaxChunkKb = 4096U; // 4 MB per DMA buffer (exact multiple of 128 KB)

    fDevice->StartDAQ();
    const auto start_time = std::chrono::steady_clock::now();
    auto last_read_time = start_time;

    auto pushReadKb = [&](uint32_t read_kb) {
        if (read_kb == 0U) return;
        const size_t total_bytes_to_read = static_cast<size_t>(read_kb) * 1024ULL;
        RawBuffer* buffer = nullptr;
        if (!fFreeQueue.TryPop(buffer)) {
            buffer = new RawBuffer(4 * 1024 * 1024);
        }
        buffer->EnsureCapacity(total_bytes_to_read);
        fDevice->ReadDATA(read_kb, buffer->data);
        buffer->size = total_bytes_to_read;
        fDataQueue.Push(buffer);
    };

    while (fIsRunning.load()) {
        const auto current_time = std::chrono::steady_clock::now();
        if (maxTime > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
            if (elapsed >= maxTime) {
                std::cout << "\n";
                ELog::Print(ELog::INFO, "Time limit reached (" + std::to_string(maxTime) + " sec). Stopping DAQ...");
                fIsRunning.store(false);
                break;
            }
        }

        // Full 32-bit BCOUNT (0..65536 KB); never mask with 0xFFFF!
        const uint32_t bcount_kb = fDevice->ReadBCOUNT();
        if (bcount_kb == NoticeFadc500Spec::kUsbReadErrorWord) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (bcount_kb == 0U) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        // Backpressure guard when disk writer falls >50 chunks behind
        if (fDataQueue.Size() > 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        uint32_t read_kb = 0U;
        if (bcount_kb >= stream_align_kb) {
            const uint32_t capped = std::min(bcount_kb, kMaxChunkKb);
            read_kb = (capped / stream_align_kb) * stream_align_kb;
        } else if (bcount_kb >= burst_align_kb) {
            const auto since_last_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_read_time).count();
            if (since_last_ms >= 40) {
                read_kb = (bcount_kb / burst_align_kb) * burst_align_kb;
            }
        }

        if (read_kb == 0U) {
            std::this_thread::sleep_for(std::chrono::microseconds(250));
            continue;
        }

        pushReadKb(read_kb);
        last_read_time = std::chrono::steady_clock::now();
    }

    // Stop hardware triggers first, allow in-flight FPGA delay pipeline (CW+DLY+RL <= 163 us) to settle,
    // then drain any remaining complete events (< stream_align_kb) before flushing!
    fDevice->StopAcquisitionOnly();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const uint32_t tail_bcount_kb = fDevice->ReadBCOUNT();
    if (tail_bcount_kb != NoticeFadc500Spec::kUsbReadErrorWord && tail_bcount_kb >= event_align_kb) {
        uint32_t remaining_aligned_kb = (tail_bcount_kb / event_align_kb) * event_align_kb;
        while (remaining_aligned_kb > 0U && fDataQueue.Size() <= 60) {
            const uint32_t step_kb = std::min(remaining_aligned_kb, kMaxChunkKb);
            pushReadKb(step_kb);
            remaining_aligned_kb -= step_kb;
        }
    }

    fDevice->ClearAndFlushUSB();
    fProducerActive.store(false);
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

    size_t bytes_per_event = ExpectedEventBytes(fRunInfo ? fRunInfo->GetFadcBD(0) : nullptr);
    bool packet_size_verified = false;
    bool target_reached_logged = false;

    size_t total_written_bytes = 0;
    uint64_t current_events = 0;

    auto sys_start_time = std::chrono::system_clock::now();
    auto ui_timer = std::chrono::steady_clock::now();
    auto perf_start_time = ui_timer;

    uint64_t last_print_events = 0;
    size_t last_print_bytes = 0;

    while (fProducerActive.load() || fDataQueue.Size() > 0) {
        RawBuffer* popBuffer = nullptr;

        if (fDataQueue.WaitAndPopFor(popBuffer, std::chrono::milliseconds(50))) {
            if (popBuffer) {
                if (popBuffer->size > 0) {
                    // Self-verify packet size from the first valid 128-byte FPGA header (supports ZEROSUP data_length==32!)
                    if (!packet_size_verified && popBuffer->size >= 128) {
                        const auto hdr = NoticeFadc500Spec::DecodeHeader128(popBuffer->data);
                        if (hdr.valid_geometry && hdr.valid_channel_tags) {
                            bytes_per_event = static_cast<size_t>(hdr.data_length) * 4ULL;
                            packet_size_verified = true;
                        }
                    }

                    size_t bytes_to_write = popBuffer->size;
                    if (maxEvents > 0 && bytes_per_event > 0) {
                        const size_t max_total_bytes = static_cast<size_t>(maxEvents) * bytes_per_event;
                        if (total_written_bytes >= max_total_bytes) {
                            bytes_to_write = 0;
                        } else if (total_written_bytes + bytes_to_write > max_total_bytes) {
                            // Exact packet-aligned cutoff so maxEvents never overshoots or writes partial packets
                            bytes_to_write = max_total_bytes - total_written_bytes;
                        }
                    }

                    if (bytes_to_write > 0) {
                        const size_t written = std::fwrite(popBuffer->data, 1, bytes_to_write, fp);
                        total_written_bytes += written;
                        if (written != bytes_to_write) {
                            ELog::Print(ELog::ERROR,
                                "Disk write error on " + outFileName + " (wrote " +
                                std::to_string(written) + " / " + std::to_string(bytes_to_write) +
                                " bytes). Stopping DAQ safely.");
                            fIsRunning.store(false);
                        }
                    }

                    current_events = (bytes_per_event > 0) ? (total_written_bytes / bytes_per_event) : 0ULL;

                    if (maxEvents > 0 && current_events >= static_cast<uint64_t>(maxEvents)) {
                        if (!target_reached_logged) {
                            std::cout << "\n\n";
                            ELog::Print(ELog::INFO,
                                "Target reached! (" + std::to_string(current_events) + " events). Stopping DAQ...");
                            target_reached_logged = true;
                        }
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
            // Flush stdio buffer to OS page cache every 0.5 s so live online monitor tails with zero lag
            std::fflush(fp);

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

    if (bytes_per_event > 0 && (total_written_bytes % bytes_per_event) != 0) {
        ELog::Print(ELog::WARNING,
            Form("Output stream size (%zu B) has %zu trailing unaligned bytes (packet=%zu B).",
                 total_written_bytes, total_written_bytes % bytes_per_event, bytes_per_event));
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
