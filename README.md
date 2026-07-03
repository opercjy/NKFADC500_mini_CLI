

# NKFADC500_mini_CLI : NoticeDAQ Standalone Control

[Notice Hardware Maintenance & Development Declaration]
본 프레임워크는 Notice Korea에서 제공한 로우 레벨 API 및 하드웨어 래퍼(Wrapper)를 기반으로 작성되었습니다. 고에너지 물리 실험의 장기적인 운용과 극단적인 노이즈 환경에서도 장비가 안정적으로 동작할 수 있도록, 연구자 주도하에 코어 아키텍처 재설계 및 지속적인 유지보수(Maintenance)가 이루어지고 있는 독립 프로젝트입니다.

Notice Korea의 FADC500 Mini (500MS/s, 12-bit, 4-Channel) 보드를 제어하고, 초고속으로 바이너리 데이터를 수집하여 분석하기 위한 고성능 하이브리드 C++ / Python 통합 DAQ 아키텍처입니다.

극단적인 고속 트리거(High Noise & Trigger Rate) 환경에서도 USB 버퍼가 뻗지 않도록 C++ 백엔드의 무결성 방어 로직(Zero-Deadlock, 100% Drain)이 적용되어 있으며, 이를 제어할 수 있는 PySide6(Qt6) 기반의 마스터 컨트롤 GUI 패널이 통합되어 있습니다. 특히, 최신 업데이트를 통해 C++ 프로세스 간 통신(IPC) 오버헤드를 제거하고, Numpy 기반의 다이렉트 바이너리 파서를 도입하여 화면 렌더링 병목 현상을 완벽하게 극복했습니다.

## 1. 시스템 아키텍처 개요 (System Architecture)

본 시스템은 수집(Online), 감시(Monitoring), 분석(Offline)의 병목을 완벽히 분리한 하이브리드 아키텍처를 가집니다.

* **[Core 1] Frontend (`frontend_nkfadc500`) : 안정성 검증 완료 (Stable)**
* 하드웨어 제어 및 USB 3.0(SuperSpeed) 통신 전담.
* 연산 오버헤드를 없애기 위해 복잡한 파싱 없이 순수 바이너리(`.dat`) 파일로 하드디스크에 고속 덤프.
* [핵심 기능]
* 2MB Deadlock 완전 파괴: 하드웨어 FIFO 버퍼 잔여물을 100% Drain(추출)하여 극한의 노이즈 상황에서도 뻗지 않는 수집 지원.
* Fail-Fast & Auto-Recovery: 하드웨어 미연결 시 즉각 종료, USB Flooding(과부하) 방어, 좀비 락(Error -7/-9) 자동 세척(Flush) 로직 탑재.
* 멀티스레드 큐잉(Producer-Consumer): USB 패킷 수집 스레드와 디스크 I/O 스레드를 완벽히 분리하여 병목(Backpressure) 현상 극복.




* **[Core 2] Production (`production_nkfadc_500`) : 구조 최적화 완료 (Stable)**
* 수집된 `.dat` 바이너리 파일을 읽어 12-bit 인터리브 마스킹을 해제.
* C++ ROOT 객체를 활용하여 고속으로 물리량(전하량, 피크 등)을 추출하고, 플랫 트리(Flat Tree) 구조의 `*.root` 파일로 변환.
* 메모리 캡슐화 패치: 트리 구조 내 불필요한 동적 배열(`std::vector`) 할당을 제거하고, 파형 데이터를 `Pmt` 객체 내부의 고정 배열(`_wave`)로 다이렉트 인젝션하여 메모리 안정성과 오프라인 렌더링 속도를 극대화.


* **[Core 3] Visualization (Direct Binary Parsing) : 비동기 렌더링 아키텍처 전면 개편 (Stable)**
* C++ 기반의 백그라운드 워커에서 발생하던 외부 창 팝업 및 통신 오버헤드를 근본적으로 제거.
* Python의 `numpy` 고속 비트시프트 연산을 활용하여 수집 중인 `.dat` 파일의 4채널 징검다리 인터리빙 헤더(0, 4, 8, 12 바이트)를 직접 해독하는 다이렉트 스트리밍 아키텍처 도입.
* 초당 20프레임(50ms)의 부드러운 주사율(Refresh Rate)과 동적 Y축 스케일링을 지원하여 딜레이 없는 실시간 파형/스펙트럼 감시 보장.


