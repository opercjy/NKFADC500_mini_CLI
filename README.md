
### 📄 `NKFADC500_mini_CLI/README.md` (Refactoring v0)

# NKFADC500_mini_CLI : NoticeDAQ Standalone Control

![C++](https://img.shields.io/badge/C++-14-blue?style=flat-square&logo=c%2B%2B)
![ROOT](https://img.shields.io/badge/Framework-CERN%20ROOT%206-005aaa?style=flat-square)
![CMake](https://img.shields.io/badge/Build-CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=flat-square&logo=linux&logoColor=black)
![Status](https://img.shields.io/badge/Status-Refactoring_v0-orange?style=flat-square)
![License](https://img.shields.io/badge/License-Notice_Authorized-red?style=flat-square)

## 0. 제조사 원본 소스코드 (Notice Korea Official Vendor Code)

> **[Notice & License]**
> 본 레포지토리에 포함된 `notice_nkfadc500_4/` 디렉토리 내의 모든 원본 소스코드 및 하드웨어 통신 라이브러리(C/C++)는 하드웨어 제조사인 **Notice Korea의 공식적인 동의 및 사용 허가** 하에 업로드되었습니다. 연구 목적의 프레임워크 통합, 유지보수 및 `rm -rf` 등으로부터의 원본 유실을 방지하기 위해 형상 관리를 수행합니다.

---

## 1. 시스템 아키텍처 개요 (System Architecture)

**NKFADC500_mini_CLI** 프로젝트는 VME 크레이트 없이 USB 3.0 인터페이스로 직접 통신하는 4채널 500MS/s FADC 장비를 제어하기 위한 초경량 고속 데이터 획득(DAQ) 프레임워크입니다. 

현재 시스템은 C++ 기반의 3가지 독립적인 응용 프로그램(CLI)으로 모듈화되어 있습니다.
1. **Frontend (`frontend_main`)**: USB 3.0 Bulk Transfer를 통해 초고속으로 하드웨어 버퍼를 읽어와 ROOT 트리(`TTree`) 형태로 디스크에 기록하는 수집 엔진.
2. **Production (`production_main`)**: 수집된 원시 파형(Raw Waveform) 데이터를 오프라인에서 스캔하여 동적 페데스탈 및 전하량(Charge)을 추출하는 물리 분석기.
3. **Visualization (`visualize_waveforms`)**: 분석된 데이터를 바탕으로 실시간 누적 스펙트럼과 파형을 ROOT 캔버스에 렌더링하는 뷰어.

---

## 2. 디렉토리 구조 및 소스코드 명세 (Directory Tree)

본 프로젝트는 제조사 로우레벨(Vendor) 계층과 사용자 응용(Application) 계층으로 명확히 분리되어 있습니다.

```text
NKFADC500_mini_CLI/
├── CMakeLists.txt                  # 통합 빌드 스크립트
├── README.md                       # 프로젝트 설명서 (현재 파일)
│
├── notice_nkfadc500_4/             # [Vendor] 제조사 제공 원본 하드웨어 통신 라이브러리
│   ├── notice/
│   │   ├── notice_env.sh           # [★필수] 제조사 환경변수(NKHOME) 로드 스크립트
│   │   └── src/                    # nkusb, usb3com(USB 3.0 Bulk), nkfadc500 래퍼 라이브러리
│   ├── programmer/                 # FPGA 펌웨어 업데이트 유틸리티
│   └── how_to_*.txt                # 제조사 공식 매뉴얼
│
├── include/                        # [App] C++ 응용 프로그램 헤더 
│   ├── DaqSystem.h                 # 하드웨어 통신 래핑 및 DAQ 제어 클래스
│   └── Processor.h                 # 파형 물리량 환산(Charge, Peak, Pedestal) 클래스
│
├── src/                            # [App] 메인 프로그램 및 코어 소스코드
│   ├── DaqSystem.cpp               # 하드웨어 파라미터 적용 및 데이터 수집 로직
│   ├── Processor.cpp               # 노이즈(RMS) 기반 동적 임계값 적분 알고리즘
│   ├── frontend_main.cpp           # 데이터 획득(DAQ) CLI 실행 파일
│   ├── production_main.cpp         # 오프라인 분석 CLI 실행 파일
│   └── visualize_waveforms.cpp     # ROOT 기반 파형 및 스펙트럼 시각화 툴
│
├── config/                         # 하드웨어 동작 설정
│   └── settings.cfg                # 4채널 파라미터 (Threshold, DACOFF, Polarity 등)
│
└── view_waveform_pro.C             # ROOT 인터랙티브 파형 검증 매크로
```

---

## 3. 설치 및 빌드 방법 (Build & Installation)

### 3.1 제조사 로우레벨 라이브러리 컴파일 (최우선 수행)
응용 프로그램을 빌드하기 전에, 하드웨어와 직접 통신하는 제조사 원본 라이브러리를 먼저 컴파일해야 합니다.

```bash
# 1. 제조사 환경 변수 로드
source notice_nkfadc500_4/notice/notice_env.sh

# 2. 하위 라이브러리 순차적 컴파일
cd notice_nkfadc500_4/notice/src/nkusb
make clean; make

cd ../usb3com
make clean; make

cd ../nkfadc500
make clean; make

# 최상위 폴더로 복귀
cd ../../../../
```

### 3.2 필수 의존성 패키지 설치 (Linux)
* **CERN ROOT 6** (환경변수 `thisroot.sh` 로드 필요)
* **CMake 3.10+**, **GCC 4.8+**
* `libusb-1.0-0-dev` (하드웨어 통신용)

### 3.3 응용 프로그램 통합 빌드 (CMake)
```bash
# 빌드 디렉토리 생성 및 컴파일
mkdir build && cd build
cmake ..
make -j4
```
빌드가 완료되면 `build/` 디렉토리 내에 `frontend_500_mini`, `production_500_mini`, `visualize_waveforms` 실행 파일이 생성됩니다.

---

## 4. 사용자 매뉴얼 (Usage)

> **[⚠️ 주의]** 터미널을 열 때마다 반드시 `source notice_nkfadc500_4/notice/notice_env.sh`를 실행하여 제조사 동적 라이브러리 경로를 연결해야 프로그램이 정상 실행됩니다.

### 4.1 하드웨어 데이터 수집 (Frontend)
`config/settings.cfg` 파일의 파라미터를 읽어 하드웨어를 세팅하고 데이터를 수집합니다.
```bash
cd build/
./frontend_500_mini
# 실행 후 수집을 종료하려면 Ctrl+C 를 입력하여 안전하게(Graceful) 종료합니다.
# 결과물: raw_data.root
```

### 4.2 오프라인 물리량 분석 (Production)
수집된 원시 데이터(`raw_data.root`)의 파형을 분석하여 적분 전하량 및 페데스탈 값을 계산합니다.
```bash
./production_500_mini raw_data.root
# 결과물: raw_data.prod.root (분석된 TTree 포함)
```

### 4.3 결과 시각화 (Visualization)
분석이 완료된 파일(`raw_data.prod.root`)을 읽어 4개 채널의 파형 및 2D 누적 히스토그램을 그립니다.
```bash
./visualize_waveforms raw_data.prod.root
```
