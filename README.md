
# FADC500 DAQ 및 분석 프로그램

## 1. 개요

이 프로젝트는 Notice 사의 NKFADC500MHz 4ch mini 모듈을 위한 데이터 획득 및 분석 프레임워크입니다. 프로젝트는 두 개의 주요 프로그램으로 구성됩니다.

* **`frontend_500_mini`**: 데이터 획득(DAQ)을 담당하는 프로그램입니다. 하드웨어 설정을 파일 기반으로 관리하고, 수집된 데이터를 ROOT TTree 형식으로 직접 저장합니다.
* **`production_500_mini`**: `frontend`로 생성된 데이터 파일을 읽어와 전하량을 계산하여 정제된 파일을 만들거나, 대화형으로 파형을 시각화하는 분석 프로그램입니다.

기존의 ROOT 스크립트 기반 DAQ 방식에서 벗어나, 데이터 획득과 분석이 분리된 안정적이고 재사용 가능한 C++ 프로젝트로 개발되었습니다.

---

## 2. 주요 기능

* **분리된 역할**: 데이터 획득(`frontend`)과 분석(`production`) 프로그램을 분리하여 각 기능에 집중합니다.
* **설정 파일 기반 DAQ**: DAQ 파라미터를 텍스트 파일(`settings.cfg`)로 관리하여, 코드 재컴파일 없이 실험 조건 변경이 용이합니다.
* **TTree 직접 저장**: 중간 바이너리 파일 생성 없이, 수집된 데이터를 실시간으로 파싱하여 ROOT TTree 형식으로 직접 저장합니다.
* **명령어 라인 인터페이스 (CLI)**: 각 프로그램은 명확한 옵션을 통해 실행을 제어합니다.
* **안전한 중단 기능**: `Ctrl+C` 입력 시 진행 중이던 DAQ 작업을 안전하게 마무리하고 데이터를 저장한 후 종료합니다.
* **상세한 실행 정보**: DAQ 실행 시 상세한 통계 정보를 제공하여 데이터 품질을 검증할 수 있습니다.
* **데이터 후처리 및 시각화**: 수집된 데이터로부터 전하량을 계산하거나, 개별/누적 파형을 대화형으로 시각화하는 기능을 제공합니다.
* **표준 빌드 시스템**: `CMake`를 사용하여 두 프로그램을 한 번에 빌드하고 체계적으로 관리합니다.

---

## 3. 필요 사항 (Prerequisites)

### 3.1. 시스템 및 소프트웨어

본 프로그램을 빌드하고 실행하기 위해 다음 패키지들이 시스템에 설치되어 있어야 합니다.

* **운영체제**: Linux (Rocky Linux 9에서 테스트됨)
* **빌드 도구**: `cmake`, `gcc-c++`
* **필수 라이브러리**:
    * `ROOT 6`: 데이터 분석 프레임워크.
    * [cite_start]`libusb`, `libusb-devel`, `libusbx-devel`: USB 1.0 인터페이스 및 고속 인터페이스 확장성을 위한 라이브러리. [cite: 2]

```bash
# Rocky Linux / RHEL / CentOS 기반 시스템
sudo dnf install cmake gcc-c++ libusb libusb-devel libusbx-devel

# Debian / Ubuntu 기반 시스템
sudo apt-get install cmake g++ libusb-1.0-0 libusb-1.0-0-dev
```

> **Note**: ROOT는 공식 홈페이지(root.cern)를 참조하여 별도로 설치해야 합니다.

### 3.2. Notice 라이브러리 설치

[cite\_start]하드웨어 제어를 위한 전용 라이브러리를 반드시 먼저 설치해야 합니다. [cite: 1]

1.  [cite\_start]**라이브러리 파일 복사**: 제공된 `notice.tgz` 압축 파일을 원하는 위치(예: `/usr/local/`)에 해제합니다. [cite: 1]
    ```bash
    sudo tar -zxvf notice.tgz -C /usr/local/
    ```
2.  [cite\_start]**환경 설정 스크립트 수정**: `/usr/local/notice/notice_env.sh` 파일을 열어 `NKHOME`과 `ROOTHOME` 경로가 실제 설치된 위치와 일치하는지 확인하고 수정합니다. [cite: 1]
3.  [cite\_start]**라이브러리 컴파일 및 설치**: `notice_env.sh`를 적용한 후, 각 라이브러리 소스 디렉토리로 이동하여 `make install`을 실행합니다. [cite: 3] (관리자 권한 필요)
    ```bash
    source /usr/local/notice/notice_env.sh
    cd $NKHOME/src/nkusb/nkusb && make clean && sudo make install
    cd $NKHOME/src/nkusb/nkusbroot && make clean && sudo make install
    cd $NKHOME/src/usb3com/usb3com && make clean && sudo make install
    cd $NKHOME/src/usb3com/usb3comroot && make clean && sudo make install
    cd $NKHOME/src/nkfadc500/nkfadc500 && make clean && sudo make install
    cd $NKHOME/src/nkfadc500/nkfadc500root && make clean && sudo make install
    ```

