# NKFADC500_mini_CLI : Notice NKFADC500-4 C++17 DAQ, FPGA Decoder & PySide6 Control Suite

<!-- Okabe-Ito Colorblind-Safe Badges (#0072B2 Blue, #D55E00 Vermilion, #009E73 Bluish Green, #E69F00 Orange, #CC79A7 Reddish Purple) -->
[![Platform](https://img.shields.io/badge/Platform-Linux_x86__64-0072B2?style=for-the-badge&logo=linux&logoColor=white)](https://github.com/opercjy/NKFADC500_mini_CLI)
[![Hardware](https://img.shields.io/badge/Digitizer-Notice_NKFADC500--4_(500_MS%2Fs,_12--bit)-D55E00?style=for-the-badge)](http://www.noticekorea.com/)
[![C++17 & ROOT 6](https://img.shields.io/badge/Engine-C%2B%2B17_%7C_CERN_ROOT_6-009E73?style=for-the-badge&logo=cplusplus&logoColor=white)](https://root.cern/)
[![PySide6 GUI](https://img.shields.io/badge/GUI-PySide6_(Qt6)_%2B_Okabe--Ito_Palette-E69F00?style=for-the-badge&logo=qt&logoColor=black)](https://wiki.qt.io/Qt_for_Python)
[![Verification](https://img.shields.io/badge/Verification-4--Stage_E2E_Verified_(Argon)-CC79A7?style=for-the-badge)](#9-인공지능-활용-선언-declaration-of-ai-use-in-staticdynamic-verification)

> **[Notice Hardware Maintenance & Development Declaration]**  
> 본 프레임워크는 **Notice Korea**에서 제공한 로우 레벨 API(`libnkusb`, `libusb3com`, `libNoticeNKFADC500`) 및 하드웨어 래퍼를 기반으로 작성되었습니다. 고에너지 물리(HEP)·핵물리·방사선 검출기 실험의 장기 운용과 고율 트리거(High-Rate Trigger & Noise) 환경에서도 장비가 무결하게 동작할 수 있도록, 연구자 주도하에 C++17 코어 아키텍처 재설계, 하드웨어 설정 객체(`RunInfo`/`FadcBD`) 직렬화, 64-bit FPGA 패킷 디코딩 정밀화, 및 지속적인 유지보수(Maintenance)가 이루어지고 있는 독립 오픈소스 프로젝트입니다.

---

## 목차 (Table of Contents)

1. [시스템 개요 및 하드웨어 계측 규격 (Hardware & Metrology Specifications)](#1-시스템-개요-및-하드웨어-계측-규격-hardware--metrology-specifications)
2. [프런트엔드 회로 및 신호처리 물리 개요 (Introduction to Front-End Circuit & DSP Physics)](#2-프런트엔드-회로-및-신호처리-물리-개요-introduction-to-front-end-circuit--dsp-physics)
3. [하이브리드 4계층 아키텍처 및 설정 객체(`RunInfo`/`FadcBD`) 직렬화 파이프라인](#3-하이브리드-4계층-아키텍처-및-설정-객체runinfofadcbd-직렬화-파이프라인)
4. [색각 이상자 배려(Okabe–Ito Palette) 및 고시인성 UI/UX 설계 표준](#4-색각-이상자-배려okabeito-palette-및-고시인성-uiux-설계-표준)
5. [디렉토리 구조 및 빌드·실행 가이드 (Build & Execution)](#5-디렉토리-구조-및-빌드실행-가이드-build--execution)
6. [`PROD` ROOT 트리 스키마 및 내장 메타데이터 객체](#6-prod-root-트리-스키마-및-내장-메타데이터-객체)
7. [오프라인 물리 분석 매크로 (SPE, Compton Edge, PSD)](#7-오프라인-물리-분석-매크로-spe-compton-edge-psd)
8. [정적/동적 코드 분석 및 4단계 E2E 무결성 검증 요약](#8-정적동적-코드-분석-및-4단계-e2e-무결성-검증-요약)
9. [인공지능 활용 선언 (Declaration of AI Use — `Argon` Model)](#9-인공지능-활용-선언-declaration-of-ai-use-in-staticdynamic-verification)
10. [감사의 글 (Acknowledgements)](#10-감사의-글-acknowledgements)
11. [GUI 마스터 패널 및 실측 분석 화면 (Experimental Screenshots)](#11-gui-마스터-패널-및-실측-분석-화면-uiux--experimental-screenshots)

---

## 1. 시스템 개요 및 하드웨어 계측 규격 (Hardware & Metrology Specifications)

**`NKFADC500_mini_CLI`**는 Notice Korea의 **NKFADC500 Mini (4-Channel, 500 MS/s, 12-bit Fast/Flash ADC)** 디지타이저 보드를 제어하고, Cypress FX3 USB 3.0 SuperSpeed 버스트 스트리밍을 통해 원시 바이너리(`.dat`) 파형을 무손실 수집·감시한 뒤 **CERN ROOT 6** 표준 데이터 트리(`_prod.root`)로 고속 직렬화하는 통합 데이터 획득(DAQ) 프레임워크입니다.

| 계측 항목 (Parameter) | 수식 및 하드웨어 정량 규격 (Mathematical & Hardware Specification) | 물리적·공학적 비고 (Engineering Notes) |
| :--- | :--- | :--- |
| **입력 채널 및 종단 임피던스** | $N_{\text{ch}} = 4$, $R_{\text{term}} = 50.0\ \Omega$ DC 커플링 | 12-bit DAC 오프셋(`DACOFF` $\in [0, 4095]$) 채널별 독립 제어 |
| **샘플링 주파수 및 시간 간격** | $f_s = \dfrac{500\text{ MS/s}}{\text{DSR}}$, $\Delta t_{\text{samp}} = \dfrac{1}{f_s} = 2.0\text{ ns}\times\text{DSR}$ | 분주비 $\text{DSR} \in \{1, 2, 4, 8\}$ (기본값 $\text{DSR}=1 \implies 2.0\text{ ns}$) |
| **나이퀴스트 주파수 및 입력 모드** | $f_{\text{Nyq}} = \dfrac{f_s}{2} = \dfrac{250\text{ MHz}}{\text{DSR}}$, `AMODE` $\in \{0\text{ (Raw)}, 1\text{ (Filtered)}\}$ | 아날로그/디지털 성형 모드(`NKFADC500write_AMODE`) 선택 지원 |
| **수직 양자화 해상도 ($\Delta V_{\text{LSB}}$)** | $12\text{-bit}$ ($N_{\text{ADC}} = 2^{12} = 4096$), $V_{\text{pp}} = 2.5\text{ V}_{\text{pp}} \implies \Delta V_{\text{LSB}} = \dfrac{2500\text{ mV}}{4096} = \mathbf{0.6103515625\text{ mV/ADC}}$ | 이상적 균일 양자화 잡음 $\sigma_q = \dfrac{\Delta V_{\text{LSB}}}{\sqrt{12}} \approx 0.176\text{ mV}$ ($0.289\text{ ADC}$) |
| **단위 전하 환산 계수 ($q_{\text{LSB}}$)** | $q_{\text{LSB}} = \dfrac{\Delta V_{\text{LSB}}\,\Delta t_{\text{samp}}}{R_{\text{term}}} = \dfrac{0.6103515625\text{ mV}\times 2.0\text{ ns}}{50.0\ \Omega} = \mathbf{24.4140625\text{ fC/(ADC}\cdot\text{sample)}}$ | $\mathbf{0.0244140625\text{ pC/(ADC}\cdot\text{sample)}}$ ($\text{DSR}=1$ 기준; 일반 $\text{DSR}$에 비례) |
| **레코드 길이 (`RL`) 및 샘플 수 ($N_s$)** | $\text{RL} \in \{1, 2, 4, 8, 16, 32, 64, 128, 256\}$, 채널당 패킷 길이 $L_{\text{ch}} = 128\times\text{RL}\text{ B}$ ($4\text{ch}$ 합계 $B_{\text{evt}} = 512\times\text{RL}\text{ B}$) | 채널당 헤더 $32\text{ B}$ ($4\text{ch}$ $128\text{ B}$) 제외 및 $2\text{ B/sample}$ 적용 시 $N_s = \dfrac{128\times\text{RL}-32}{2} = \mathbf{64\times\text{RL} - 16}$ ($\text{RL}=1 \implies 48\text{ pts}$, $\text{RL}=8 \implies 496\text{ pts}$) |
| **FPGA 클럭 및 타임스탬프** | $125\text{ MHz}$ 시스템 클럭 기반 Fine Counter ($8\text{ ns}$ 단위, $0\dots 124$) + Coarse Counter ($1\ \mu\text{s} = 1000\text{ ns}$ 단위) | $t = 8\text{ ns}\cdot N_{\text{fine}} + 1000\text{ ns}\cdot N_{\text{coarse}}$ (TCB: 24-bit Coarse $\approx 16.78\text{ s}$ 롤오버 언래핑; Local: 48-bit Coarse $\approx 8.9\text{ yr}$) |
| **온보드 메모리 및 버스 정렬** | $64\text{ MB}$ DDR SDRAM ($\text{BCOUNT} \in [0, 65536]\text{ KB}$) / USB 3.0 FX3 Slave FIFO | `128 KB` 스트리밍 슬라이스 / `16 KB` FX3 버스트 경계 정렬 |

---

## 2. 프런트엔드 회로 및 신호처리 물리 개요 (Introduction to Front-End Circuit & DSP Physics)

광전자증배관(PMT)이나 실리콘 광증배관(SiPM)을 고속 FADC에 결합할 때, 검출기 양극(Anode) 등가 회로와 동축 전송선 정합, 그리고 이산 신호처리(DSP)의 기본 관계를 파악하면 파형 반사(Ringing)와 전하 적분 편향을 예방할 수 있습니다.

### 2.1 PMT 양극 노턴 등가 회로, $50\ \Omega$ 전송선 정합 및 전하 분배 관계

유효 증배 이득이 $G_{\text{eff}}$인 PMT에 $N_{\text{pe}}$개의 광전자가 입사하면, 양극(Anode)은 총 전하 $Q_{\text{anode}} = N_{\text{pe}}\,e\,G_{\text{eff}}$ ($e = 1.602176634\times 10^{-19}\text{ C}$)를 공급하는 전류원 $i_{\text{anode}}(t)$와 양극 부하 저항 $R_a$ (통상 $10\text{--}100\text{ k}\Omega$), 양극 기생 정전용량 $C_a$ (통상 $3\text{--}10\text{ pF}$)의 병렬 노턴(Norton) 등가 회로로 모델링됩니다. 이 신호를 특성 임피던스 $Z_0 = \sqrt{L'/C'} = 50.0\ \Omega$ 동축 케이블을 통해 NKFADC500-4의 입력 종단 저항 $R_{\text{term}} = 50.0\ \Omega$에 연결할 때, 수신단(FADC)과 송신단(PMT 베이스)의 전압 반사 계수는 다음과 같습니다:

$$\Gamma_L = \frac{R_{\text{term}} - Z_0}{R_{\text{term}} + Z_0} = 0, \qquad \Gamma_S(\omega) = \frac{Z_{\text{source}}(\omega) - Z_0}{Z_{\text{source}}(\omega) + Z_0}$$

수신단에서 $R_{\text{term}} = Z_0 = 50.0\ \Omega$ 정합이 이루어지므로 1차 입사파는 반사 없이 흡수됩니다($\Gamma_L = 0$). 이때 송신단(PMT 베이스 출력단)의 저항 구성에 따라 전하 전달 효율 $\eta_{\text{term}} = Q_{\text{FADC}} / Q_{\text{anode}}$과 파형 감쇠 특성이 두 가지 모드로 구분됩니다:

1. **직접 $50\ \Omega$ 수신단 종단 + 직렬 기생진동 감쇠 저항 ($R_{\text{damp}} \sim 10\text{--}51\ \Omega$ 직렬 배치)**:
   - 양극과 동축 코어 사이에 소형 직렬 저항 $R_{\text{damp}}$만 배치하고 별도의 $50\ \Omega$ 병렬(Shunt) 종단 저항을 두지 않는 구성입니다. 저주파·중주파 영역($\omega C_a (R_{\text{damp}} + Z_0) \ll 1$)에서 송신단 임피던스는 $Z_{\text{source}}(0) = R_a + R_{\text{damp}} \gg Z_0$이므로 광대역 역종단($\Gamma_S = 0$) 조건은 아니지만, $\Gamma_L = 0$이므로 주 반사가 발생하지 않으며 $R_{\text{damp}}$가 리드 인덕턴스 $L_{\text{lead}}$와 $C_a$ 사이의 고주파 기생 $LC$ 링잉을 감쇠시킵니다.
   - 특히 $R_a \gg R_{\text{damp}} + R_{\text{term}}$이므로, $R_{\text{damp}}$에 의해 피크 진폭이 낮아지고 상승/하강 시정수 $\tau_a = (R_{\text{damp}} + R_{\text{term}})\,C_a$만큼 펄스 폭이 넓어지더라도 **FADC 입력 저항 $R_{\text{term}}$을 통과하는 총 적분 전하량은 거의 $100\%$ 보존**됩니다:

$$Q_{\text{FADC}} = \int_{0}^{T_{\text{gate}}} \frac{V_{\text{FADC}}(t)}{R_{\text{term}}}\,dt = Q_{\text{anode}}\left(\frac{R_a}{R_a + R_{\text{damp}} + R_{\text{term}}}\right) \approx Q_{\text{anode}} \quad \left(\eta_{\text{term}} \approx 0.999\text{ for } R_a = 100\text{ k}\Omega\right)$$

2. **송신단 병렬 역종단 (Shunt Back-Termination, $R_{\text{back}} = 50.0\ \Omega$ 병렬 배치)**:
   - 커넥터 불연속이나 FADC 입력단 반응성 성분($Z_{\text{in}}(\omega) \ne Z_0$)에 의한 미세 2차 반사파까지 송신단에서 완전 흡수($\Gamma_S \approx 0$)하기 위해 양극과 접지 사이에 $R_{\text{back}} = 50.0\ \Omega$을 병렬 배치하는 구성입니다. 이 경우 전류가 $R_{\text{back}}$과 전송선($Z_0 = R_{\text{term}} = 50.0\ \Omega$)으로 $1:1$ 분배되므로 FADC 수집 전하는 절반이 됩니다:

$$Q_{\text{FADC}}^{\text{(shunt)}} = Q_{\text{anode}}\left(\frac{R_{\text{back}}}{R_{\text{back}} + R_{\text{term}}}\right) = \frac{1}{2}\,Q_{\text{anode}} \quad \left(\eta_{\text{term}} = 0.5\right)$$

일반적인 전하 전달 계수 $\eta_{\text{term}}$에 대해, 단일 광전자(SPE, $1\text{ PE}$) 평균 적분 전하 $Q_{1\text{pe}}$와 ADC 적분 면적($\text{ADC}\cdot\text{sample}$)의 환산식은 다음과 같습니다(예시: 직접 종단 $\eta_{\text{term}} \approx 1$, $G_{\text{eff}} = 3.25\times 10^6$, $\text{DSR}=1$ 기준):

$$Q_{1\text{pe}} = \eta_{\text{term}}\,e\,G_{\text{eff}}, \qquad N_{\text{ADC}\cdot\text{samp}}(1\text{ PE}) = \frac{Q_{1\text{pe}}}{q_{\text{LSB}}} \quad \left(\text{예: } Q_{1\text{pe}} \approx 0.521\text{ pC} \implies \frac{520.7\text{ fC}}{24.414\text{ fC/(ADC}\cdot\text{sample)}} \approx 21.33\text{ ADC}\cdot\text{sample}\right)$$

### 2.2 이산 표본화, 베이스라인 추정 및 정류 적분(Rectified Integration) 잡음 편향

고속 PMT 단일 광전자 펄스(전형적 상승 시간 $t_r \sim 1.5\text{--}3\text{ ns}$, 반치폭 $\text{FWHM} \sim 4\text{--}8\text{ ns}$)를 $\Delta t_{\text{samp}} = 2.0\text{ ns}\times\text{DSR}$ 간격으로 이산 표본화할 때, 언더샘플링(Undersampling)에 의한 피크 진폭 손실과 에일리어싱을 막기 위해 아날로그/디지털 성형 대역폭(`AMODE`)과 샘플링 분주비($\text{DSR}=1 \implies 500\text{ MS/s}$)가 펄스 주 로브(Main lobe) 안에 최소 $3$개 이상의 샘플을 확보하도록 설정합니다.

[`production_main.cpp`](./app/production_main.cpp)는 이벤트별 파형 배열 $V_k$ ($k = 0, \dots, N_s - 1$)에서 트리거 지연(`DLY`) 이전의 무신호 프리트리거 구간 $N_{\text{ped}}$ 샘플을 사용하여 사건별 베이스라인 $\hat{B}$와 RMS 잡음 $\sigma_{\text{base}}$를 계산합니다([`NoticeFadc500Spec::ComputeBaselineSamples`](./core/include/NoticeFadc500Spec.hh)):

$$\hat{B} = \frac{1}{N_{\text{ped}}}\sum_{k=0}^{N_{\text{ped}}-1} V_k, \qquad \sigma_{\text{base}} = \sqrt{\frac{1}{N_{\text{ped}}}\sum_{k=0}^{N_{\text{ped}}-1}\left(V_k - \hat{B}\right)^2}$$

극성 보정 및 베이스라인 차감 파형 $w_k = (\hat{B} - V_k)\cdot(-1)^{1-\text{POL}}$ ($\text{POL}=0$: 음극성 펄스 반전, $\text{POL}=1$: 양극성 펄스 유지)에 대해, 본 프레임워크는 분석 목적에 따라 두 가지 적분량을 구분합니다:

1. **전체 창 정류 적분 (`Charge_Ch{i}`)의 양(+)의 잡음 편향**:
   - 음수 오버슈트나 베이스라인 요동을 배제하기 위해 반파 정류 클램핑 $Q_{\text{rect}} = \sum_{k=0}^{N_s-1}\max(0, w_k)$을 적용하면, 무신호 가우시안 잡음 $w_k \sim \mathcal{N}(0, \sigma_{\text{base}}^2)$의 반파 정류 기댓값 $\mathbb{E}[\max(0, w_k)] = \sigma_{\text{base}}/\sqrt{2\pi}$가 전체 샘플 수 $N_s$에 걸쳐 누적되어 양(+)의 페데스탈 오프셋을 형성합니다:

$$\mathbb{E}\!\left[Q_{\text{ped}}^{\text{(rect)}}\right] = \frac{N_s\,\sigma_{\text{base}}}{\sqrt{2\pi}} \quad \left(\text{예: } N_s = 496,\ \sigma_{\text{base}} \approx 1.19\text{ ADC} \implies \mathbb{E}[Q_{\text{ped}}^{\text{(rect)}}] \approx 235\text{ ADC}\cdot\text{sample} \approx 5.74\text{ pC}\right)$$

   - 따라서 [`offline_spe.cpp`](./offline_spe.cpp)와 같은 저전하 교정에서는 반드시 페데스탈 피크 중심 $Q_0$를 함께 피팅하여 차감($Q_{1\text{pe}} = \mu_1 - Q_0$)하거나, 피크 주변 국소 게이트(`Qtotal_Ch{i}`, $[-10, +120]\text{ ns}$)를 사용해야 합니다.
2. **비클리핑 선형 게이트 적분 ($Q_{\text{lin}} = \sum_{k=k_1}^{k_2} w_k$)의 무편향 분산**:
   - 클리핑 없이 길이 $N_{\text{gate}} = k_2 - k_1 + 1$ 구간을 선형 합산하면 평균 잡음 편향은 $\mathbb{E}[Q_{\text{ped}}^{\text{(lin)}}] = 0$이며, 적분 전하 분산은 게이트 내부 샘플 잡음과 프리트리거 베이스라인 추정 오차($\text{Var}(\hat{B}) = \sigma_{\text{base}}^2 / N_{\text{ped}}$)의 공분산 합으로 주어집니다:

$$\text{Var}(Q_{\text{lin}}) = N_{\text{gate}}\,\sigma_{\text{base}}^2\left(1 + \frac{N_{\text{gate}}}{N_{\text{ped}}}\right)$$

### 2.3 FPGA 디지털 트리거 상태 머신, 5-Bit Coarse Delay 한계 및 SDRAM 트레이닝

1. **4종 디지털 트리거 모드 (`TMODE` $\in [1, 15]$ 비트마스크, [`nkfadc500_4_test.C:36-44`](./notice_nkfadc500_4/notice/test/mini_nkfadc500/nkfadc500_4_test.C))**:
   - **Pulse Count / Height (`Bit 0`, `ten`, $\text{TMODE}=1$)**: 판별 시간 창 $\text{PCI} \in [32, 8160]\text{ ns}$ 동안 $w_k \ge \text{THR}$ ($\text{THR} \in [1, 4095]\text{ ADC}$) 조건을 만족하는 펄스 개수가 $\text{PCT} \in [1, 15]$개 이상일 때 트리거 발생($\text{PCT}=1$일 때 단일 펄스 진폭 트리거로 동작).
   - **Pulse Width (`Bit 1`, `tew`, $\text{TMODE}=2$)**: $w_k \ge \text{THR}$ 상태가 연속으로 $\text{PWT} \in [2, 1022]\text{ ns}$ 이상 유지될 때 트리거 발생.
   - **Peak Sum (`Bit 2`, `tep`, $\text{TMODE}=4$)**: 이동 합산 창 $\text{PSW} \in [2, 16{,}382]\text{ ns}$ 동안의 파형 면적 합이 26-bit 임계값 $\text{THR}_{\text{sum}} \in [1, 67{,}108{,}863]$ 이상일 때 트리거 발생.
   - **Peak Sum OR (`Bit 3`, `tep2`, $\text{TMODE}=8$)**: Peak Sum 조건의 OR 결합 트리거.
2. **16-Bit Trigger Lookup Table (`TLT`) 진리표 인코딩**:
   - 4채널 입력 조합 $(C_3, C_2, C_1, C_0) \in \{0, 1\}^4$의 16가지 상태에 대한 불리언 함수 $F(C_3,C_2,C_1,C_0) = \text{Bit}_{(8C_3 + 4C_2 + 2C_1 + C_0)}(\text{TLT})$로 정의됩니다:
     $$\text{TLT} = \texttt{0xFFFE} \implies C_0 \lor C_1 \lor C_2 \lor C_3\ (\text{Any-Channel Inclusive OR}), \qquad \text{TLT} = \texttt{0x8000} \implies C_0 \land C_1 \land C_2 \land C_3\ (\text{4-Fold Coincidence})$$
3. **5-Bit Coarse Delay 레지스터 오버플로 경계 조건**:
   - 벤더 드라이버([`NoticeNKFADC500.c:179`](./notice_nkfadc500_4/notice/src/nkfadc500/nkfadc500/NoticeNKFADC500.c))는 총 지연 시간 $D_{\text{tot}} = \text{CW} + \text{DLY}$ [$\text{ns}$]를 FPGA 레지스터 `0x20000007`에 기록할 때 다음과 같이 상위 비트와 하위 비트로 분할합니다:
     $$\text{REG}_{\text{DLY}} = \left(\left\lfloor\frac{\text{CW} + \text{DLY}}{1000}\right\rfloor \ll 10\right) \,\Big|\, \left((\text{CW} + \text{DLY}) \bmod 1000\right)$$
   - 이때 상위 Coarse 카운터가 **5-bit** ($0\dots 31 \implies \le 31{,}000\text{ ns}$)이고 하위 Fine 카운터가 $8\text{ ns}$ 클럭 단위 최대 $992\text{ ns}$이므로, 하드웨어 절대 상한은 다음과 같습니다:
     $$\text{CW} + \text{DLY} \le 31{,}992\text{ ns}$$
   - 본 프레임워크의 [`NoticeFadc500Spec.hh`](./core/include/NoticeFadc500Spec.hh)는 설정 파싱 및 하드웨어 초기화 시 $\text{CW} + \text{DLY} > 31{,}992\text{ ns}$ 오버플로로 인한 지연 시간 래핑(Wrapping)을 사전 차단합니다.
4. **SDRAM 8-Lane LVDS 트레이닝 패턴 (`0xFFAA5500`) 및 Bit-Slip 정렬**:
   - [`Fadc500Device::SafeAlignDRAM()`](./core/src/Fadc500Device.cpp)은 온보드 64 MB SDRAM의 8개 바이트 레인에 교번 비트 시계열(Training Pattern) `0xFFAA5500` (`11111111_10101010_01010101_00000000`)을 기록한 뒤, IDELAY 탭($0\dots 31$) 스캔으로 아이 다이어그램(Eye Diagram) 유효 윈도우 중앙 위상을 찾고 `BITSLIP` ($0\dots 3$)을 조정하여 워드 경계를 동기화합니다.

---

## 3. 하이브리드 4계층 아키텍처 및 설정 객체(`RunInfo`/`FadcBD`) 직렬화 파이프라인

기존 텍스트 설정 파일(`.cfg`)만 참조하던 방식에서 나아가, 본 버전은 **파형 수집 시점(Frontend)에 하드웨어에 실제 인가·검증된 `RunInfo` 및 `FadcBD` C++ 데이터 구조체(`TObject`)를 즉시 ROOT 객체 파일(`_cfg.root`)로 직렬화**하고, **오프라인 변환 시점(Production)에 이 객체를 역직렬화하여 최종 분석 파일(`_prod.root`) 내부에 영구 내장(Embedding)**하는 무결성 메타데이터 파이프라인을 갖추고 있습니다.

```text
┌────────────────────────────────────────────────────────────────────────────────────┐
│                [Master GUI] PySide6 Control Panel (bin/fadc500_gui)                │
│  • DaqTab (--thr Auto-Scan)  • OnlineMonitorTab (Okabe-Ito 20fps)  • ProductionTab │
└────────────────┬─────────────────────────────────────────┬─────────────────────────┘
                 │ QProcess Isolation                      │ Non-Blocking File Tail
                 ▼                                         ▼
┌──────────────────────────────────────────┐   ┌─────────────────────────────────────┐
│ [Core 1] frontend_nkfadc500 (C++17)      │   │ [Core 3] Direct Binary Monitor      │
│ 1. ConfigParser + CLI(--thr) 로드        │   │ • 4채널 인터리브 헤더(0,4,8,12 B)   │
│ 2. Fadc500Device 레지스터 클램핑 & PED   ├──►│   NumPy 벡터 비트시프트 실시간 해독 │
│ 3. RunInfo / FadcBD 객체 직렬화 저장:    │   │ • Okabe-Ito 색각 안전 4채널 렌더링  │
│    ➜ <run>_cfg.root & <run>.cfg 생성     │   └─────────────────────────────────────┘
│ 4. 2ms Pipeline Drain + USB3 .dat 덤프   │
└────────────────┬─────────────────────────┘
                 │ Raw Stream (<run>.dat) + Serialized Objects (<run>_cfg.root)
                 ▼
┌────────────────────────────────────────────────────────────────────────────────────┐
│ [Core 2] production_nkfadc_500 (C++17 + CERN ROOT 6)                               │
│ 1. <run>_cfg.root 에서 RunInfo & FadcBD 객체 직접 역직렬화 (구버전은 .cfg 폴백)    │
│ 2. 32-Byte 인터리브 헤더 해독, 64-bit TCB / 48-bit Local 클럭 언래핑               │
│ 3. 손상 블록 발생 시 최대 1 MB 슬라이딩 윈도우 패킷 재동기화 (TryResyncToNextHeader)│
│ 4. <run>_prod.root 내부에 TTree("PROD") + RunInfo + FadcBD 객체 동시 영구 직렬화   │
└────────────────────────────────────────────────────────────────────────────────────┘
```

### 측정 종료(`STOP`) 후 잔여 메모리 이월(Stale Buffer Bleed-Over) 원천 차단 메커니즘

- **하드웨어 파이프라인 안정화 대기 ([`Fadc500Device::ClearAndFlushUSB`](./core/src/Fadc500Device.cpp))**: `NKFADC500stop` 호출 직후 $2\text{ ms}$ ($2{,}000\ \mu\text{s} \gg \max(\text{CW}+\text{DLY}+\text{RL}) \approx 163\ \mu\text{s}$) 동안 대기하여 FPGA 지연 파이프라인 내부를 통과 중인 마지막 트리거 사건이 SDRAM에 완전히 기록되게 한 뒤 `BCOUNT`를 드레인하며, `NKFADC500reset` 이후 2차 잔여 `BCOUNT` 재검사를 통해 Cypress FX3 FIFO에 프리페치된 잔여 블록이 다음 런으로 넘어가는 현상을 $100\%$ 차단합니다.
- **소프트웨어 Producer–Consumer 동기화 ([`BinaryDaqManager.cpp`](./core/src/BinaryDaqManager.cpp))**: 원자적 `fProducerActive` 플래그를 통해 `ProducerWorker`의 Post-Stop Tail Drain이 완료될 때까지 `ConsumerWorker`가 조기 종료되지 않도록 보장하며, 새 런 시작 시 `fDataQueue` 잔여 버퍼를 전수 회수합니다.

### CAEN 고전압 하드웨어 추상화 계층(HAL) 및 `DaqTab` $\leftrightarrow$ `HvTab` 양방향 실시간 제어 ([`HvBackend.py`](./gui/core/HvBackend.py), [`HvTab.py`](./gui/widgets/HvTab.py))

검출기 운용 환경에서 사용되는 다양한 CAEN 고전압 공급기를 단일 인터페이스로 제어하기 위해, [`AbstractHvBackend`](./gui/core/HvBackend.py) 추상 클래스 기반의 **3중 드라이버 백엔드(Official `CAENHVWrapper`, Direct ASCII `$BD:..,CMD:..` USB/TCP Protocol, Digital Twin Ramp/Trip Simulator)**와 **8채널 $\to$ FADC 4채널 매핑 매트릭스**를 구축하였습니다:

1. **Universal Multichannel Crate Server (`SY5527`, `SY4527LC`, `SY4527`)**:
   - 내장 CPU 서버에 TCP/IP로 접속하여 **`(Slot, Channel)` 계층 주소** 단위로 최대 8개 이상의 채널 파라미터(`V0Set`, `I0Set`, `RUp`, `RDWn`, `SVMax`, `VMon`, `IMon`, `Status`, `Pw`)를 슬롯별 배치 쿼리로 고속 제어·감시합니다.
   - `HvTab`에서 연결된 서버의 실시간 상태(`VMon`, `IMon`, `ON`/`RAMP`/`TRIP`)는 Qt 시그널(`sig_hv_updated`)을 통해 **`DaqTab` 내부의 고전압 제어 패널에 실시간 표시**되며, 연구자는 탭을 이동하지 않고 **`DaqTab`에서 직접 목표 전압(`VSet`) 변경·적용, 개별/일괄 전원 인가(`ON`/`OFF`), 및 램핑·Trip 중 런 시작을 방지하는 안전 인터록(HV Safety Interlock)**을 수행할 수 있습니다.
2. **8-Channel NIM / Desktop 모듈 (`N1470`/`N1471`, `DT1470ET`/`DT5533E`) 및 단독 점유(Exclusive Lock) 관리**:
   - NIM 빈(Bin) 장착형 또는 탁상형(Desktop) 8채널 모듈은 USB 가상 시리얼(`/dev/ttyACM*`, `/dev/ttyUSB*`) 및 단일 소켓 TCP 인터페이스가 **단일 호스트·단일 프로세스 단독 점유(Single-Session Exclusive Lock)** 방식으로 동작합니다.
   - [`CaenAsciiProtocolBackend`](./gui/core/HvBackend.py)는 `fcntl.flock(LOCK_EX | LOCK_NB)` 비차단 배타 잠금을 통해 외부 프로그램(CAEN `GECO2020`, 웹 세션 등)의 포트 선점 여부를 즉시 진단하며, 연결 해제 또는 GUI 종료(`MainWindow.closeEvent`) 시 세션과 파일 잠금을 결정론적으로 반환하여 좀비 잠금을 방지합니다. 만약 외부 프로그램이 세션을 단독 점유 중일 때는 **수동 폴백(Manual/Exclusive-Lock Fallback)** 모드로 자동 전환하여 8채널 전압·전류 메타데이터가 누락 없이 DAQ 런 요약 및 SQLite DB에 기록되도록 보장합니다.

---

## 4. 색각 이상자 배려(Okabe–Ito Palette) 및 고시인성 UI/UX 설계 표준

본 프로젝트의 실시간 파형·스펙트럼 모니터([`OnlineMonitorTab.py`](./gui/widgets/OnlineMonitorTab.py))와 문서는 적록 색각 이상자(Deuteranopia / Protanopia) 및 청황 색각 이상자(Tritanopia) 연구자도 4개 채널 파형을 직관적으로 구별할 수 있도록, 국제 학술지(*Nature Methods*, *PTEP*, *JINST*) 표준인 **Okabe–Ito Colorblind-Safe Palette**와 고대비 타이포그래피를 적용하였습니다.

| 채널 (Channel) | 색상 명칭 (Okabe–Ito Name) | HEX 코드 | RGB 값 `(R, G, B)` | 시각적 대비 특성 (Accessibility Notes) |
| :---: | :--- | :---: | :---: | :--- |
| **Channel 0** | **Okabe–Ito Blue** | `#0072B2` | `(0, 114, 178)` | 밝은 배경(`#ECEFF1`) 대비 최고 명도 대비율, 주 신호 채널용 |
| **Channel 1** | **Okabe–Ito Vermilion** | `#D55E00` | `(213, 94, 0)` | 적록 색각 이상 시에도 순수 적색과 달리 황갈색 고대비로 명확히 분리 |
| **Channel 2** | **Okabe–Ito Bluish Green** | `#009E73` | `(0, 158, 115)` | 청록 스펙트럼 대역으로 Channel 1(주홍)과 혼동 0% |
| **Channel 3** | **Okabe–Ito Reddish Purple** | `#CC79A7` | `(204, 121, 167)` | 보조 트리거·감시 채널용 고채도 자주색 |

---

## 5. 디렉토리 구조 및 빌드·실행 가이드 (Build & Execution)

### 디렉토리 구조

```text
NKFADC500_mini_CLI/
├── app/                          # [C++17 Executables] 수집·변환·모니터 메인 소스
│   ├── frontend_main.cpp         # 고속 USB3 수집기 + RunInfo/FadcBD 객체 직렬화 (_cfg.root)
│   ├── production_main.cpp       # 64-bit FPGA 디코더 + ROOT 트리 및 RunInfo/FadcBD 통합 직렬화
│   └── online_monitor.cpp        # ROOT 캔버스 실시간 스트림 모니터
├── core/                         # [C++17 DAQ Core] 하드웨어 규격, USB3 제어, 링버퍼 풀
│   ├── include/NoticeFadc500Spec.hh # 벤더 검증 레지스터 상수·패킷 헤더 디코더 단일 규격 헤더
│   └── src/                      # Fadc500Device, BinaryDaqManager, ConfigParser, ELog
├── objects/                      # [CERN ROOT 6 Objects] TObject 파생 데이터 모델 (libFADC500Objects.so)
│   ├── include/                  # RunInfo.hh, FadcBD.hh, Pmt.hh, RawChannel.hh, LinkDef.h
│   └── src/                      # ROOT Dictionary 및 직렬화 구현부
├── gui/                          # [PySide6 Master GUI] Qt6 + PyQtGraph 통합 제어 콘솔
├── notice_nkfadc500_4/           # [Bundled Vendor SDK] Notice Korea C 로우레벨 드라이버 소스
├── config/settings.cfg           # 하드웨어 설정 템플릿
└── offline_*.cpp                 # CERN ROOT 6 기반 고급 물리 분석 매크로 4종
```

### 원클릭 빌드 및 실행

시스템에 `/usr/local/notice` (`$NKHOME`)가 설치되어 있지 않아도 `CMakeLists.txt`가 저장소 내 `notice_nkfadc500_4/` C 드라이버 소스를 자동으로 컴파일하여 `./lib`에 배치하고 상대 `RPATH` (`$ORIGIN/../lib`)로 연결합니다.

```bash
# 1. Python GUI 의존성 설치
pip3 install PySide6 pyqtgraph numpy

# 2. C++17 코어 및 Notice SDK 통합 빌드 (root 권한 불필요)
source /usr/local/bin/thisroot.sh
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
cd ..

# 3-A. PySide6 GUI 마스터 콘솔 실행
./bin/fadc500_gui

# 3-B. CLI 단독 수집 및 ROOT 프로덕션 실행
./bin/frontend_nkfadc500 -f config/settings.cfg -o data/run_0001.dat -n 100000 --thr 30
./bin/production_nkfadc_500 data/run_0001.dat -w
```

---

## 6. `PROD` ROOT 트리 스키마 및 내장 메타데이터 객체

`production_nkfadc_500`이 생성하는 `<run>_prod.root` 파일 내부에는 아래 **3종의 최상위 ROOT 객체**가 함께 저장됩니다:

1. **`RunInfo` (`Key: "RunInfo"`, `class RunInfo : public TObject`)**: 런 번호(`GetRunNumber()`), 샘플링 간격(`GetSamplingNs()`), 보드 배열(`GetFadcBD(0)`).
2. **`FadcBD` (`Key: "FadcBD"`, `class FadcBD : public TObject`)**: 수집 시점의 보드 레지스터 전체 스냅샷(`GetMID()`, `GetSAMPLING()`, `GetRL()`, `GetTLT()`, `GetTRIGEN()`, `GetPRESCALE()`, `GetZEROSUP()`, 및 채널별 `GetTHR(ch)`, `GetDACOFF(ch)`, `GetPOL(ch)`, `GetDLY(ch)`, `GetCW(ch)`, `GetTMODE(ch)`, 하드웨어 판독 페데스탈 `GetPED(ch)`).
3. **`PROD` (`Key: "PROD"`, `class TTree`)**: 이벤트별 물리량 및 파형 플랫 트리.

| `PROD` 트리 브랜치 | ROOT 타입 | 수식 및 물리적 정의 (Mathematical Definition & Unit) |
| :--- | :---: | :--- |
| `EventID` / `TriggerNumber` | `UInt_t` | 순차 이벤트 인덱스 / FPGA 하드웨어 트리거 번호 |
| `TriggerTime` / `LocalTriggerTime` | `ULong64_t` | 64-bit TCB 시각 / 48-bit 보드 로컬 시각 $t_{\text{trig}} = 8\,N_{\text{fine}} + 1000\,N_{\text{coarse}}$ [$\text{ns}$] |
| `TriggerPattern` / `TriggerType` | `UInt_t` | 4채널 트리거 발생 비트마스크 (`Bit 0..3`) 및 트리거 유형 |
| `ModuleID` / `RunNumber` | `UInt_t` / `Int_t` | 보드 MID 및 런 번호 |
| `RecordLength` / `SamplingNs` | `Int_t` / `Double_t` | 이벤트당 샘플 수 $N_s$ 및 샘플링 간격 $\Delta t_{\text{samp}}$ [$\text{ns}$] |
| `HwPedestal_Ch{i}` | `Int_t` | 패킷 헤더에 기록된 FPGA 하드웨어 페데스탈 [$\text{ADC}$] |
| `Baseline_Ch{i}` / `BaselineRMS_Ch{i}` | `Double_t` | 프리트리거 구간 평균 베이스라인 $\hat{B}_i$ 및 잡음 $\sigma_{\text{base},i}$ [$\text{ADC}$] |
| `Amplitude_Ch{i}` | `Double_t` | 피크 진폭 $A_i = \max_k w_{i,k}$ [$\text{ADC}$] ($\times 0.61035 \to \text{mV}$) |
| `PeakTime_Ch{i}` | `Double_t` | 피크 도달 시각 $t_{\text{peak},i} = k_{\max,i}\cdot\Delta t_{\text{samp}}$ [$\text{ns}$] |
| `Charge_Ch{i}` | `Double_t` | 전체 창 정류 적분 $Q_i = \sum_{k=0}^{N_s-1}\max(0, w_{i,k})$ [$\text{ADC}\cdot\text{sample}$] |
| `Qtotal_Ch{i}` / `Qtail_Ch{i}` | `Double_t` | 피크 기준 국소 창($[-10, +120]\text{ ns}$) 및 지연 꼬리 창($[+16, +120]\text{ ns}$) 적분 |
| `PsdRatio_Ch{i}` | `Double_t` | 전하 비교 파형 구별비 $\text{PSD}_i = Q_{\text{tail},i} / Q_{\text{total},i} \in [0, 1]$ |
| `wTime_Ch{i}` / `wDrop_Ch{i}` (`-w`) | `vector<double>` | 시간 축 배열 [$\text{ns}$] 및 베이스라인 차감 파형 배열 $w_{i,k}$ [$\text{ADC}$] |

```cpp
// ROOT 매크로에서 내장된 하드웨어 설정 객체(RunInfo / FadcBD) 즉시 조회 예시
TFile* f = TFile::Open("data/run_0001_prod.root", "READ");
FadcBD* bd = (FadcBD*)f->Get("FadcBD");
printf("Ch0 THR = %d ADC, DLY = %d ns, HW PED = %d ADC\n", bd->GetTHR(0), bd->GetDLY(0), bd->GetPED(0));
```

---

## 7. 오프라인 물리 분석 매크로 (SPE, Compton Edge, PSD)

1. **단일 광전자(SPE) 절대 이득 교정 ([`offline_spe.cpp`](./offline_spe.cpp))**:
   - 저광량 포아송 광원 환경에서 수집된 전하 스펙트럼을 페데스탈($n=0$)과 $n$-광전자 가우시안 합성 모델(Bellamy *et al.*, *NIM-A* **343**, 484 (1994) 기반 다중 광전자 응답; `TMath::Gaus` 피크 진폭 매개변수화 $A_n$, $\mu_2 = Q_0 + 2 Q_{1\text{pe}}$, $\sigma_2 = \sqrt{2}\sigma_1$)로 `MINUIT2` 피팅하여 단일 광전자 전하 $Q_{1\text{pe}} = \mu_1 - Q_0$ 및 유효 증배 이득 $G_{\text{eff}} = Q_{1\text{pe}} / (\eta_{\text{term}}\,e)$를 도출합니다:
     $$f_{\text{SPE}}(Q) = A_0\exp\!\left[-\frac{(Q - Q_0)^2}{2\sigma_0^2}\right] + \sum_{n=1}^{2} A_n\exp\!\left[-\frac{(Q - Q_0 - n Q_{1\text{pe}})^2}{2n\sigma_1^2}\right]$$
2. **감마선 컴프턴 엣지(Compton Edge) 정밀 피팅 ([`offline_compton_edge.cpp`](./offline_compton_edge.cpp))**:
   - 저-$Z$ 액체섬광체에서 입사 감마선 에너지 $E_\gamma$에 대한 클라인–니시나(Klein–Nishina) 최대 반도 전자 에너지(Compton Edge, $\theta = \pi$ 후방산란)는 다음과 같습니다:
     $$E_{\text{CE}}(E_\gamma) = \frac{E_\gamma}{1 + \dfrac{m_e c^2}{2 E_\gamma}} \quad \left(^{137}\text{Cs } 661.7\text{ keV} \implies 477.3\text{ keV};\ ^{60}\text{Co } 1173.2, 1332.5\text{ keV} \implies 963.4, 1118.1\text{ keV}\right)$$
   - 검출기 에너지 분해능 가우시안($\sigma_{\text{CE}}$)과 합성곱된 연속체 끝단을 상보 오차함수($\text{erfc}$) 모델로 피팅하여 컴프턴 엣지 전하 $Q_{\text{CE}}$ [$\text{pC}$]와 상대 분해능 $\sigma_{\text{CE}} / Q_{\text{CE}}$를 추출합니다:
     $$f_{\text{CE}}(Q) = A\,\text{erfc}\!\left(\frac{Q - Q_{\text{CE}}}{\sqrt{2}\,\sigma_{\text{CE}}}\right) + B$$
3. **종합 진단 및 $\Delta t$ 애프터펄스 필터링 ([`offline_anal_500.cpp`](./offline_anal_500.cpp), [`offline_anal_edu.cpp`](./offline_anal_edu.cpp))**:
   - 2D 파형 잔상(Persistence), 진폭/전하 분포, 2D PSD(`PsdRatio` vs `Charge`) 산점도, 및 사건 간 시간차 $\Delta t_k = t_{\text{trig},k} - t_{\text{trig},k-1}$ 컷을 통한 이온 피드백 애프터펄스 제거를 수행합니다.

---

## 8. 정적/동적 코드 분석 및 4단계 E2E 무결성 검증 요약

- **Stage 1 (C++17 & CMake 클린 빌드 감사)**: 번들 Notice SDK 자동 빌드, `MODULE FADC500Objects` ROOT 6 딕셔너리 정합성, `RawBuffer` Rule-of-5, 스레드 안전 `ELog` 검증 완료 (`0 Warnings, 0 Errors`).
- **Stage 2 (합성 FPGA 스트림 & `RunInfo`/`FadcBD` 객체 직렬화 E2E 검증)**: 32-Byte 인터리브 헤더, USB3 `0xFF` 버스트 패딩, 37-Byte 중도 손상 복구(`TryResyncToNextHeader` $100/100$ 복원), Zero-Suppression 헤더 전용 스트림($25/25$ 복원), 및 `_cfg.root` $\to$ `_prod.root` `RunInfo`/`FadcBD` 객체 직렬화·역직렬화 $100\%$ 일치 확인.
- **Stage 3 (CERN ROOT 6 오프라인 매크로 배치 검증)**: 4종 오프라인 매크로 배치 실행 및 `2.5 Vpp` ($50\ \Omega$) 물리 스케일 정합성 확인.
- **Stage 4 (PySide6 GUI & Okabe–Ito 라이브 모니터 검증)**: `Paths.py` 단일 DB 경로, `DaqTab` `--thr` 자동 스캔, `OnlineMonitorTab` NumPy 벡터 디코더 및 `.cfg` 자동 동기화, `ProcessManager` 세대 토큰(Generation-Token) 보호 검증 완료.

---

## 9. 인공지능 활용 선언 (Declaration of AI Use in Static/Dynamic Verification)

본 프로젝트의 하드웨어 제어 아키텍처 설계, 물리 분석 알고리즘 구현, 및 실험 데이터 검증은 연구자(Author)의 직접적인 주도하에 수행되었습니다. 소프트웨어의 신뢰성과 코드 무결성을 극대화하기 위해, 연구자는 **Google Antigravity (`agy`)** 에이전틱 환경의 **Argon (`Argon`)** 모델을 아래의 **정적/동적 코드 분석 및 교차 검증 도구**로 활용하였습니다:

1. **정적 코드 분석 및 벤더 SDK 규격 대조 (Static Code Analysis & Hardware Spec Auditing)**:
   - **Argon** 모델을 활용하여 C++17 코어(`frontend_nkfadc500`, `production_nkfadc_500`), Notice Korea 로우레벨 C SDK(`NoticeNKFADC500.c`, `usb3com.c`), 및 PySide6 GUI 스크립트 전반의 메모리 수명주기(Rule-of-5), 멀티스레드 동기화(Post-Stop Tail Drain 및 2 ms FPGA 파이프라인 안정화), 레지스터 유효 경계(`NoticeFadc500Spec.hh`), 64-bit 타이밍 오버플로, 및 `RunInfo`/`FadcBD` 객체 직렬화 파이프라인을 전수 정적 분석하고 리팩터링하였습니다.
2. **종단간 동적 회귀 검증 및 물리 스케일 감사 (Dynamic E2E Regression & Physics Calibration Testing)**:
   - **Argon** 모델을 통해 합성 FPGA 바이너리 스트림 생성부터 ROOT `PROD` 트리 및 `RunInfo`/`FadcBD` 객체 직렬화, 4종의 오프라인 물리 분석 매크로(`2.5 Vpp`, $50\ \Omega$ 전하·전압 교정), 및 PySide6 GUI 파서까지의 4단계 자동화 동적 테스트(Dynamic E2E Test Suite)를 수행하여 런타임 예외와 심볼 충돌을 검증·해결하였습니다.
3. **연구자 최종 검수 및 책임 (Author Oversight & Responsibility)**:
   - AI가 제안하거나 리팩터링한 모든 C++/Python 코드와 빌드 설정은 연구자가 실제 서버 및 검출기 데이터 환경에서 직접 실행·검수하였으며, 최종 소프트웨어 및 물리 분석 결과에 대한 모든 학술적·기술적 책임은 전적으로 연구자에게 있습니다.

---

## 10. 감사의 글 (Acknowledgements)

본 DAQ 시스템이 완성되기까지 보이지 않는 곳에서 헌신해 주신 많은 분들과 인프라에 깊은 감사를 표합니다.

**대한민국 국민 여러분께**  
당장의 가시적인 이익이 보이지 않는 기초 과학의 길을 묵묵히 걸을 수 있는 것은, 국민 여러분께서 땀 흘려 조성해 주신 소중한 연구 기금 덕분입니다. 이 시스템이 앞으로 포착해 낼 우주의 미세한 신호들은 모두 국민 여러분의 지지와 성원이 만들어낸 결실입니다. 보내주신 아낌없는 믿음에 깊이 감사드리며, 그 무거운 책임감과 자긍심을 안고 연구의 최전선에서 쉼 없이 탐구하겠습니다.

**Notice Korea 김상열 박사님께**  
뛰어난 하드웨어를 설계해 주신 Notice Korea 김상열 대표/박사님께 깊은 감사를 드립니다. 아낌없이 공유해 주신 로우 레벨 로직과 API 래퍼, 직접 빌드해 주신 FPGA 스크립트와 상세한 테스트 코드는 이 프로젝트를 완성하는 가장 든든한 기반이 되었습니다. 하드웨어의 깊은 곳까지 세밀하게 짚어주신 박사님의 헌신과 장인 정신에 진심 어린 경의를 표합니다.

---

## 11. GUI 마스터 패널 및 실측 분석 화면 (UI/UX & Experimental Screenshots)

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
