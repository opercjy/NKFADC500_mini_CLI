#include "BinaryDaqManager.hh"
#include "ELog.hh"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <cstring>

// -------------------------------------------------------------------------
// 생성자: ZMQ 퍼블리셔 초기화 및 CONFLATE(최신 1개만 유지) 옵션 적용
// -------------------------------------------------------------------------
BinaryDaqManager::BinaryDaqManager(RunInfo* runInfo) 
    : fRunInfo(runInfo), fDevice(nullptr), fIsRunning(false),
      fZmqContext(1), fZmqPublisher(fZmqContext, ZMQ_PUB)
{
    FadcBD* bdConfig = fRunInfo->GetFadcBD(0);
    if (!bdConfig) return;

    fDevice = new Fadc500Device(bdConfig->GetMID());
    fDevice->Initialize(bdConfig);

    for (int i = 0; i < 10; i++) {
        fFreeQueue.Push(new RawBuffer(4 * 1024 * 1024));
    }

    // 💡 [Phase 6] ZMQ 셋업: 큐에 쌓이지 않고 항상 최신 1개만 유지 (메모리 누수 완벽 차단)
    int conflate = 1;
    fZmqPublisher.setsockopt(ZMQ_CONFLATE, &conflate, sizeof(conflate));
    fZmqPublisher.bind("tcp://127.0.0.1:5555"); // GUI와 통신할 로컬 포트 개방

    fResidualCapacity = 1024 * 1024; // 1MB 여유 공간
    fResidualBuffer = new unsigned char[fResidualCapacity];
    fResidualSize = 0;
}

// -------------------------------------------------------------------------
// 소멸자
// -------------------------------------------------------------------------
BinaryDaqManager::~BinaryDaqManager() {
    Stop();
    if (fDevice) delete fDevice;
    delete[] fResidualBuffer;
}

// -------------------------------------------------------------------------
// Start: 멀티스레드 가동 및 콘솔 배너 출력
// -------------------------------------------------------------------------
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

// -------------------------------------------------------------------------
// Stop: 스레드 조인 및 안전 종료
// -------------------------------------------------------------------------
void BinaryDaqManager::Stop() {
    fIsRunning = false; 
    fDataQueue.Stop();
    fFreeQueue.Stop();
    if (fProducerThread.joinable()) fProducerThread.join();
    if (fConsumerThread.joinable()) fConsumerThread.join();
}

