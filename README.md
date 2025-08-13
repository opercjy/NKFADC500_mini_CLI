
````markdown
# FADC500 DAQ 및 분석 프레임워크

## 1\. 개요

[cite\_start]이 프로젝트는 **Notice社의 NKFADC500MHz 4채널 FADC 모듈**을 위한 C++ 기반 데이터 획득(DAQ) 및 분석 프레임워크입니다. [cite: 6] [cite\_start]하드웨어 매뉴얼에 명시된 FADC의 다양한 기능(파형/피크 데이터 동시 획득, 다중 트리거 모드, 제로 서프레션 등)을 최대한 활용할 수 있도록 설계되었습니다. [cite: 13, 14, 15]

본 프레임워크는 데이터 획득, 후처리, 시각화의 역할을 명확히 분리하여 안정성과 재사용성을 극대화했습니다.

## 2\. 하드웨어 사양 (FADC500 Module)

본 소프트웨어는 아래의 하드웨어 사양을 기반으로 합니다:

  * [cite\_start]**ADC**: 8채널, 500 MSa/s, 12-bit Flash ADC [cite: 7]
  * [cite\_start]**입력**: 50Ω Lemo 커넥터, 2V (peak-to-peak) 다이나믹 레인지 [cite: 8, 10]
  * [cite\_start]**대역폭**: 250 MHz 아날로그 대역폭 (125 MHz로 선택 가능) [cite: 9, 47]
  * **트리거**:
      * [cite\_start]내장 리딩 엣지 판별기 (Leading Edge Discriminator) [cite: 11]
      * [cite\_start]다중 트리거 모드: Pulse Count, Pulse Width, Peak Sum, TDC [cite: 221, 238, 245, 247]
      * [cite\_start]채널별 트리거 로직 조합을 위한 Trigger Lookup Table (LUT) [cite: 209, 250]
  * [cite\_start]**메모리**: 채널당 32k 샘플 FIFO, 총 8GB DDR3 DRAM 데이터 버퍼 [cite: 16, 17]
  * [cite\_start]**인터페이스**: USB 3.0 (최대 100 MB/s) [cite: 20]

-----

## 3\. 프로젝트 구성

### 3.1. 소프트웨어 아키텍처

본 프로젝트는 하드웨어 제어부터 사용자 애플리케이션까지 다음과 같은 계층화된 구조를 가집니다.

```mermaid
graph TD
    subgraph "사용자 영역 (User Space)"
    A["<b>1. 응용 프로그램</b><br>frontend_500_mini"] --> B;
    B["<b>2. ROOT C++ 래퍼</b><br>lib...ROOT.so"] --> C;
    C["<b>3. 핵심 C 라이브러리</b><br>libNotice...so, libusb3com.so"] --> D;
    D["<b>4. 저수준 USB 제어</b><br>libnkusb.so"] --> E;
    end
    subgraph "시스템 영역 (System Space)"
    E["<b>5. 시스템 USB 라이브러리</b><br>libusb-1.0.so"] --> F["<b>6. OS 커널</b>"];
    end
````

### 3.2. 프로그램 상세

  * **`frontend_500_mini`**: 하드웨어 설정 및 데이터 획득을 담당하는 핵심 DAQ 프로그램.
  * **`production_500_mini`**: 수집된 데이터로부터 전하량 등 물리량을 계산하는 후처리 프로그램.
  * **`visualize_waveforms`**: 수집된 파형 데이터를 상세하게 시각화하고 탐색하는 전용 뷰어 프로그램.
  * **`scripts/view_waveform_pro.C`**: ROOT 인터프리터 환경에서 파형을 분석하는 고급 매크로.

-----

## 4\. 설치 가이드

### 4.1. 1단계: 시스템 요구사항 및 라이브러리 준비

1.  **필수 패키지 설치**:
    ```bash
    # RHEL/CentOS/Rocky 기반
    sudo dnf install cmake gcc-c++ libusb-devel

    # Debian/Ubuntu 기반
    sudo apt-get install cmake g++ libusb-1.0-0-dev
    ```
2.  **ROOT 6**를 [공식 홈페이지](https://root.cern/)를 참조하여 시스템에 설치합니다.
3.  Notice社에서 제공하는 **`notice.tgz`** 압축 파일을 준비합니다.

### 4.2. 2단계: Notice 라이브러리 설치

1.  `notice.tgz` 파일을 `/usr/local/`에 압축 해제합니다.
    ```bash
    sudo tar -zxvf notice.tgz -C /usr/local/
    ```
2.  환경 설정 스크립트 `/usr/local/notice/notice_env.sh`를 열어 `NKHOME`과 `ROOTHOME` 경로가 실제 설치된 위치와 일치하는지 확인 및 수정합니다.
3.  터미널에 환경 변수를 적용하고, 각 라이브러리를 순서대로 컴파일 및 설치합니다. (관리자 권한 필요)
    ```bash
    source /usr/local/notice/notice_env.sh

    cd $NKHOME/src/nkusb/nkusb && make clean && sudo make install
    cd $NKHOME/src/nkusb/nkusbroot && make clean && sudo make install

    cd $NKHOME/src/usb3com/usb3com && make clean && sudo make install
    cd $NKHOME/src/usb3com/usb3comroot && make clean && sudo make install

    cd $NKHOME/src/nkfadc500/nkfadc500 && make clean && sudo make install
    cd $NKHOME/src/nkfadc500/nkfadc500root && make clean && sudo make install
    ```

### 4.3. 3단계: USB 장치 접근 권한 설정

FADC500 장치에 일반 사용자 접근 권한을 부여합니다.

```bash
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0547", ATTRS{idProduct}=="1502", MODE="0666"' | sudo tee /etc/udev/rules.d/50-nkfadc500.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### 4.4. 4단계: 프로젝트 빌드