* **[Master GUI] PySide6 Control Panel (`fadc500_gui`) : 프레임워크 마이그레이션 완료 (Stable)**
* PyQt5에서 PySide6(Qt6)로 렌더링 엔진 전면 교체 완료. CLI 기반의 백엔드 엔진들을 서브 프로세스(QProcess)로 완벽히 격리하여 제어.
* 시스템 다운 시 강력한 `SIGKILL` 하드웨어 강제 처형 및 복구 기능 탑재.
* 정규식(Regex) 기반의 실시간 데이터 파싱 시스템을 통해 DataQ, Pool, Rate 등의 하드웨어 상태를 대시보드에 즉각 동기화.



## 2. 그래픽 인터페이스 및 사용자 경험

본 프로젝트로 완성된 통합 GUI 패널은 복잡한 설정 파일과 터미널 명령어 입력의 고통을 덜어주고, 직관적인 대시보드를 통해 실험의 가시성을 극대화합니다.

* **1:2 비율 라이브 뷰어 (Online Monitor):** 캔버스를 분할하여 좌측에는 시계열 파형(Waveform)을, 우측에는 펄스 파고/적분 스펙트럼(Histogram)을 실시간으로 렌더링합니다. 채널별 가시성(Visibility) 제어 및 최대 이벤트 누적 한계(Accumulate Events) 설정이 가능합니다.
* **메인 대시보드 및 실시간 수집 뷰:** 수집 파일명(Run Number), 정지 조건(이벤트/시간), 자동 연속 수집(Long-term)을 손쉽게 세팅. 현재 수집 중인 파일의 크기, 초당 처리 속도(MB/s), 트리거 레이트(Hz)를 즉각적으로 표시.
* **하이브리드 시스템 콘솔:** C++ 백엔드에서 출력되는 순수 텍스트, ANSI 색상 코드, 그리고 Python 내부의 HTML 태그를 완벽하게 분리하여 렌더링하는 지능형 파서를 탑재하여 시스템 로그의 가독성을 극대화.
* **스마트 Config 요약기:** 현재 구동 중인 환경 설정 파일(`settings.cfg`)에서 핵심 파라미터를 추출하여 우측 상단 표(Table)에 요약 표시.
* **하드웨어 파라미터 제어:** 채널별 임계값(Threshold), 베이스라인 오프셋(DACOFF), 지연 시간(DLY), 극성(POL) 등을 스핀박스로 정밀 제어.
* **데이터베이스 런 이력 관리:** SQLite3 기반 경량 로컬 DB 내장. 수집(Frontend)과 변환(Production) 이력, 품질(Quality), 총 이벤트 수, 코멘트 영구 보존.

## 3. 디렉토리 및 소스 코드 구조 (Directory Structure)

본 프로젝트는 시스템의 안정성을 위해 C++ 백엔드(Core)와 Python 프론트엔드(GUI)가 철저하게 분리된 구조를 가집니다.

```
NKFADC500_mini_CLI/
├── app/                  # [C++ Application] 백엔드 실행 파일 생성 소스
│   ├── frontend_main.cpp   # 초고속 바이너리 수집기 (frontend_nkfadc500)
│   └── production_main.cpp # 오프라인 ROOT 변환기 (production_nkfadc_500)
├── core/                 # [C++ Engine] 하드웨어 제어, 큐(Queue) 관리, DAQ 코어 모듈
├── objects/              # [C++ Data] ROOT C++ 데이터 모델 (TTree 구조화)
├── gui/                  # [Python GUI] PySide6 기반 마스터 제어 패널
│   ├── main.py             # GUI 진입점 (Entry Point)
│   ├── core/               # 프로세스 매니저 및 백그라운드 워커
│   ├── windows/            # 메인 윈도우 레이아웃 오케스트레이터
│   └── widgets/            # 기능별 독립 탭 위젯 (DaqTab, OnlineMonitorTab 등)
├── config/               # 하드웨어 구동 환경설정 파일 (settings.cfg)
├── rules/                # Linux udev USB 장치 인식 규칙 스크립트
├── setup.sh              # 환경 변수 및 독립 워크스페이스 구축 스크립트
├── offline_*.cpp         # ROOT 기반 오프라인 분석 매크로
└── CMakeLists.txt        # 최상위 빌드 스크립트 (모듈 통합 빌드 및 링킹)

```