### 3.3. USB 장치 접근 권한 설정

매번 `sudo`를 사용하여 프로그램을 실행하지 않으려면, FADC500 장치에 대한 접근 권한을 일반 사용자에게 부여해야 합니다. 아래 명령어를 사용하여 `udev` 규칙을 추가합니다.

```bash
# 1. udev 규칙 파일을 생성하고 내용을 추가합니다.
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0547", ATTRS{idProduct}=="1502", MODE="0666"' | sudo tee /etc/udev/rules.d/50-nkfadc500.rules

# 2. udev 규칙을 시스템에 다시 로드합니다.
sudo udevadm control --reload-rules
sudo udevadm trigger
```

-----

## 4\. 상세 설정 가이드 (`config/settings.cfg`)

`settings.cfg` 파일의 각 파라미터에 대한 상세 설명입니다.

  * [cite\_start]`sid = 1`: 장비에 할당된 고유 ID (Serial ID). [cite: 4]
  * **`sampling_rate = 1`**: ADC 샘플링 속도 **분주비**. (1: 500MSa/s -\> 2ns/sample, 2: 250MSa/s -\> 4ns/sample, 4: 125MSa/s -\> 8ns/sample)
  * `recording_length = 8`: 트리거 발생 시 저장할 파형의 길이. (1=128ns, 2=256ns, 4=512ns, 8=1us, ...)
  * `threshold = 50`: 트리거를 발생시킬 신호의 최소 높이 (ADC counts).
  * `polarity = 0`: 입력 신호의 극성. (0: Negative, 1: Positive)
  * `coincidence_width = 1000`: 여러 채널의 동시 트리거를 판별할 시간 창(window)의 폭 (단위: ns).

### 트리거 관련 상세 파라미터

#### `trigger_mode` (내부 트리거 종류 선택)

어떤 종류의 내부 트리거 로직을 활성화할지 \*\*비트마스크(bitmask)\*\*로 결정합니다. 여러 모드를 사용하려면 각 모드에 해당하는 숫자를 더하면 됩니다.

  * **1 (2^0)**: Pulse Count Trigger
  * **2 (2^1)**: Pulse Width Trigger
  * **4 (2^2)**: Peak Sum Trigger
  * **8 (2^3)**: TDC Trigger / Peak Sum OR

> **예시**: `trigger_mode = 3` (1 + 2, Pulse Count 와 Pulse Width 동시 사용)

#### `trigger_enable` (트리거 소스 선택)

어떤 소스로부터 발생한 트리거를 최종적으로 수용할지 \*\*비트마스크(bitmask)\*\*로 결정합니다.

  * **1 (2^0)**: Self Trigger (`trigger_lookup_table` 결과)
  * **2 (2^1)**: Pedestal Trigger
  * **4 (2^2)**: Software Trigger
  * **8 (2^3)**: External Trigger

> **예시**: `trigger_enable = 9` (1 + 8, 내부 트리거와 외부 트리거를 모두 사용)

#### `trigger_lookup_table` (채널 로직 조합)

4개 채널의 내부 트리거 신호(A0\~A3)를 조합하여 최종 "Self Trigger"를 만드는 \*\*하드웨어 진리표(Truth Table)\*\*입니다. 설정 파일에는 **십진수**로 값을 입력합니다.
| A3 | A2 | A1 | A0 | Data  |
|----|----|----|----|-------|
| 1  | 1  | 1  | 1  | bit15 |
| 1  | 1  | 1  | 0  | bit14 |
| 1  | 1  | 0  | 1  | bit13 |
| 1  | 1  | 0  | 0  | bit12 |
| 1  | 0  | 1  | 1  | bit11 |
| 1  | 0  | 1  | 0  | bit10 |
| 1  | 0  | 0  | 1  | bit9  |
| 1  | 0  | 0  | 0  | bit8  |
| 0  | 1  | 1  | 1  | bit7  |
| 0  | 1  | 1  | 0  | bit6  |
| 0  | 1  | 0  | 1  | bit5  |
| 0  | 1  | 0  | 0  | bit4  |
| 0  | 0  | 1  | 1  | bit3  |
| 0  | 0  | 1  | 0  | bit2  |
| 0  | 0  | 0  | 1  | bit1  |
| 0  | 0  | 0  | 0  | bit0  |
  * **입력 (Address)**: 4비트 숫자 `A3 A2 A1 A0` (CH4, CH3, CH2, CH1 순)
  * **출력 (Data)**: `trigger_lookup_table` 값의 `(A3A2A1A0)`번째 비트가 `1`이면 트리거 발생.