프로젝트 최상위 디렉토리에서 아래 명령어를 실행하여 모든 프로그램을 빌드합니다.

```bash
mkdir -p build && cd build
cmake ..
make
```

빌드가 성공하면 `build/bin` 디렉토리 내에 모든 실행 파일이 생성됩니다.

### 4.5. 5단계: 환경 변수 설정 (선택 사항)

매번 `./build/bin/frontend_500_mini` 와 같이 긴 경로를 입력하지 않으려면, `build/bin` 디렉토리를 시스템의 `PATH` 환경 변수에 추가합니다.

`~/.bashrc` 또는 `~/.bash_profile` 파일 맨 아래에 다음 라인을 추가하세요. (`/path/to/your/project`는 실제 프로젝트 경로로 변경)

```bash
export PATH="/path/to/your/project/build/bin:$PATH"
```

파일 저장 후, 터미널을 새로 시작하거나 `source ~/.bashrc` 명령을 실행하여 변경사항을 적용합니다.

-----

## 5\. 사용법

### 5.1. 데이터 획득 (`frontend_500_mini`)

`config/settings.cfg` 파일을 수정한 후, `frontend_500_mini`를 실행하여 데이터를 수집합니다.

```bash
# 예시 1: 10만 개 이벤트 획득
frontend_500_mini -f config/settings.cfg -o run001_cosmic -n 100000

# 예시 2: 300초(5분) 동안 데이터 획득
frontend_500_mini -f config/settings.cfg -o run002_long_run -t 300
```

### 5.2. 데이터 분석 및 시각화

#### 방법 1: 데이터 후처리 (`production_500_mini`)

전하량을 계산하여 `*.prod.root` 파일을 생성합니다.

```bash
production_500_mini -w -i run001_cosmic.root
```

#### 방법 2: 전용 시각화 프로그램 (`visualize_waveforms`)

컴파일된 뷰어로 데이터를 대화형으로 탐색합니다.

```bash
visualize_waveforms run001_cosmic.root
```

-----

## 6\. 설정 상세 (`config/settings.cfg`)

### 6.1. `trigger_lookup_table` 상세 설명

[cite\_start]4개 채널(CH1\~4)의 내부 트리거 발생 여부(`A0`\~`A3`)를 조합하여 최종 트리거를 만드는 16비트 하드웨어 진리표입니다. [cite: 250, 251] 값은 **십진수**로 입력합니다.

  * **입력 (Address)**: 4비트 이진수 `A3 A2 A1 A0` (CH4, CH3, CH2, CH1 순서)
  * [cite\_start]**출력 (Data)**: `trigger_lookup_table` 값의 `(A3A2A1A0)`번째 비트가 `1`이면 최종 트리거가 발생합니다. [cite: 209]

