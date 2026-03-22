# NKFADC500\_mini\_CLI : NoticeDAQ Standalone Control

![C++](https://img.shields.io/badge/C++-17-blue?style=flat-square&logo=c%2B%2B)
![ROOT](https://img.shields.io/badge/Framework-CERN%20ROOT%206-005aaa?style=flat-square)
![CMake](https://img.shields.io/badge/Build-CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=flat-square&logo=linux&logoColor=black)
![Status](https://img.shields.io/badge/Status-Phase_3_Complete-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/License-Notice_Authorized-red?style=flat-square)

[Notice Hardware Maintenance & Development Declaration]
본 프레임워크는 Notice Korea에서 제공한 로우 레벨 API 및 하드웨어 래퍼(Wrapper)를 기반으로 작성되었습니다. 고에너지 물리 실험의 장기적인 운용과 극단적인 노이즈 환경에서도 장비가 안정적으로 동작할 수 있도록, 연구자 주도하에 코어 아키텍처 재설계 및 지속적인 유지보수(Maintenance)가 이루어지고 있는 독립 프로젝트입니다.

Notice Korea의 FADC500 Mini (500MS/s, 12-bit, 4-Channel) 보드를 제어하고, 초고속으로 바이너리 데이터를 수집하여 ROOT 프레임워크 기반으로 분석하기 위한 **고성능 하이브리드 C++ / Python 통합 DAQ 아키텍처**입니다.

극단적인 고속 트리거(High Noise & Trigger Rate) 환경에서도 USB 버퍼가 뻗지 않도록 \*\*C++ 백엔드의 무결성 방어 로직(Zero-Deadlock, 100% Drain)\*\*이 적용되어 있으며, 이를 마우스 클릭만으로 우아하게 제어할 수 있는 **PyQt5 기반의 마스터 컨트롤 GUI 패널**이 완벽하게 통합되어 있습니다.


## 🏛️ 1. 시스템 아키텍처 개요 (System Architecture)

본 시스템은 수집(Online)과 분석(Offline)의 병목을 완벽히 분리한 하이브리드 아키텍처를 가집니다.

  * **[Core 1] Frontend (`frontend_500_mini`) : 안정성 검증 완료 (Stable)**
      * 하드웨어 제어 및 USB 3.0(SuperSpeed) 통신 전담.
      * 연산 오버헤드를 없애기 위해 복잡한 파싱 없이 순수 바이너리(`.dat`) 파일로 하드디스크에 고속 덤프.
      * **[핵심 기능]**
          * **2MB Deadlock 완전 파괴:** 하드웨어 FIFO 버퍼 잔여물을 100% Drain(추출)하여 극한의 노이즈 상황에서도 영원히 뻗지 않는 무한대 수집 지원.
          * **Fail-Fast & Auto-Recovery:** 하드웨어 미연결 시 즉각 종료, USB Flooding(과부하) 방어, 좀비 락(Error -7/-9) 자동 세척(Flush) 로직 탑재.
          * **멀티스레드 큐잉(Producer-Consumer):** USB 패킷 수집 스레드와 디스크 I/O 스레드를 완벽히 분리하여 병목(Backpressure) 현상 극복.
  * **[Core 2] Production (`production_500_mini`) : 구현 완료 (Stable)**
      * 수집된 `.dat` 바이너리 파일을 읽어 12-bit 인터리브 마스킹을 해제.
      * C++ ROOT 객체를 활용하여 고속으로 물리량(전하량, 피크 등)을 추출하고, 입자물리 정석 규격인 플랫 트리(Flat Tree) 구조의 `*.root` 파일로 변환.
      * `-w` 옵션을 지원하여 베이스라인이 차감된 무손실 파형 벡터를 TGraph 형태로 저장 가능.
  * **[Core 3] Visualization (`online_monitor`) : 구현 완료 (Stable)**
      * 수집 중인 바이너리 파형을 실시간으로 추적(`tail -f` 방식)하여 확인하는 **라이브 온라인 모니터링(Live Online Monitoring)** 기능.
      * 메모리 누수(Memory Leak) 및 파일 포인터 에러(Underflow) 방지 아키텍처가 적용되어 장기 가동 시에도 시스템 부하가 없음.
  * **[Master GUI] PyQt5 Control Panel (`fadc500_gui`) : 구현 완료 (Stable)**
      * CLI 기반의 백엔드 엔진들을 서브 프로세스(QProcess)로 완벽히 격리하여 제어하는 종합 그래픽 유저 인터페이스.
      * 시스템 다운 시 강력한 `SIGKILL` 하드웨어 강제 처형 및 복구 기능 탑재.

## 🖥️ 2. 그래픽 인터페이스 및 사용자 경험

본 프로젝트의 Phase 5로 완성된 통합 GUI 패널은 복잡한 설정 파일과 터미널 명령어 입력의 고통을 덜어주고, 직관적인 대시보드를 통해 실험의 가시성을 극대화합니다.

*(📸 실제 구동 화면 마지막 하단에 배치 하였습니다)*

### 🚀 2.1 메인 대시보드 및 실시간 수집 뷰 (DAQ Control & Live Dashboard)

  * **원클릭 수집 제어:** 수집 파일명(Run Number), 정지 조건(이벤트/시간), 자동 연속 수집(Long-term)을 손쉽게 세팅할 수 있습니다.
  * **Live Status Dashboard:** 현재 수집 중인 파일의 크기, 초당 처리 속도(MB/s), 트리거 레이트(Hz)를 즉각적으로 보여줍니다.
  * **스마트 Config 요약기:** 현재 구동 중인 환경 설정 파일(`settings.cfg`)에서 핵심 파라미터(RL, TLT, CW, THR 등)를 추출하여 우측 상단의 표(Table)로 요약해 줍니다. (설정 실수 원천 차단)
  * **백프레셔(Backpressure) 모니터:** C++ 백엔드의 메모리 큐 상태(`DataQ` / `Pool`)를 시각적으로 분리 표기하여, 현재 PC의 디스크 쓰기 속도가 USB 수집 속도를 따라가고 있는지 병목 구간을 직관적으로 진단할 수 있습니다.

### ⚙️ 2.2 하드웨어 파라미터 제어 (Hardware Config)

  * 텍스트 에디터를 열 필요 없이, GUI 내에서 채널별 임계값(Threshold), 베이스라인 오프셋(DACOFF), 지연 시간(DLY), 극성(POL) 등을 스핀박스로 정밀하게 튜닝할 수 있습니다.

### 📈 2.3 데이터 프로덕션 및 변환 (Production & Transformation)

  * 수집된 순수 바이너리(`.dat`) 파일을 ROOT Tree(`.root`)로 고속 변환하는 작업을 프로그레스 바(Progress Bar)와 함께 시각적으로 제공합니다.
  * 무손실 파형 보존 모드(`-w`) 및 대화형 뷰어(`-d` Interactive Mode)를 클릭 한 번으로 실행할 수 있습니다.

### 🗄️ 2.4 데이터베이스 런 이력 관리 (Run Database)

  * SQLite3 기반의 경량 로컬 DB가 내장되어 있어, 그동안 수행했던 수집(Frontend)과 변환(Production) 이력이 자동으로 기록됩니다.
  * 각 런(Run)마다 어떤 품질(Quality)이었는지, 수집된 총 이벤트 수와 코멘트가 무엇이었는지 표 형태로 영구 보존됩니다.


## 📁 3. 디렉토리 및 소스 코드 구조 (Directory Structure)

본 프로젝트는 시스템의 안정성을 위해 C++ 백엔드(Core)와 Python 프론트엔드(GUI)가 철저하게 분리된 구조를 가집니다.

Plaintext

```
NKFADC500_mini_CLI/
├── app/                  # [C++ Application] 백엔드 실행 파일 생성 소스
│   ├── frontend_main.cpp   # 초고속 바이너리 수집기 (frontend_500_mini)
│   ├── production_main.cpp # 오프라인 ROOT 변환기 (production_500_mini)
│   └── online_monitor.cpp  # 실시간 라이브 뷰어 (online_monitor)
├── core/                 # [C++ Engine] 하드웨어 제어, 큐(Queue) 관리, DAQ 코어 모듈
├── objects/              # [C++ Data] ROOT C++ 데이터 모델 (TTree 구조화)
├── gui/                  # [Python GUI] PyQt5 기반 마스터 제어 패널
│   ├── main.py             # GUI 진입점 (Entry Point)
│   ├── core/               # IPC(프로세스간 통신), 서브프로세스 감시 및 DB 관리 매니저
│   ├── windows/            # 메인 윈도우 레이아웃 오케스트레이터
│   └── widgets/            # 기능별 독립 탭 위젯 (DaqTab, ConfigTab 등)
├── config/               # 하드웨어 구동 환경설정 파일 (settings.cfg)
├── rules/                # Linux udev USB 장치 인식 규칙 스크립트
└── CMakeLists.txt        # 최상위 빌드 스크립트 (모듈 통합 빌드 및 링킹)
```

## 🛠️ 4. 설치 및 빌드 가이드 (Build & Installation)

### Phase 1: 필수 의존성 및 패키지 설치

  * **C++ Backend:** CERN ROOT 6, CMake 3.16+, GCC (C++17), `libusb-1.0`
  * **Python GUI:** Python 3.8+, PyQt5

### Phase 2: 제조사 로우레벨 라이브러리 컴파일 (⚠️ 반드시 `su` 권한 진행)

**[핵심 주의사항]** 리눅스 보안 정책상 `sudo`를 사용하면 사용자 환경 변수(ROOT 경로 등)가 초기화되어 ROOT 딕셔너리 컴파일 에러가 발생합니다. **반드시 `su` 명령어로 관리자 계정에 진입한 뒤, 환경 변수를 수동 로드하고 컴파일해야 합니다.**

```bash
# 1. 관리자(root) 계정으로 전환
su

# 2. 필수 환경 변수 로드 (본인 시스템 경로에 맞게 수정)
source /usr/local/notice/notice_env.sh
source /usr/local/bin/thisroot.sh

# 3. Layer 1 ~ Layer 3 일괄 빌드 및 설치
cd /usr/local/notice/src/nkusb/nkusb; make clean; make install
cd ../nkusbroot; make clean; make install

cd ../../usb3com/usb3com; make clean; make install
cd ../usb3comroot; make clean; make install

cd ../../nkfadc500/nkfadc500; make clean; make install
cd ../nkfadc500root; make clean; make install

# 4. 설치 완료 후 관리자 계정 종료
exit
```

### Phase 3: 메인 애플리케이션 빌드 및 실행 (일반 유저)

```bash
# 프로젝트 최상위 폴더에서 실행
mkdir -p build && cd build
cmake ..
make -j4

# ---------------------------------------------------
# [실행 방법 1] 통합 GUI 마스터 패널 실행 (권장)
# ---------------------------------------------------
../bin/fadc500_gui

# ---------------------------------------------------
# [실행 방법 2] CLI 단독 실행 (터미널/스크립트 환경용)
# ---------------------------------------------------
# 1) 프론트엔드 수집 가동
../bin/frontend_500_mini -f ../config/settings.cfg -o ../data/run_0001.dat

# 2) 실시간 파형 모니터링 (수집 중인 터미널과 별도의 새 터미널 창에서 실행)
../bin/online_monitor ../data/run_0001.dat

# 3) 수집 완료 후 ROOT 변환 (오프라인)
../bin/production_500_mini ../data/run_0001.dat
```

## 🗺️ 5. 개발 히스토리 및 로드맵 (Development History)

  * [x] **Phase 1:** 객체지향(OOP) 기반 코어 아키텍처 및 CMake 빌드 시스템 통합
  * [x] **Phase 2:** 초고속 무결성 바이너리 수집기(Frontend) 구현 (Fail-Safe 탑재)
  * [x] **Phase 3:** 순수 플랫 트리(Pure Flat Tree) 구조의 오프라인 변환기(Production) 구현
  * [x] **Phase 4:** 메모리 릭(Leak) 방지 4x2 하이브리드 실시간 파형 뷰어(Online Monitoring) 구현
  * [x] **Phase 5:** 사용자 친화적 PyQt5 마스터 GUI 패널 및 로컬 DB 통합 완료
  * [ ] **Phase 6 (진행 중):** CAEN HV(고전압) 전원장치 소프트웨어 연동 제어 및 채널 마스킹(소프트웨어 리패킹 엔진)을 통한 데이터 획득(DAQ) 극한 성능 최적화

## 📖 6. 감사의 글 (Acknowledgements)

본 DAQ 시스템이 완성되기까지 보이지 않는 곳에서 헌신해 주신 많은 분들과 인프라에 깊은 감사를 표합니다.

**대한민국 국민 여러분께**
당장의 가시적인 이익이 보이지 않는 기초 과학의 길을 묵묵히 걸을 수 있는 것은, 국민 여러분께서 땀 흘려 조성해 주신 소중한 연구 기금 덕분입니다. 이 시스템이 앞으로 포착해 낼 우주의 미세한 신호들은 모두 국민 여러분의 지지와 성원이 만들어낸 결실입니다. 보내주신 아낌없는 믿음에 깊이 감사드리며, 그 무거운 책임감과 자긍심을 안고 연구의 최전선에서 쉼 없이 탐구하겠습니다.

**Notice Korea 김상열 박사님께**
뛰어난 하드웨어를 설계해 주신 Notice Korea 김상열 대표/박사님께 깊은 감사를 드립니다. 아낌없이 공유해 주신 로우 레벨 로직과 API 래퍼, 직접 빌드해 주신 FPGA 스크립트와 상세한 테스트 코드는 이 프로젝트를 완성하는 가장 든든한 기반이 되었습니다. 하드웨어의 깊은 곳까지 세밀하게 짚어주신 박사님의 헌신과 장인 정신에 진심 어린 경의를 표합니다.

## GUI 마스터 패널 및 사용자 경험 (UI & UX)
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/4ccb7833-b175-4509-9694-eac3a2f078fd" />
<img width="1620" height="840" alt="image" src="https://github.com/user-attachments/assets/427a3dae-dff1-4d9c-984b-abbf548d4bc2" />
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/2f07e20b-62df-4656-94d8-790a0e8cc9e0" />
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/c680e1b0-6808-4414-bc6d-4db44c4048ff" />
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/cd1f1e40-5297-4758-b0b7-98ec2643c947" />
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/9366296d-e999-4835-9f10-555387bf28fe" />
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/2fac9490-d3f0-4182-96fc-482e503796ec" />
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/9b3c8240-a9ab-40d3-af5d-89c0d50d0f82" />
<img width="1220" height="832" alt="image" src="https://github.com/user-attachments/assets/d7ed0213-36a0-4139-bee8-e8dbd8a57d74" />
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/0ba21ef0-d4cc-453d-942e-56e4bdb83c55" />
<img width="1260" height="940" alt="image" src="https://github.com/user-attachments/assets/a4e0bc36-e3a8-4276-a8a8-2f10f70ed5b7" />
