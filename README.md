
### 📄 `NKFADC500_mini_CLI/README.md` (Refactoring v0)

# NKFADC500_mini_CLI : NoticeDAQ Standalone Control

![C++](https://img.shields.io/badge/C++-17-blue?style=flat-square&logo=c%2B%2B)
![ROOT](https://img.shields.io/badge/Framework-CERN%20ROOT%206-005aaa?style=flat-square)
![CMake](https://img.shields.io/badge/Build-CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=flat-square&logo=linux&logoColor=black)
![Status](https://img.shields.io/badge/Status-Refactoring_v0-orange?style=flat-square)
![License](https://img.shields.io/badge/License-Notice_Authorized-red?style=flat-square)

Notice Korea의 FADC500 Mini (500MS/s, 12-bit, 4-Channel) 보드를 제어하고, 초고속으로 바이너리 데이터를 수집하여 ROOT 프레임워크 기반으로 분석하기 위한 **하이브리드 C++ DAQ 아키텍처**입니다. 

향후 GUI 연동 및 다양한 분석 모듈 추가가 용이하도록 객체지향적(OOP)이고 확장 가능한 구조로 설계되었습니다.

---

## 🏛️ 1. 시스템 아키텍처 개요 (System Architecture)

본 시스템은 수집(Online)과 분석(Offline)의 병목을 완벽히 분리한 하이브리드 아키텍처를 가집니다.

* **[Core 1] Frontend (`frontend_500_mini`)**: 
  * 하드웨어 제어 및 USB 3.0 통신 전담.
  * 연산 오버헤드를 없애기 위해 복잡한 파싱 없이 순수 바이너리(`.dat`) 파일로 하드디스크에 고속 덤프 (Zero-Deadtime 달성).
* **[Core 2] Production (`production_500_mini`)**: 
  * 수집된 `.dat` 바이너리 파일을 읽어 12-bit 인터리브 마스킹을 해제.
  * C++ ROOT 객체(`RawData`, `RunInfo` 등)에 담아 최종적으로 압축된 `*.root` 파일로 변환 및 적분 전하량(Charge) 추출.
* **[Core 3] Visualization (`visualize_waveforms`)**: 
  * ROOT 환경에서 파형 및 누적 스펙트럼(히스토그램)을 렌더링.
* **[Expansion] PyQt GUI Control Panel (개발 예정)**: 
  * CLI 기반의 설정 및 구동을 마우스 클릭으로 제어할 수 있는 그래픽 유저 인터페이스.

---

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

---

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

---

## 🏃 4. 시스템 구동 매뉴얼 (Usage)

### 4.1 데이터 수집 가동 (Frontend)
`config/settings.cfg` 파일에 4채널 독립 임계값(Threshold), 오프셋(DAC Offset) 등을 설정하고 수집기를 가동합니다.

```bash
# 프로젝트 루트 디렉토리 기준
./bin/frontend_500_mini -f config/settings.cfg -o run_0001.dat
```
* 수집을 종료하고 디스크에 데이터를 안전하게 저장하려면 **`Ctrl + C`** 를 입력합니다.

### 4.2 오프라인 변환 (Production) - *구현 진행 중*
```bash
./bin/production_500_mini run_0001.dat
```

---

## 🗺️ 5. 개발 로드맵 (Roadmap)

- [x] **Phase 1:** 객체지향(OOP) 기반 코어 아키텍처 및 CMake 빌드 시스템 통합
- [x] **Phase 2:** Zero-Deadtime 순수 바이너리 수집기(Frontend) 구현
- [ ] **Phase 3:** 바이너리 데이터 ROOT 객체 변환기(Production) 구현
- [ ] **Phase 4:** TTree 물리량 분석 및 ROOT 기반 뷰어(Visualization) 구현
- [ ] **Phase 5:** 사용자 친화적 PyQt GUI 통합 애플리케이션 개발
```