## 4. 설치 및 빌드 가이드 (Build & Installation)

### Phase 1: 필수 의존성 및 패키지 설치

* **C++ Backend:** CERN ROOT 6.x (Minuit2 활성화 권장), CMake 3.16+, GCC (C++17), `libusb-1.0`
* **Python GUI:** Python 3.8+

```bash
pip3 install PySide6 pyqtgraph numpy

```

### Phase 2: 제조사 로우레벨 라이브러리 컴파일 (반드시 `su` 권한 진행)

리눅스 보안 정책상 `sudo`를 사용하면 사용자 환경 변수(ROOT 경로 등)가 초기화되어 ROOT 딕셔너리 컴파일 에러가 발생합니다. 반드시 `su` 명령어로 관리자 계정에 진입한 뒤, 환경 변수를 수동 로드하고 컴파일해야 합니다.

```bash
# 1. 관리자(root) 계정으로 전환
su

# 2. 필수 환경 변수 로드 (본인 시스템 경로에 맞게 수정)
mkdir /usr/local/include
mkdir /usr/local/lib
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
# [실행 방법 1] GUI 마스터 콘솔 실행 (권장)
# ---------------------------------------------------
cd ..
./bin/fadc500_gui

# ---------------------------------------------------
# [실행 방법 2] CLI 단독 실행 (터미널/스크립트 환경용)
# ---------------------------------------------------
# 1) 프론트엔드 수집 가동
./bin/frontend_nkfadc500 -f config/settings.cfg -o data/run_0001.dat

# 2) 수집 완료 후 ROOT 변환 (오프라인)
./bin/production_nkfadc_500 -f config/settings.cfg -d data/ -p run_0001

```

## 5. 개발 히스토리 및 로드맵 (Development History)

* [x] **Phase 1:** 객체지향(OOP) 기반 코어 아키텍처 및 CMake 빌드 시스템 통합
* [x] **Phase 2:** 초고속 무결성 바이너리 수집기(Frontend) 구현 (Fail-Safe 탑재)
* [x] **Phase 3:** 순수 플랫 트리(Pure Flat Tree) 구조의 오프라인 변환기(Production) 구현 및 구조 최적화
* [x] **Phase 4:** 프레임워크 전면 마이그레이션 (PyQt5 → PySide6) 및 로컬 DB 통합
* [x] **Phase 5:** 다이렉트 바이너리 파서(Direct Binary Parser) 기반의 0.05초 반응성 라이브 모니터링 구축
* [x] **Phase 6:** GUI 대시보드 UI/UX 전면 개편 (1:2 분할 스플릿 그리드 적용)
* [x] **Phase 7:** 교육 및 연구 목적의 물리량(pC) 기반 고급 오프라인 분석 파이프라인(ROOT 매크로) 고도화
* [ ] **Phase 8 (진행 중):** CAEN HV(고전압) 전원장치 소프트웨어 연동 제어 및 채널 마스킹을 통한 데이터 획득(DAQ) 극한 성능 최적화

## 6. 교육 및 연구용 고급 오프라인 분석 파이프라인 (Advanced Educational Analysis)

본 시스템은 단순한 데이터 수집을 넘어, 입자물리학 데이터를 분석하고 검출기의 특성을 파악하는 정석적인 방법론을 제공합니다. 데이터는 교육 및 분석에 최적화된 플랫 트리(Flat Tree) 구조로 저장되며, NKFADC500 Mini 장비의 하드웨어 스펙(12-bit 해상도, 5Vpp 동적 범위, 500MS/s, 50Ohm 임피던스)을 반영하여 ADC 카운트를 순수 물리량(pC)으로 정밀 환산(0.048828125 pC/ADC)하는 로직과 `RecordLength` 동적 스케일링이 완벽하게 적용되어 있습니다.

### 제공되는 핵심 분석 스크립트

**1. 기초 분석 및 노이즈 컷(Cut) 필터링 (`offline_anal_edu.cpp`, `offline_anal_500.cpp`)**

* 파형 누적 밀도도(Waveform Persistence), 진폭 분포, 전하량 스펙트럼, 2D 산점도를 한 화면에 렌더링합니다.
* **물리적 컷(Physics Cut)의 이해:** 진폭 컷(Amplitude Cut)을 통해 베이스라인 전자 노이즈를 제거하고, 하드웨어 클럭 기반의 시간 차이 컷($\Delta t$)을 계산하여 PMT 후속 펄스(Afterpulse)를 걸러내는 과정을 직관적으로 학습할 수 있습니다.