// -------------------------------------------------------------------------
// Producer: USB 고속 판독 및 Error -4 (단선) 방어
// -------------------------------------------------------------------------
void BinaryDaqManager::ProducerWorker(int maxTime) {
    fDevice->StartDAQ();
    auto start_time = std::chrono::steady_clock::now();

    while (fIsRunning) {
        if (maxTime > 0) {
            auto current_time = std::chrono::steady_clock::now();
            int elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
            if (elapsed >= maxTime) {
                std::cout << "\n"; 
                ELog::Print(ELog::INFO, Form("Time limit reached (%d sec). Stopping DAQ...", maxTime));
                fIsRunning = false;
                break;
            }
        }

        unsigned int raw_bcount = fDevice->ReadBCOUNT();

        if (raw_bcount == 0xFFFFFFFF) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 플래그 마스킹 후 순수 데이터 용량 추출
        unsigned int bcount_kb = raw_bcount & 0x0000FFFF;

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

        int stat = fDevice->ReadData(total_bytes_to_read, buffer->data);
        if (stat == -4) { 
            // 💡 [Phase 6] USB 단선 시 무한루프 방지 및 안전 종료
            fIsRunning = false; 
            break; 
        }

        buffer->size = total_bytes_to_read;
        fDataQueue.Push(buffer);
        
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    
    fDevice->StopDAQ();
    fDataQueue.Stop(); 
}

// -------------------------------------------------------------------------
// Consumer: 디스크 덤프 및 ZMQ Flat Binary 브로드캐스팅 (스티칭 적용)
// -------------------------------------------------------------------------
void BinaryDaqManager::ConsumerWorker(const std::string& outFileName, int maxEvents) {
    FILE* fp = fopen(outFileName.c_str(), "wb");
    if (!fp) {
        std::cout << "\n";
        ELog::Print(ELog::FATAL, Form("Cannot open file %s", outFileName.c_str()));
        fIsRunning = false;
        return;
    }

    size_t total_written_bytes = 0;
    int current_events = 0;
    
    // FADC500 Mini 1개 이벤트 바이트 계산 (16 Byte Header + 4Ch Data)
    // 1 RL = 64 points. 4채널 * 2바이트 = 1 RL당 512바이트 페이로드
    uint32_t record_len = fRunInfo->GetFadcBD(0)->GetRL();
    uint32_t payload_size = record_len * 512; 
    uint32_t event_byte_size = 16 + payload_size;

    auto sys_start_time = std::chrono::system_clock::now();
    auto ui_timer = std::chrono::steady_clock::now();
    auto perf_start_time = std::chrono::steady_clock::now(); 
    auto zmq_timer = std::chrono::steady_clock::now(); 
    
    int last_print_events = 0;
    size_t last_print_bytes = 0;

    while (fIsRunning || fDataQueue.Size() > 0) {
        RawBuffer* popBuffer = nullptr;
        
        if (fDataQueue.TryPop(popBuffer)) {
            if (popBuffer && popBuffer->size > 0) {
                
                // 1. 하드디스크에 바이너리 원본 덤프 (무손실 보장)
                size_t written = fwrite(popBuffer->data, 1, popBuffer->size, fp);
                total_written_bytes += written;
                
                // 이벤트 수 정확한 계산
                current_events = total_written_bytes / event_byte_size;

                // -----------------------------------------------------------------
                // 💡 2. [Phase 6 최종] Residual Buffer 스티칭 및 ZMQ 순수 파형 추출
                // -----------------------------------------------------------------
                uint32_t total_parse_size = fResidualSize + popBuffer->size;
                unsigned char* parse_buffer = new unsigned char[total_parse_size];

                // 이전 턴의 자투리(Residual) 결합
                if (fResidualSize > 0) {
                    std::memcpy(parse_buffer, fResidualBuffer, fResidualSize);
                }
                std::memcpy(parse_buffer + fResidualSize, popBuffer->data, popBuffer->size);

                uint32_t offset = 0;
                bool sent_this_chunk = false;

                // 이벤트 파싱 루프 (찢어진 이벤트는 루프를 통과하지 못함)
                while (offset + event_byte_size <= total_parse_size) {
                    
                    // 파이썬 GUI가 뻗지 않도록 0.1초당 최신 1개 이벤트만 추출해서 전송
                    if (!sent_this_chunk) {
                        auto current_time = std::chrono::steady_clock::now();
                        if (std::chrono::duration<double>(current_time - zmq_timer).count() >= 0.1) {
                            
                            // GUI로 쏠 Flat Binary C-Struct 생성 [EvtNum(4B) + ChID(4B) + nPoints(4B) + 파형 배열]
                            uint32_t nPoints = record_len * 64 * 4; // 4채널 전체 포인트 수
                            size_t msg_size = 12 + payload_size;
                            zmq::message_t msg(msg_size);
                            
                            uint32_t* header_ptr = static_cast<uint32_t*>(msg.data());
                            header_ptr[0] = current_events; // Event Num
                            header_ptr[1] = 4;              // Num Channels
                            header_ptr[2] = nPoints;        // Data Length
                            
                            // FPGA 헤더(16B)를 건너뛰고 순수 파형 ADC 데이터만 복사 (Zero-Copy)
                            std::memcpy(header_ptr + 3, parse_buffer + offset + 16, payload_size);
                            
                            fZmqPublisher.send(msg, zmq::send_flags::dontwait);
                            zmq_timer = current_time;
                            sent_this_chunk = true;
                        }
                    }
                    offset += event_byte_size; 
                }

                // 3. 파싱하고 남은 찢어진 자투리(Residual) 데이터를 다음 턴을 위해 안전하게 보관
                fResidualSize = total_parse_size - offset;
                if (fResidualSize > 0) {
                    std::memcpy(fResidualBuffer, parse_buffer + offset, fResidualSize);
                }
                delete[] parse_buffer;
                // -----------------------------------------------------------------

                if (maxEvents > 0 && current_events >= maxEvents) {
                    std::cout << "\n\n"; 
                    ELog::Print(ELog::INFO, Form("Target reached! (Est. %d events). Stopping DAQ...", current_events));
                    fIsRunning = false; 
                }
                
                popBuffer->size = 0;
                fFreeQueue.Push(popBuffer); 
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 3. UI 및 콘솔 출력 업데이트 (0.5초 주기)
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
    std::cout << Form("   Total Events  : %d\n", current_events);
    std::cout << Form("   Total Written : %.2f MB\n", total_written_bytes / 1048576.0);
    std::cout << "   Avg Trig Rate : " << std::fixed << std::setprecision(2) << avg_rate << " Hz\n";
    std::cout << "\033[1;36m========================================================\033[0m\n";
}