**기본 예시**

  * **모든 채널 OR**: `trigger_lookup_table = 65534` (0xFFFE)
  * **모든 채널 AND**: `trigger_lookup_table = 32768` (0x8000)

**고급 예시: Veto 및 Coincidence 로직**

  * **시나리오 1: CH2(Signal) 신호가 있고, CH1(Veto)과 CH3(Veto) 신호가 없을 때만 트리거**

      * **논리**: `A1 and (not A0) and (not A2)`
      * **패턴** (`A3A2A1A0`): `0010` (유일)
      * **설정값**: `trigger_lookup_table = 4` (계산: `1 << 2`)

  * **시나리오 2: CH2(Signal) 신호가 있고, 동시에 CH1(Veto) 또는 CH3(Veto) 신호가 있을 때만 트리거**

      * **논리**: `A1 and (A0 or A2)`
      * **패턴** (`A3A2A1A0`): `0011` (CH1+CH2), `0110` (CH3+CH2)
      * **설정값**: `trigger_lookup_table = 72` (계산: `(1 << 3) + (1 << 6)`)

-----

## 5\. 빌드 및 설치

### 5.1. 환경 변수 설정

```bash
source /usr/local/notice/notice_env.sh
```

### 5.2. 빌드

프로젝트 최상위 디렉토리에서 아래 명령어를 실행하면 `build/` 폴더 내에 `frontend_500_mini`와 `production_500_mini` 실행 파일이 모두 생성됩니다.

```bash
mkdir build
cd build
cmake ..
make
```

### 5.3. 설치 (선택 사항)

```bash
# 설정된 경로에 설치
make install

# 설치된 파일 제거
make uninstall
```

-----

## 6\. 사용법

### 6.1. 1단계: 데이터 획득 (`frontend_500_mini`)

먼저 `frontend_500_mini`를 실행하여 데이터를 수집하고 `.root` 파일을을 -n (이벤트 수) 또는 -t (시간) 옵션 중 하나를 반드시 선택해야 합니다.

```bash
# 기본 사용법
./build/frontend_500_mini -f <설정파일> -o <출력파일_기본이름> [-n <이벤트_수> | -t <시간(초)>]

# 예시 1: 100,000개 이벤트 획득
./build/frontend_500_mini -f config/settings.cfg -o run001_cosmic -n 100000

# 예시 2: 300초(5분) 동안 데이터 획득
./build/frontend_500_mini -f config/settings.cfg -o run002_long_run -t 300
```

### 6.2. 2단계: 데이터 처리 및 시각화 (`production_500_mini`)

다음으로 `production_500_mini`를 사용하여 생성된 파일을 분석합니다.

  * **데이터 정제 모드 (`-w`)**: 전하량 정보를 계산하여 `*.root.prod` 파일을 생성합니다.

    ```bash
    # 사용법
    ./build/production_500_mini -w -i <입력_root_파일>

    # 예시
    ./build/production_500_mini -w -i run001_cosmic.root
    ```

  * **대화형 시각화 모드 (`-d`)**: 원본 또는 정제된 파일을 열어 파형과 히스토그램을 확인합니다.

    ```bash
    # 사용법
    ./build/production_500_mini -d -i <입력_root_파일>

    # 예시
    ./build/production_500_mini -d -i run001_cosmic.root.prod
    ```

<!-- end list -->