**2. 지능형 단일 광전자(SPE) 정밀 교정 (`offline_spe.cpp`)**

* 파일 내 최대 전하량을 능동적으로 스캔하여 데이터의 특성(단일 광자 레이저 환경인지, 고광량 섬광체 환경인지)을 진단하고 터미널에 피드백을 제공합니다.
* ROOT의 고정밀 `MINUIT2` 엔진을 사용하여 저광량 영역에 대해 **Pedestal, 1-PE, 2-PE를 포함하는 다중 가우시안(Multi-Gaussian) 피팅**을 수행합니다. 통합 피팅선뿐만 아니라 각 성분을 분해하여 화면에 중첩 렌더링함으로써, 연구자가 PMT의 절대 이득(Absolute Gain)이 도출되는 원리를 명확히 이해할 수 있습니다.

**3. 액체섬광체 컴프턴 엣지 탐색 (`offline_compton_edge.cpp`)**

* 방사선원(예: Cs-137) 스펙트럼 분석 시, 고에너지 연속체(Continuum)의 우측 끝에서부터 좌측으로 역탐색하여 컴프턴 엣지의 능선을 포착합니다.
* 오차 함수(Error Function, ERFC)를 이용한 정밀 피팅을 통해 검출기의 에너지 분해능(Resolution)과 엣지의 물리적 위치(pC)를 계산해 냅니다.

### 분석 스크립트 실행 방법

분석 환경(ROOT)이 로드된 상태에서 터미널에 아래 명령어를 입력하십시오.

```bash
# 1. 종합 분석 대시보드 (인자: 파일명, 채널, 진폭 컷)
root -l 'offline_anal_500.cpp("data/run_0001_prod.root", 0, 50.0)'

# 2. 지능형 SPE 캘리브레이션 및 Absolute Gain 도출 (인자: 파일명, 채널)
root -l 'offline_spe.cpp("data/run_0001_prod.root", 0)'

# 3. 컴프턴 엣지 정밀 피팅 (인자: 파일명, 채널, X축 최대 탐색 범위)
root -l 'offline_compton_edge.cpp("data/run_0001_prod.root", 0, 2000.0)'

# 4. 교육용 델타 T 및 동적 스케일링 뷰어
root -l 'offline_anal_edu.cpp("data/run_0001_prod.root", 0, 50.0, 10.0)'

```

## 7. 감사의 글 (Acknowledgements)

본 DAQ 시스템이 완성되기까지 보이지 않는 곳에서 헌신해 주신 많은 분들과 인프라에 깊은 감사를 표합니다.

**대한민국 국민 여러분께**
당장의 가시적인 이익이 보이지 않는 기초 과학의 길을 묵묵히 걸을 수 있는 것은, 국민 여러분께서 땀 흘려 조성해 주신 소중한 연구 기금 덕분입니다. 이 시스템이 앞으로 포착해 낼 우주의 미세한 신호들은 모두 국민 여러분의 지지와 성원이 만들어낸 결실입니다. 보내주신 아낌없는 믿음에 깊이 감사드리며, 그 무거운 책임감과 자긍심을 안고 연구의 최전선에서 쉼 없이 탐구하겠습니다.

**Notice Korea 김상열 박사님께**
뛰어난 하드웨어를 설계해 주신 Notice Korea 김상열 대표/박사님께 깊은 감사를 드립니다. 아낌없이 공유해 주신 로우 레벨 로직과 API 래퍼, 직접 빌드해 주신 FPGA 스크립트와 상세한 테스트 코드는 이 프로젝트를 완성하는 가장 든든한 기반이 되었습니다. 하드웨어의 깊은 곳까지 세밀하게 짚어주신 박사님의 헌신과 장인 정신에 진심 어린 경의를 표합니다.

## 8. GUI 마스터 패널 및 사용자 경험 (UI & UX)

최근 시스템 코어 및 프론트엔드 연동 아키텍처 개편에 맞추어 GUI 레이아웃이 새롭게 업그레이드되었습니다.

(최신 버전의 UI/UX 스크린샷 업데이트 예정입니다.)
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











