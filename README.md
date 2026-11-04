# NKFADC500_mini_CLI : Notice NKFADC500-4 Standalone C++17 DAQ & PySide6 Control Suite

[![Platform](https://img.shields.io/badge/Platform-Linux%20(x86__64)-0078D4?logo=linux&logoColor=white)](https://github.com/opercjy/NKFADC500_mini_CLI)
[![Hardware](https://img.shields.io/badge/Hardware-Notice%20NKFADC500--4%20(500%20MS%2Fs%2C%2012--bit)-E65100)](http://www.noticekorea.com/)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17%20%7C%20CERN%20ROOT%206-00599C?logo=cplusplus&logoColor=white)](https://root.cern/)
[![GUI Framework](https://img.shields.io/badge/GUI-PySide6%20(Qt6)%20%2B%20PyQtGraph-41CD52?logo=qt&logoColor=white)](https://wiki.qt.io/Qt_for_Python)
[![Verification](https://img.shields.io/badge/E2E%20Audit-4--Stage%20Static%2FDynamic%20Verified%20(Argon)-8A2BE2)](#8-인공지능-활용-선언-declaration-of-ai-use-in-staticdynamic-verification)

> **[Notice Hardware Maintenance & Development Declaration]**  
> 본 프레임워크는 **Notice Korea**에서 제공한 로우 레벨 API(`libnkusb`, `libusb3com`, `libNoticeNKFADC500`) 및 하드웨어 래퍼를 기반으로 작성되었습니다. 고에너지 물리(HEP)·핵물리·방사선 검출기 실험의 장기 운용과 고율 트리거(High-Rate Trigger & Noise) 환경에서도 장비가 무결하게 동작할 수 있도록, 연구자 주도하에 C++17 코어 아키텍처 재설계, 64-bit FPGA 패킷 디코딩 정밀화, 및 지속적인 유지보수(Maintenance)가 이루어지고 있는 독립 오픈소스 프로젝트입니다.

---

## 1. 프로젝트 개요 및 하드웨어 제원 (Overview & Hardware Specifications)

**`NKFADC500_mini_CLI`**는 Notice Korea의 **NKFADC500 Mini (4채널, 500 MS/s, 12-bit Flash ADC)** 디지타이저 보드를 제어하고, 초고속 USB 3.0 (Cypress FX3 SuperSpeed) 버스트 스트리밍으로 원시 바이너리(`.dat`) 데이터를 수집·감시한 뒤 **CERN ROOT 6** 플랫 트리(`_prod.root`)로 변환·분석하는 **하이브리드 C++17 / Python(PySide6) 통합 데이터 획득(DAQ) 프레임워크**입니다.

고속 트리거 환경에서도 USB 3.0 FIFO 및 보드 내장 64 MB DRAM 버퍼가 데드락에 빠지지 않도록 **비동기 링버퍼(Zero-Deadlock Producer–Consumer)** 및 **종료 후 잔여 패킷 완전 회수(100% Post-Stop Tail Drain)** 로직이 탑재되어 있으며, **NumPy 벡터화 다이렉트 바이너리 파서**를 통해 수집 중 디스크 I/O 간섭 없이 $50\text{ ms}$ ($20\text{ fps}$) 주사율의 실시간 파형·스펙트럼 감시를 수행합니다.

### NKFADC500-4 (FADC500 Mini) 핵심 하드웨어 및 물리 환산 규격

| 항목 (Parameter) | 하드웨어 규격 및 물리 환산치 (Specification & Calibration) | 비고 (Notes) |
| :--- | :--- | :--- |
| **입력 채널 및 임피던스** | 4 Channels (MCX/SMA), $R_{\text{term}} = 50.0\ \Omega$ DC 커플링 | 채널별 독립 DAC 오프셋(`DACOFF`) 조정 |
| **샘플링 속도 및 시간 분해능** | $f_s = 500\text{ MS/s}$ ($\Delta t_{\text{samp}} = 2.0\text{ ns/sample}$) | 4-ADC 인터리빙 (ADC 클럭 정렬 내장) |
| **수직 해상도 및 동적 범위** | $12\text{-bit}$ ($0\text{--}4095\text{ ADC}$), 공칭 $V_{\text{pp}} = 2.5\text{ V}_{\text{pp}}$ ($\pm 1.25\text{ V}$) | $\Delta V_{\text{LSB}} = \dfrac{2500\text{ mV}}{4096} = \mathbf{0.61035\text{ mV/ADC}}$ |
| **단위 전하 환산 계수 ($q_{\text{LSB}}$)** | $q_{\text{LSB}} = \dfrac{\Delta V_{\text{LSB}}\,\Delta t_{\text{samp}}}{50\ \Omega} = \mathbf{24.4140625\text{ fC/(ADC}\cdot\text{sample)}}$ | $0.0244140625\text{ pC/(ADC}\cdot\text{sample)}$ |
| **레코드 길이 (`RL`) 및 샘플 수** | $\texttt{RL} \in \{1, 2, 4, 8, 16, 32, 64, 128, 256\}$ ($\text{패킷 바이트} = \texttt{RL}\times 512\text{ B}$) | $\texttt{RL}=4 \implies N_s = 496\text{ samples}$ ($992\text{ ns}$ 창) |
| **타이밍 클럭 분해능** | Fine Timestamp $1\text{ ns}$ / Coarse Timestamp $8\text{ ns}$ ($125\text{ MHz}$) | 48-bit Local FPGA 시간 + 64-bit TCB 시간 |
| **온보드 버퍼 및 인터페이스** | 64 MB SDRAM (`BCOUNT` 1 KB 블록 단위) / USB 3.0 SuperSpeed | `128 KB` / `16 KB` FX3 Slave FIFO 정렬 읽기 |

---

## 2. 하이브리드 시스템 아키텍처 (System Architecture)

본 시스템은 **수집(Online Frontend)**, **실시간 감시(Direct Monitoring)**, **오프라인 변환(ROOT Production)**, **통합 제어(PySide6 Master GUI)**의 4대 계층을 분리하여 I/O 병목과 프로세스 간 간섭을 원천 차단합니다.

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                   [Master GUI] PySide6 Control Panel (fadc500_gui)            │
│   • DaqTab (Threshold Auto-Scan / Multi-Run)   • ProductionTab (Batch ROOT)   │
│   • OnlineMonitorTab (20 fps NumPy Vector)     • RunCatalogTab (SQLite3 DB)   │
└───────────────┬───────────────────────────────────┬───────────────────────────┘
                │ QProcess (CLI Isolation)          │ Direct Non-Blocking File Tail
                ▼                                   ▼
┌───────────────────────────────────────┐   ┌───────────────────────────────────┐
│ [Core 1] frontend_nkfadc500 (C++17)   │   │ [Core 3] Direct Binary Monitor    │
│ • NoticeFadc500Spec.hh 레지스터 검증   │   │ • 4채널 인터리빙 헤더(0,4,8,12 B) │
│ • 32-bit BCOUNT & FX3 Burst 정렬 읽기  ├──►│   NumPy 비트시프트 즉시 해독       │
│ • Producer-Consumer 링버퍼 (.dat 덤프) │   │ • 파형 + 파고/전하 스펙트럼 렌더링│
└───────────────┬───────────────────────┘   └───────────────────────────────────┘
                │ Raw Binary (.dat) + Companion Config (.cfg)
                ▼
┌───────────────────────────────────────────────────────────────────────────────┐
│ [Core 2] production_nkfadc_500 (C++17 + CERN ROOT 6)                          │
│ • USB3 0xFF 버스트 패딩 스킵 및 패킷 헤더 무결성 검증                         │
│ • 64-bit TCB & 48-bit Local TriggerTime 언래핑 (2.14초 32-bit 오버플로 제거)  │
│ • 80-sample 프리트리거 베이스라인 차감, 파고/전하 적분 및 PSD(Qtail/Qtotal)   │
│ • Flat TTree ("PROD") 및 Pmt 고정 배열 객체 고속 직렬화 (~280+ MB/s)          │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 계층별 핵심 엔지니어링 특징

1. **[Core 1] 초고속 무결성 수집 엔진 (`frontend_nkfadc500`)**
   - **단일 하드웨어 규격 헤더 ([`NoticeFadc500Spec.hh`](./core/include/NoticeFadc500Spec.hh))**: 벤더 C SDK(`NoticeNKFADC500.c`, `usb3com.c`)와 전수 대조하여 모든 레지스터의 유효 범위(`CW`, `RL`, `DLY`, `THR`, `DACOFF`, `TLT`, `TMODE`, `DSR`)와 5-bit coarse 딜레이 한계($\texttt{CW} + \texttt{DLY} \le 31{,}992\text{ ns}$)를 사전 검증합니다.
   - **32-bit `BCOUNT` & FX3 Burst 정렬**: 대용량 버퍼 적재 시 `65,536 KB (0x10000)` 16-bit 마스킹 랩어라운드를 방지하고, `128 KB`/`16 KB` FX3 Slave FIFO 버스트 정렬 스트리밍 및 수집 종료 후 DRAM 잔여 완결 패킷 회수(Post-Stop Tail Drain)를 보장합니다.
   - **안전한 ADC/DRAM 정렬 및 CLI 오버라이드**: `ADCALIGN_500` 수행 시 `count == 0` 0-나눗셈(`SIGFPE`) 방지, `DRAMON` 무한 루프 타임아웃 방어, 페데스탈 자동 판독(`NKFADC500read_PED`), 런별 설정 파일(`.cfg`) 자동 백업, 및 `--thr <ADC>` 동적 문턱값 오버라이드를 지원합니다.

2. **[Core 2] 64-bit FPGA 디코딩 및 CERN ROOT 변환 엔진 (`production_nkfadc_500`)**
   - **정밀 32-Byte 인터리브 헤더 해독**: 4채널 4바이트 스트라이드(`ch * 4`)로 인터리빙된 FPGA 패킷 헤더에서 `DataLength`, `RunNumber`, `TriggerType`, `TriggerDestination`, 64-bit `TriggerTime`, `TriggerNumber`, `LocalTriggerNumber` (`17..20*4`), `TriggerPattern` (`21..24*4`), 48-bit `LocalTriggerTime`을 오차 없이 분리합니다.
   - **타임스탬프 32-bit 오버플로 완전 해결**: 기존 $1\text{ ns}$ 클럭을 32-bit 부호 있는 정수에 담아 $2.147\text{ s}$마다 음수로 뒤집히던 문제를 64-bit `ULong64_t` 및 하드웨어 카운터 언래핑(Unwrapping)으로 근본 해결했습니다.
   - **파형 구별(PSD) 물리 변수 내장**: 전체 창 정류 적분(`Charge_Ch*`)과 함께 피크 기준 국소 창($[-10, 120]\text{ ns}$) 및 지연 꼬리 창($[16, 120]\text{ ns}$) 적분(`Qtotal_Ch*`, `Qtail_Ch*`, `PsdRatio_Ch*`)을 즉시 산출하여 `PROD` 트리에 기록합니다.

3. **[Core 3 & Master GUI] PySide6 마스터 콘솔 (`fadc500_gui`)**
   - **50배 가속된 NumPy 실시간 모니터 ([`OnlineMonitorTab.py`](./gui/widgets/OnlineMonitorTab.py))**: 파이썬 루프 없이 NumPy 벡터 비트 연산과 `deque` 링버퍼로 4채널 파형 및 파고·전하 히스토그램을 $20\text{ fps}$로 동시 렌더링합니다.
   - **통합 경로 관리 ([`Paths.py`](./gui/core/Paths.py)) 및 SQLite3 런 카탈로그 ([`DatabaseManager.py`](./gui/core/DatabaseManager.py))**: 실행 작업 디렉터리에 구애받지 않는 단일 경로 해석기로 `data/run_catalog.db`에 수집·변환 이력, 처리 속도(`MB/s`), 이벤트 수, 연구자 코멘트를 영구 기록합니다.

---

## 3. 디렉토리 및 소스 코드 구조 (Directory Structure)

```text
NKFADC500_mini_CLI/
├── app/                          # [C++17 Applications] 수집·변환·모니터 실행 바이너리 소스
│   ├── frontend_main.cpp         # 초고속 USB3 바이너리 수집기 (frontend_nkfadc500)
│   ├── production_main.cpp       # 64-bit FPGA 디코더 및 ROOT 변환기 (production_nkfadc_500)
│   └── online_monitor.cpp        # CLI/백엔드 실시간 바이너리 스트림 모니터
├── core/                         # [C++17 DAQ Core] 하드웨어 래퍼, 스펙 검증, 비동기 버퍼 풀
│   ├── include/
│   │   ├── NoticeFadc500Spec.hh  # NKFADC500-4 하드웨어 상수·패킷 규격·레지스터 검증 헤더
│   │   ├── Fadc500Device.hh      # Notice USB3 SDK C++ 래퍼 클래스
│   │   ├── BinaryDaqManager.hh   # Producer-Consumer 멀티스레드 수집 매니저
│   │   ├── RawBufferPool.hh      # 조건변수(CV) 기반 Zero-Allocation 메모리 풀
│   │   └── ConfigParser.hh       # 10진수/16진수(0xFFFE) 및 범위 검증 설정 파서
│   └── src/                      # 코어 엔진 구현부
├── objects/                      # [CERN ROOT 6 Data Model] ROOT TTree 직렬화 객체 (libFADC500Objects.so)
│   ├── include/                  # FadcBD, Pmt, RawChannel, RunInfo 헤더 및 LinkDef.h
│   └── src/                      # 고정 배열 파형 주입 및 ROOT Dictionary 구현부
├── gui/                          # [PySide6 Master GUI] Qt6 기반 통합 제어 대시보드
│   ├── main.py                   # GUI 엔트리 포인트 (bin/fadc500_gui)
│   ├── core/                     # Paths, DatabaseManager(SQLite3), ProcessManager, ConfigManager
│   ├── windows/                  # MainWindow 오케스트레이터
│   └── widgets/                  # DaqTab, OnlineMonitorTab, ProductionTab, RunCatalogTab, HvTab
├── notice_nkfadc500_4/           # [Bundled Vendor SDK] Notice Korea C 로우레벨 드라이버 소스
├── config/                       # 하드웨어 구동 환경설정 템플릿 (settings.cfg)
├── rules/                        # Linux udev USB3 장치 권한 자동 설정 스크립트
├── offline_anal_500.cpp          # [ROOT Macro] 4-패널 파형 밀도·파고·전하·PSD 종합 분석기
├── offline_anal_edu.cpp          # [ROOT Macro] 교육·연구용 Δt 애프터펄스 컷 및 물리량 분석기
├── offline_spe.cpp               # [ROOT Macro] MINUIT2 다중 가우시안 단일 광전자(SPE) 절대이득 교정기
├── offline_compton_edge.cpp      # [ROOT Macro] 액체섬광체 감마선 컴프턴 엣지(ERFC) 정밀 피팅기
└── CMakeLists.txt                # 최상위 CMake 통합 빌드 (Notice SDK 자동 컴파일 및 RPATH 설정)
```

---

## 4. 설치 및 빌드 가이드 (Build & Installation)

### Step 1: 필수 의존성 패키지 준비

- **C++ 빌드 환경**: Linux (`x86_64`), GCC (`C++17` 이상), CMake `3.16+`, `libusb-1.0-devel`, **CERN ROOT 6.x** (`Minuit2` 활성화 권장)
- **Python GUI 환경**: Python `3.8+`

```bash
pip3 install PySide6 pyqtgraph numpy
```

### Step 2: Notice 로우레벨 SDK 및 메인 프로젝트 원클릭 통합 빌드

최신 `CMakeLists.txt`는 시스템에 `/usr/local/notice` (`$NKHOME`)가 설치되어 있지 않더라도, 저장소 내 번들된 `notice_nkfadc500_4/notice/src/` 소스(`libnkusb.so`, `libusb3com.so`, `libNoticeNKFADC500.so`)를 자동으로 컴파일하여 `./lib`에 배치하고 상대 `RPATH` (`$ORIGIN/../lib`)로 링킹합니다. 별도의 `root`(`su`) 권한 없이 일반 사용자 계정에서 즉시 빌드할 수 있습니다.

```bash
# 1. CERN ROOT 환경 로드 후 빌드 수행
source /usr/local/bin/thisroot.sh
mkdir -p build && cd build
cmake ..
make -j$(nproc)
cd ..
```

> **[선택 사항] 시스템 전역 `/usr/local/notice` 수동 설치가 필요한 경우**:
> ```bash
> su
> mkdir -p /usr/local/include /usr/local/lib
> source /usr/local/notice/notice_env.sh && source /usr/local/bin/thisroot.sh
> cd /usr/local/notice/src/nkusb/nkusb && make clean && make install
> cd ../../usb3com/usb3com && make clean && make install
> cd ../../nkfadc500/nkfadc500 && make clean && make install
> exit
> ```

### Step 3: GUI 마스터 콘솔 및 CLI 실행 방법

```bash
# ===================================================================
# [방법 A] PySide6 마스터 GUI 콘솔 실행 (권장)
# ===================================================================
./bin/fadc500_gui

# ===================================================================
# [방법 B] 터미널 CLI 단독 실행 (배치 스크립트 및 원격 서버 운용)
# ===================================================================
# 1) 프론트엔드 고속 바이너리 수집 (-n: 목표 이벤트 수, -t: 수집 시간[초], --thr: 전 채널 문턱값 오버라이드)
./bin/frontend_nkfadc500 -f config/settings.cfg -o data/run_0001.dat -n 100000 --thr 30

# 2) 수집된 바이너리(.dat)를 CERN ROOT 트리(_prod.root)로 고속 변환 (-w: 파형 배열 포함)
./bin/production_nkfadc_500 data/run_0001.dat -w
```

---

## 5. `PROD` ROOT 플랫 트리 스키마 및 물리량 정의 (ROOT Data Schema)

`production_nkfadc_500` 실행 시 생성되는 `*_prod.root` 파일 내부의 `PROD` 트리는 즉각적인 C++/Python(`uproot`) 분석을 위해 아래와 같은 표준화된 브랜치를 제공합니다 (`i = 0, 1, 2, 3`).

| 브랜치 이름 (Branch Name) | 데이터 타입 | 물리적 정의 및 단위 (Physical Meaning & Unit) |
| :--- | :---: | :--- |
| `EventNumber` | `Int_t` | 순차 이벤트 인덱스 ($0, 1, 2, \dots$) |
| `TriggerTime` / `LocalTriggerTime` | `ULong64_t` | 64-bit TCB 절대 시각 / 48-bit 보드 로컬 트리거 시각 [$\text{ns}$] (오버플로 보정 완료) |
| `TriggerType` / `TriggerPattern` | `Int_t` | 트리거 타입 및 4채널 보드 트리거 비트마스크 |
| `Baseline_Ch{i}` / `BaselineRMS_Ch{i}` | `Float_t` | 프리트리거 초기 $80\text{ samples}$ ($160\text{ ns}$) 평균 페데스탈 및 표준편차 [$\text{ADC}$] |
| `Amplitude_Ch{i}` | `Float_t` | 베이스라인 차감 후 최대 파고 진폭 [$\text{ADC}$] ($\times 0.61035 \to \text{mV}$) |
| `PeakTime_Ch{i}` | `Float_t` | 파형 내 최대 피크 도달 시각 [$\text{ns}$] ($\text{sample index} \times 2.0\text{ ns}$) |
| `Charge_Ch{i}` | `Float_t` | 전체 기록 창($N_s$ 샘플) 정류 적분 전하 합 $\sum \max(0, w_k)$ [$\text{ADC}\cdot\text{sample}$] |
| `Qtotal_Ch{i}` / `Qtail_Ch{i}` | `Float_t` | 피크 기준 국소 창($[-10, 120]\text{ ns}$) 및 지연 꼬리 창($[16, 120]\text{ ns}$) 적분 [$\text{ADC}\cdot\text{sample}$] |
| `PsdRatio_Ch{i}` | `Float_t` | 파형 구별(PSD) 지연 분율 $\text{PSD} = Q_{\text{tail}} / Q_{\text{total}} \in [0, 1]$ |
| `Waveform_Ch{i}` (`-w` 옵션) | `Float_t[N]` | 베이스라인 차감 및 극성 정렬된 파형 배열 [$\text{ADC}$] |

---

## 6. 교육 및 연구용 고급 오프라인 분석 파이프라인 (ROOT Offline Macros)

저장소 루트에 포함된 4종의 CERN ROOT C++ 매크로는 `2.5 Vpp` ($50\ \Omega$) 물리 환산 계수($0.61035\text{ mV/ADC}$, $0.024414\text{ pC/ADC}\cdot\text{sample}$)와 동적 `RecordLength`/`SamplingNs` 스케일링을 자동 적용합니다.

1. **`offline_anal_500.cpp` (4-패널 종합 진단 대시보드)**:
   - 파형 누적 밀도도(Waveform Persistence), 파고 진폭 분포(`mV`/`ADC`), 전하 적분 스펙트럼(`pC`/`ADC·sample`), 및 2D PSD(`PsdRatio` vs `Charge`) 산점도를 한 화면에 렌더링합니다.
2. **`offline_anal_edu.cpp` (물리 컷 & $\Delta t$ 애프터펄스 필터링 뷰어)**:
   - 진폭 문턱 컷(Amplitude Cut)을 통한 다크 노이즈 제거와 64-bit 클럭 기반 사건 간 시간차($\Delta t$) 컷을 통한 PMT 후속 펄스(Afterpulse) 제거 과정을 단계별로 시각화합니다. (구버전/신버전 ROOT 간 `libTreePlayer` 충돌 방지를 위해 순수 `TBranch`/`TLeaf` 엔진으로 구현되었습니다.)
3. **`offline_spe.cpp` (MINUIT2 다중 가우시안 단일 광전자 교정기)**:
   - 저광량 스펙트럼에서 **Pedestal + 1-PE + 2-PE 다중 가우시안(Multi-Gaussian)** 모델을 `MINUIT2`로 정밀 피팅하여 각 성분을 분해 렌더링하고 PMT의 절대 증배 이득(Absolute Gain $G$)을 도출합니다.
4. **`offline_compton_edge.cpp` (감마선 컴프턴 엣지 ERFC 피팅기)**:
   - $^{137}\text{Cs}$ ($E_{\text{CE}} = 477.3\text{ keV}$) 및 $^{60}\text{Co}$ ($963.4, 1118.1\text{ keV}$) 감마선 연속체의 고전하 어깨 영역을 역탐색하여 상보 오차함수(`ERFC`) 모델로 컴프턴 엣지 위치(`pC`)와 에너지 분해능을 추출합니다.

```bash
# 실행 예시 (CERN ROOT 6 환경)
root -l 'offline_anal_500.cpp("data/run_0001_prod.root", 0, 30.0)'
root -l 'offline_spe.cpp("data/run_0001_prod.root", 0)'
root -l 'offline_compton_edge.cpp("data/run_0001_prod.root", 0, 2000.0)'
root -l 'offline_anal_edu.cpp("data/run_0001_prod.root", 0, 30.0, 10.0)'
```

---

## 7. 정적/동적 코드 분석 및 4단계 E2E 검증 요약 (Static & Dynamic Verification)

본 코드베이스는 C++17 DAQ 코어, Notice 벤더 C SDK 인터페이스, CERN ROOT 6 직렬화, 및 PySide6 GUI 전 계층에 걸쳐 **정적 분석(Static Analysis)**과 **4단계 종단간 동적 검증(4-Stage End-to-End Dynamic Verification)**을 통과했습니다:

- **Stage 1 (C++17 & CMake 클린 빌드 감사)**: 번들된 Notice C SDK 자동 컴파일, `ROOT_GENERATE_DICTIONARY` 모듈명(`MODULE FADC500Objects`) `.rootmap`/`.pcm` 정합성 검증, `RawBuffer` Rule-of-5 메모리 안전성 및 스레드 안전 `ELog` (`localtime_r` + `std::mutex`) 검증 완료 (`0 Warnings, 0 Errors`).
- **Stage 2 (합성 FPGA 패킷 $\to$ `production_nkfadc_500` 비트 정확도 검증)**: 실제 NKFADC500-4 32-byte 인터리브 헤더, USB3 `0xFF` 버스트 패딩, 알려진 지수 감쇠 파형 및 48-bit/64-bit 클럭 롤오버 패킷을 주입하여 `LocalTriggerNumber`, `TriggerPattern`, `TriggerTime`, `Amplitude`, `Charge`, `PsdRatio` 복원 정확도 $100\%$ 확인.
- **Stage 3 (CERN ROOT 6 오프라인 매크로 배치 실행 검증)**: 생성된 `_prod.root` 트리를 대상으로 `offline_anal_500.cpp`, `offline_anal_edu.cpp`, `offline_spe.cpp`, `offline_compton_edge.cpp` 4종 매크로 무오류 배치 실행 검증 완료.
- **Stage 4 (PySide6 GUI 헤드리스 통합 및 NumPy 모니터 파서 검증)**: `Paths.py` 단일 DB 경로, `DaqTab` `--thr` 자동 스캔, `HvTab` 위젯 바인딩, `OnlineMonitorTab` NumPy 벡터 디코더, `ProductionTab` 정규식 및 프로세스 종료 클린업 무결성 확인.

---

## 8. 인공지능 활용 선언 (Declaration of AI Use in Static/Dynamic Verification)

본 프로젝트의 하드웨어 제어 아키텍처 설계, 물리 분석 알고리즘 구현, 및 실험 데이터 검증은 연구자(Author)의 직접적인 주도하에 수행되었습니다. 소프트웨어의 신뢰성과 코드 무결성을 극대화하기 위해, 연구자는 **Google Antigravity (`agy`)** 에이전틱 환경의 **Argon (`Argon`)** 모델을 아래의 **정적/동적 코드 분석 및 교차 검증 도구**로 활용하였습니다:

1. **정적 코드 분석 및 벤더 SDK 규격 대조 (Static Code Analysis & Hardware Spec Auditing)**:
   - **Argon** 모델을 활용하여 C++17 코어(`frontend_nkfadc500`, `production_nkfadc_500`), Notice Korea 로우레벨 C SDK(`NoticeNKFADC500.c`, `usb3com.c`), 및 PySide6 GUI 스크립트 전반의 메모리 수명주기(Rule-of-5), 멀티스레드 동기화, 레지스터 유효 경계(`NoticeFadc500Spec.hh`), 64-bit 타이밍 오버플로, 및 FPGA 헤더 바이트 오프셋을 전수 정적 분석하고 리팩터링하였습니다.
2. **종단간 동적 회귀 검증 및 물리 스케일 감사 (Dynamic E2E Regression & Physics Calibration Testing)**:
   - **Argon** 모델을 통해 합성 FPGA 바이너리 스트림 생성부터 ROOT `PROD` 트리 변환, 4종의 오프라인 물리 분석 매크로(`2.5 Vpp`, $50\ \Omega$ 전하·전압 교정), 및 PySide6 GUI 파서까지의 4단계 자동화 동적 테스트(Dynamic E2E Test Suite)를 수행하여 런타임 예외와 심볼 충돌을 검증·해결하였습니다.
3. **연구자 최종 검수 및 책임 (Author Oversight & Responsibility)**:
   - AI가 제안하거나 리팩터링한 모든 C++/Python 코드와 빌드 설정은 연구자가 실제 서버 및 검출기 데이터 환경에서 직접 실행·검수하였으며, 최종 소프트웨어 및 물리 분석 결과에 대한 모든 학술적·기술적 책임은 전적으로 연구자에게 있습니다.

---

## 9. 감사의 글 (Acknowledgements)

본 DAQ 시스템이 완성되기까지 보이지 않는 곳에서 헌신해 주신 많은 분들과 인프라에 깊은 감사를 표합니다.

**대한민국 국민 여러분께**  
당장의 가시적인 이익이 보이지 않는 기초 과학의 길을 묵묵히 걸을 수 있는 것은, 국민 여러분께서 땀 흘려 조성해 주신 소중한 연구 기금 덕분입니다. 이 시스템이 앞으로 포착해 낼 우주의 미세한 신호들은 모두 국민 여러분의 지지와 성원이 만들어낸 결실입니다. 보내주신 아낌없는 믿음에 깊이 감사드리며, 그 무거운 책임감과 자긍심을 안고 연구의 최전선에서 쉼 없이 탐구하겠습니다.

**Notice Korea 김상열 박사님께**  
뛰어난 하드웨어를 설계해 주신 Notice Korea 김상열 대표/박사님께 깊은 감사를 드립니다. 아낌없이 공유해 주신 로우 레벨 로직과 API 래퍼, 직접 빌드해 주신 FPGA 스크립트와 상세한 테스트 코드는 이 프로젝트를 완성하는 가장 든든한 기반이 되었습니다. 하드웨어의 깊은 곳까지 세밀하게 짚어주신 박사님의 헌신과 장인 정신에 진심 어린 경의를 표합니다.

---

## 10. GUI 마스터 패널 및 실측 분석 화면 (UI/UX & Experimental Screenshots)

아래 화면들은 실제 NKFADC500 Mini 장비와 검출기 테스트벤치에서 측정·수집된 파형, 스펙트럼, 및 마스터 제어 패널 구동 결과입니다.

<img width="1370" height="940" alt="image" src="https://github.com/user-attachments/assets/caf885ed-6a0e-49b9-b3d4-640814ba8622" />
<img width="1620" height="840" alt="image" src="https://github.com/user-attachments/assets/479fa003-bcf3-45d4-88e6-4eeb6a5af267" />
<img width="1370" height="940" alt="image" src="https://github.com/user-attachments/assets/68e789c8-bd36-41de-89f7-7e63d8b898ca" />
<img width="1370" height="940" alt="image" src="https://github.com/user-attachments/assets/c5858df4-c5cb-4b36-87c3-d2c69a1cf1da" />
<img width="1370" height="940" alt="image" src="https://github.com/user-attachments/assets/cca1b29c-21c0-4f47-a844-ef76cb35266c" />
<img width="1370" height="940" alt="image" src="https://github.com/user-attachments/assets/e56c3927-fdb1-4fc0-956c-825f6cdf4e41" />
<img width="1370" height="940" alt="image" src="https://github.com/user-attachments/assets/88afd1a2-55e9-4770-8f9b-251395f70e96" />
<img width="1370" height="940" alt="image" src="https://github.com/user-attachments/assets/1c4f2458-44b4-4d81-a955-bacb10501f02" />
<img width="1620" height="1040" alt="image" src="https://github.com/user-attachments/assets/2fd31349-56b7-4d9b-9a19-f5a6300b1819" />
<img width="1420" height="640" alt="image" src="https://github.com/user-attachments/assets/6302864c-0d4d-4d2d-9111-0757d465f2de" />
<img width="1420" height="940" alt="image" src="https://github.com/user-attachments/assets/a9b391dc-5b15-4d8b-9183-6acb0cf3da12" />
<img width="1020" height="740" alt="image" src="https://github.com/user-attachments/assets/b60cfbac-8913-41c8-bb8a-0bc9c070f654" />
