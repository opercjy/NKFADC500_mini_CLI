# NKFADC500_mini_CLI : NoticeDAQ Standalone Control

![C++](https://img.shields.io/badge/C++-17-blue?style=flat-square&logo=c%2B%2B)
![ROOT](https://img.shields.io/badge/Framework-CERN%20ROOT%206-005aaa?style=flat-square)
![CMake](https://img.shields.io/badge/Build-CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=flat-square&logo=linux&logoColor=black)
![Status](https://img.shields.io/badge/Status-Phase_3_Complete-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/License-Notice_Authorized-red?style=flat-square)

Notice Korea의 FADC500 Mini (500MS/s, 12-bit, 4-Channel) 보드를 제어하고, 초고속으로 바이너리 데이터를 수집하여 ROOT 프레임워크 기반으로 분석하기 위한 **고성능 하이브리드 C++ DAQ 아키텍처**입니다.

향후 GUI 연동 및 다양한 분석 모듈 추가가 용이하도록 객체지향적(OOP)이고 확장 가능한 구조로 설계되었으며, 극단적인 고속 트리거 환경에서도 시스템이 뻗지 않도록 \*\*무결성 방어 로직(Zero-Deadlock)\*\*이 적용되어 있습니다.

-----

## 🏛️ 1. 시스템 아키텍처 개요 (System Architecture)

본 시스템은 수집(Online)과 분석(Offline)의 병목을 완벽히 분리한 하이브리드 아키텍처를 가집니다.

  * **[Core 1] Frontend (`frontend_500_mini`) : 안정성 검증 완료 (Stable)**
      * 하드웨어 제어 및 USB 3.0(SuperSpeed) 통신 전담.
      * 연산 오버헤드를 없애기 위해 복잡한 파싱 없이 순수 바이너리(`.dat`) 파일로 하드디스크에 고속 덤프.
      * **[핵심 기능]**
          * **스마트 파서:** `settings.cfg`를 통해 하드웨어의 모든 레지스터(채널별 임계값, 딜레이, 트리거 모드 등)를 유연하게 제어.
          * **Fail-Fast & Auto-Recovery:** 하드웨어 미연결 시 즉각 종료, USB Flooding(과부하) 방어, FIFO Lock-up 자동 해제 로직 탑재.
          * **논블로킹 UI:** 스레드 병목 없이 0.5초 주기로 터미널에 실시간 수집 속도(MB/s) 및 트리거 레이트(Hz) 출력.
  * **[Core 2] Production (`production_500_mini`) : 구현 완료 (Stable)**
      * 수집된 `.dat` 바이너리 파일을 읽어 12-bit 인터리브 마스킹을 해제.
      * C++ ROOT 객체를 활용하여 고속으로 물리량(전하량, 피크 등)을 추출하고, 입자물리 정석 규격인 플랫 트리(Flat Tree) 구조의 `*.root` 파일로 변환.
      * FADC400 스타일의 `-w` 옵션을 지원하여 베이스라인이 차감된 무손실 파형 벡터를 TGraph 형태로 저장 가능.
  * **[Core 3] Visualization (`online_monitor`) : 구현 완료 (Stable)**
      * 수집 중인 바이너리 파형을 실시간으로 추적(`tail -f` 방식)하여 확인하는 **라이브 온라인 모니터링(Live Online Monitoring)** 기능.
      * 메모리 누수(Memory Leak) 방지 아키텍처가 적용되어 장기 가동 시에도 시스템 부하가 없으며, 4채널 독립 파형 및 실시간 전하량 스펙트럼(Log-Y)을 동시 제공.
  * **[Expansion] PyQt GUI Control Panel : 개발 예정 (Phase 5)**
      * CLI 기반의 설정 및 구동을 마우스 클릭으로 제어할 수 있는 종합 그래픽 유저 인터페이스.

-----

## 🚀 2. 필수 의존성 및 권한 설정 (Prerequisites)

### 2.1 패키지 요구사항

  * **CERN ROOT 6** (환경변수 `thisroot.sh` 로드 필요)
  * **CMake 3.16+**, **GCC 지원 C++17 컴파일러**
  * `libusb-1.0` 라이브러리 (RHEL/Fedora: `libusb1-devel`, Ubuntu: `libusb-1.0-0-dev`)

### 2.2 USB 장치 권한 등록 (udev rules)

일반 유저 권한으로 USB 3.0 통신을 수행하기 위해 장치 권한을 먼저 시스템에 등록해야 합니다. (최초 1회만 수행)

```bash
# 프로젝트 루트 폴더에서 실행
sudo cp rules/50-nkfadc500.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

-----

## 🛠️ 3. 설치 및 빌드 가이드 (Build & Installation)

### Phase 1: 제조사 로우레벨 라이브러리 컴파일 (⚠️ 반드시 `su` 권한 진행)

**[핵심 주의사항]** 리눅스 보안 정책상 `sudo`를 사용하면 사용자 환경 변수(ROOT 경로 등)가 초기화되어 ROOT 딕셔너리 컴파일 에러가 발생합니다. **반드시 `su` 명령어로 관리자 계정에 진입한 뒤, 환경 변수를 수동 로드하고 컴파일해야 합니다.**

```bash
# 1. 관리자(root) 계정으로 전환
su

# 2. 필수 환경 변수 스크립트 2개 로드 (본인 시스템 경로에 맞게 수정)
source /usr/local/notice/notice_env.sh
source /usr/local/bin/thisroot.sh   # (또는 본인의 ROOT thisroot.sh 경로)

# 3. Layer 1 ~ Layer 3 일괄 빌드 및 설치
cd /usr/local/notice/src/nkusb/nkusb
make clean; make install
cd ../nkusbroot
make clean; make install

cd ../../usb3com/usb3com
make clean; make install
cd ../usb3comroot
make clean; make install

cd ../../nkfadc500/nkfadc500
make clean; make install
cd ../nkfadc500root
make clean; make install

# 4. 설치 완료 후 반드시 관리자 계정에서 빠져나옵니다.
exit
```

### Phase 2: 메인 DAQ 프로그램 빌드 (일반 유저 진행)

제조사 라이브러리가 시스템에 안착되었다면, 일반 계정으로 돌아와 메인 애플리케이션을 빌드합니다.

```bash
# 프로젝트 최상위 폴더(NKFADC500_mini_CLI)에서 실행
mkdir -p build && cd build
cmake ..
make -j4
```

빌드가 성공하면 `bin/` 디렉토리에 실행 파일들이 생성됩니다.

-----

## 🏃 4. 시스템 구동 매뉴얼 (Usage)

### 4.1 하드웨어 설정 파일 (`config/settings.cfg`) 구성 및 이해

데이터 수집 전, `config/settings.cfg` 파일을 열어 하드웨어 파라미터를 물리적 환경에 맞게 조율합니다.
*(하나의 값을 적으면 4개 채널 전체에 일괄 적용되며, 4개의 값을 띄어쓰기로 적으면 Ch0 \~ Ch3에 각각 독립적으로 적용됩니다.)*

#### 🟢 [글로벌 및 트리거 설정]

  * `RUN_NUMBER`: 현재 수집 번호. 구동 시 `run_XXXX.cfg` 형태로 설정이 자동 백업됩니다.
  * `BOARD`: 장비에 할당된 VME/USB 고유 Serial ID (기본값: 1)
  * `SAMPLING_RATE`: 샘플링 주파수 (1: 500MS/s, 2: 250MS/s, 4: 125MS/s)
  * `RECORD_LEN`: 파형 저장 길이. 1 단위가 128ns를 의미하므로, `8` 입력 시 1μs(1024ns) 기록.
  * `PRESCALE`: 트리거 분주비 (1 \~ 65535). 데이터 레이트가 너무 높을 때 들어온 트리거를 N번에 한 번씩만 인정하여 DAQ 부하를 조절합니다. (일반 물리 실험 시 `1` 권장)
  * `TRIG_TLT`: 다중 채널 트리거 로직(Trigger Lookup Table). (예: `65534`(0xFFFE) = 4채널 중 하나라도 신호가 오면 모두 기록하는 OR 로직)
  * `TRIG_ENABLE`: 트리거를 발생시킬 권한을 부여할 채널의 비트마스크. (예: `15`(0xF) = 4개 채널 전체 트리거 허용, `9` = Ch0, Ch3만 허용)
  * `PTRIG_INT`: 페데스탈(Pedestal) 강제 트리거 발생 주기(ms). 물리 신호 유무와 무관하게 허공(베이스라인)을 찍습니다. (일반 물리 실험 시 반드시 `0`으로 비활성화, 캘리브레이션 시 `100` 등 사용)

#### 🔵 [채널별 아날로그 및 타이밍 설정]

  * `DACOFF`: ADC DC 베이스라인 오프셋 (0 \~ 4095). PMT와 같은 Negative(음의 방향) 펄스 관측 시, 파형이 아래로 충분히 뻗을 수 있도록 공간을 확보하기 위해 주로 높은 값(`3400` 이상)을 부여합니다.
  * `POL`: 입력 신호 극성. (0: Negative 하강 엣지, 1: Positive 상승 엣지)
  * `THR`: 펄스 트리거 문턱값 (Threshold). 설정된 베이스라인 노이즈 대역보다는 높게 설정해야 가짜 트리거(Noise Trigger)를 막을 수 있습니다.
  * `DLY`: 트리거 시점을 기준으로 파형 저장을 시작할 딜레이(ns). 펄스의 상승(Rising Edge) 이전의 페데스탈(Pedestal) 영역을 함께 수집하기 위해 적절한 지연값을 줍니다.
  * `CW`: Coincidence Window. 다중 채널 동시발생(Coincidence)을 판별할 때 허용할 시간 폭(ns)입니다.
  * `AMODE`: ADC 모드 및 디지털 필터링. FPGA 내부에서 파형을 어떻게 전처리할지 결정합니다.
      * `0` (Bypass): 필터를 끄고 날것(Raw)의 파형을 그대로 받습니다. PMT나 SiPM처럼 빠르고 날카로운 신호 관측 시 필수입니다.
      * `1` 이상: 이동 평균(Moving Average) 등 저/고주파 필터를 적용합니다. HPGe처럼 느린 신호의 노이즈를 깎아내어 정확한 전하량(Charge)을 구할 때 유리합니다.

### 4.2 데이터 수집 가동 (Frontend)

설정이 완료되었다면 아래 명령어로 초고속 덤프 수집기를 가동합니다.

```bash
# 프로젝트 루트 디렉토리 기준
./bin/frontend_500_mini -f config/settings.cfg -o data/run_0001.dat [옵션]
```

**[프론트엔드 실행 옵션]**

  * `-f <file>` : 사용할 설정 파일 경로 (기본값: `../config/settings.cfg`)
  * `-o <file>` : 저장될 바이너리 데이터 파일명 (기본값: `run_XXXX.dat`)
  * `-n <개수>` : 목표 이벤트 수. 해당 개수에 도달하면 자동으로 안전 종료됩니다.
  * `-t <초>` : 시간 제한. 지정된 초(Seconds)가 지나면 자동으로 안전 종료됩니다.

💡 **Graceful Shutdown:** 수집을 도중에 종료하려면 **`Ctrl + C`** 를 한 번만 입력하십시오. 진행 중이던 버퍼를 디스크에 모두 안전하게 내려쓴 뒤, Run Summary(요약 통계)를 출력하고 정상 종료됩니다.

### 4.3 오프라인 변환 (Production)

수집된 순수 바이너리(`.dat`) 데이터를 ROOT TTree 형태(`.root`)로 초고속 파싱합니다.
터미널에서 변환 진행률과 ETA(예상 남은 시간)가 실시간으로 표시됩니다.

```bash
# 1. 고속 물리량(Charge/Peak) 추출 모드 (저용량, 빠른 연산)
./bin/production_500_mini data/run_0001.dat

# 2. 무손실 전체 파형 보존 모드 (상세 분석용)
./bin/production_500_mini data/run_0001.dat -w
```

  * 변환이 완료되면 원본 파일 이름에 `_prod`가 붙은 `run_0001_prod.root` 파일이 생성됩니다.
  * `-w` 옵션을 부여하면 `wTime_ChX`, `wDrop_ChX` 등 ROOT TGraph로 쉽게 그릴 수 있는 Vector Branch가 트리에 추가로 기록됩니다.

### 4.4 실시간 라이브 파형 뷰어 (Online Monitor)

프론트엔드 수집기가 돌아가고 있는 와중에 **별도의 터미널 창**을 열어 실시간으로 파형과 전하량 스펙트럼(Charge Spectrum)을 모니터링할 수 있습니다. `tail -f` 방식처럼 동작하므로 파일의 성장을 실시간으로 추적합니다.

```bash
# [터미널 1] 프론트엔드 수집 가동
./bin/frontend_500_mini -f config/settings.cfg -o data/live_stream.dat

# [터미널 2] 뷰어 가동 (같은 파일을 지정)
./bin/online_monitor data/live_stream.dat
```

  * **[상단 행] 파형 모니터:** 실시간 베이스라인(페데스탈) 점선이 함께 표기되어 신호의 흔들림을 파악할 수 있습니다.
  * **[하단 행] 스펙트럼 모니터:** Log-Y 스케일이 적용된 전하량 누적 분포를 통해 물리적인 입자 스펙트럼을 즉시 검증할 수 있습니다.
  * 뷰어를 종료하려면 터미널에서 `Ctrl+C`를 누르거나 ROOT 캔버스의 `X` 닫기 버튼을 누르면 메모리 누수 없이 안전하게 종료됩니다.

-----

## 🗺️ 5. 개발 로드맵 (Roadmap)

  - [x] **Phase 1:** 객체지향(OOP) 기반 코어 아키텍처 및 CMake 빌드 시스템 통합
  - [x] **Phase 2:** 초고속 무결성 바이너리 수집기(Frontend) 구현 완료 (Fail-Safe, Real-time Dashboard 탑재)
  - [x] **Phase 3:** 순수 플랫 트리(Pure Flat Tree) 구조의 오프라인 파서(Production) 구현 완료
  - [x] **Phase 4:** 메모리 릭(Leak) 방지 4x2 하이브리드 실시간 파형 뷰어(Online Monitoring) 구현 완료
  - [ ] **Phase 5:** 사용자 친화적 PyQt GUI 통합 애플리케이션 개발