실행시 예
````bash
[opercjy@localhost cosmic]$ frontend_500_mini -f setttings.cfg -o cosmic_test1 -n 10000
Info: nkusb_open_device: opening device Vendor ID = 0x547, Product ID = 0x1502, Serial ID = 1
vme controller found!
Info: nkusb_open_device: super speed device opened (bus = 2, address = 5, serial id = 1).
Now NKFADC500 is ready.
ch1 calibration delay = 19
ch2 calibration delay = 20
ch3 calibration delay = 18
ch4 calibration delay = 15
DRAM(0) is aligned, delay = 5, bitslip = 0
DRAM(1) is aligned, delay = 5, bitslip = 0
DRAM(2) is aligned, delay = 4, bitslip = 0
DRAM(3) is aligned, delay = 5, bitslip = 0
DRAM(4) is aligned, delay = 4, bitslip = 0
DRAM(5) is aligned, delay = 4, bitslip = 0
DRAM(6) is aligned, delay = 4, bitslip = 0
DRAM(7) is aligned, delay = 4, bitslip = 0
---- DAQ Settings Summary ----
 * SID: 1
 * Recording Length (rl=8): 1024 ns
 * Coincidence Width (cw): 200 ns
 * Threshold (thr): 50 (ADC counts)
 * Polarity (pol): Negative
-----------------------------

Starting DAQ for 10000 events. Data will be saved to 'cosmic_test1.root'
DAQ started at: 2025-07-16 12:48:52
Processing event: 1000... (2025-07-16 12:48:53)
Processing event: 2000... (2025-07-16 12:48:53)
Processing event: 3000... (2025-07-16 12:48:54)
Processing event: 4000... (2025-07-16 12:48:55)
Processing event: 5000... (2025-07-16 12:48:55)
Processing event: 6000... (2025-07-16 12:48:56)
Processing event: 7000... (2025-07-16 12:48:57)
Processing event: 8000... (2025-07-16 12:48:57)
Processing event: 9000... (2025-07-16 12:48:58)
Processing event: 10000... (2025-07-16 12:48:59)

---- DAQ Summary ----
DAQ finished at: 2025-07-16 12:48:59
Total elapsed time: 6.82 seconds
Total events collected: 10000
Average trigger rate: 1466.69 Hz

--- Trigger Type Statistics ---
  - TCB trigger       :    10000 times (100.00%)
-------------------------------

--- Trigger Pattern Statistics ---
Top 10 most frequent trigger patterns:
  - Pattern     1 (0x0001): 7175 times (71.75%)
  - Pattern 00256 (0x0100): 1928 times (19.28%)
  - Pattern 00016 (0x0010): 780 times (7.80%)
  - Pattern 00257 (0x0101): 57 times (0.57%)
  - Pattern 00272 (0x0110): 24 times (0.24%)
  - Pattern 00017 (0x0011): 21 times (0.21%)
  - Pattern 00273 (0x0111): 15 times (0.15%)
--------------------------------
Data successfully saved to cosmic_test1.root
DAQ System shut down properly.
[opercjy@localhost cosmic]$ production_500_mini -w -i cosmic_test1.root
Info: Run Info Loaded. Polarity=0, Sampling Rate Divisor=1, Time/Sample=2 ns
Processing 10000 events to create a refined dataset...
Done.
Refined data saved to: cosmic_test1.root.prod
[opercjy@localhost cosmic]$ production_500_mini -d -i cosmic_test1.root
Info: Run Info Loaded. Polarity=0, Sampling Rate Divisor=1, Time/Sample=2 ns

[메인 메뉴] e: 개별 이벤트 보기 | c: 누적 플롯 보기 | q: 종료 > e
[이벤트 0/9999] n: 다음, p: 이전, j [번호]: 점프, q: 메뉴로 > q

[메인 메뉴] e: 개별 이벤트 보기 | c: 누적 플롯 보기 | q: 종료 > c
Generating cumulative waveform plots... please wait.

[메인 메뉴] e: 개별 이벤트 보기 | c: 누적 플롯 보기 | q: 종료 > q
Displaying canvases. Close all ROOT windows to exit.
````
visualize_waveforms.cpp 실행 결과, 
````
root -l visualize_waveforms.C
root [0]
Processing visualize_waveforms.C...
File 'cosmic_test_20ns.root' opened successfully.
Found 10000 events in 'fadc_tree'.

Generating cumulative plots... This may take a moment.
Done.

[이벤트 0/9999] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > n

[이벤트 1/9999] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > n

[이벤트 2/9999] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > n

[이벤트 3/9999] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > n

[이벤트 4/9999] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > n

[이벤트 5/9999] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > n

[이벤트 6/9999] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > n

[이벤트 7/9999] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > ^C
````
<img width="1196" height="823" alt="image" src="https://github.com/user-attachments/assets/b2f6db14-67c8-4d5d-bad1-dc41d8696c66" />
<img width="1193" height="825" alt="image" src="https://github.com/user-attachments/assets/8ab8a258-98fa-4b1e-ada2-517e74cd2c3e" />