| A3 | A2 | A1 | A0 | Data |
|---|---|---|---|---|
| 1 | 1 | 1 | 1 | bit15 |
| 1 | 1 | 1 | 0 | bit14 |
| ... | ... | ... | ... | ... |
| 0 | 0 | 0 | 1 | bit1 |
| 0 | 0 | 0 | 0 | bit0 |

#### 시나리오별 예제

  * **모든 채널 OR**: `trigger_lookup_table = 65534` (0xFFFE)
  * **모든 채널 AND**: `trigger_lookup_table = 32768` (0x8000)

  * **시나리오 1: CH1에서만 트리거 발생**

      * **논리**: `A0`
      * **해당 패턴**: `0001` (A0=1)
      * **비트 위치**: 1 (`0001`은 십진수로 1)
      * **설정값**: `2` (계산: `1 << 1`)

  * **시나리오 2: CH1과 CH2가 동시에 (Coincidence) 발생**

      * **논리**: `A1 and A0`
      * **해당 패턴**: `0011` (A1=1, A0=1)
      * **비트 위치**: 3 (`0011`은 십진수로 3)
      * **설정값**: `8` (계산: `1 << 3`)
  * **(추가) A1과 A0가 모두 1인 모든 패턴을 활성**
      * 0011 (A0, A1)
      * 0111 (A0, A1, A2)
      * 1011 (A0, A1, A3)
      * 1111 (A0, A1, A2, A3)
      * 패턴들의 비트 위치는 각각 3, 7, 11, 15
      * 2^3 + 2^7 + 2^11 + 2^15 = 8 + 128 + 2048 + 32768 = `34952`

  * **시나리오 3: CH1, CH2, CH3, CH4 중 하나라도 발생 (OR)**

      * **논리**: `A0 or A1 or A2 or A3`
      * **해당 패턴**: `0001`, `0010`, `0011`, ... , `1111` (하나라도 1이 포함된 모든 경우)
      * **비트 패턴**: `1111 1111 1111 1110` (이진수)
      * **설정값**: `65534` (0xFFFE)

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

 **예시**: `trigger_enable = 9` (1 + 8, 내부 트리거와 외부 트리거를 모두 사용)


### 6.2. 주요 파라미터 표

| 파라미터 | 설명 | 단위 / 범위 | 관련 함수 |
|---|---|---|---|
| `sid` | 장비 고유 ID | 정수 | `NKFADC500open` |
| `sampling_rate`| ADC 샘플링 속도 분주비. **1**: 500MSa/s, **2**: 250MSa/s | 1, 2, 4, ... | `NKFADC500write_DSR` |
| `recording_length`| 저장할 파형 길이. (예: 8 -\> 1024ns) | 1(128ns), 2(256ns),... | [cite\_start]`TCBIBSwrite_RL` [cite: 429] |
| `threshold` | 트리거 판별을 위한 신호 높이 문턱값. | ADC counts (1 \~ 4095) | `NKFADC500write_THR` |
| `polarity` | 입력 신호 극성. **0**: Negative, **1**: Positive. | 0 또는 1 | `NKFADC500write_POL`|
| `coincidence_width`| 동시 계수 판별 시간 창. [cite\_start]| ns (8 \~ 32,760) [cite: 426] | `NKFADC500write_CW` |
| `waveform_delay` | 트리거 시점 기준, 파형 저장 시작 지연. [cite\_start]| ns (0 \~ 32,760) [cite: 442] | `NKFADC500write_DLY` |
| `trigger_mode` | 내부 트리거 로직 선택 (비트마스크). 1:Pulse Count, 2:Pulse Width, 4:Peak Sum, 8:TDC | 1\~15 | [cite\_start]`TCBIBSwrite_TM` [cite: 469] |
| `trigger_enable` | 최종 트리거 소스 선택 (비트마스크). 1:Self, 2:Pedestal, 4:Software, 8:External | 1\~15 | `NKFADC500write_TRIGENABLE` |

```
</immersive>
```

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



