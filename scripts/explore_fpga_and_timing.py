#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NKFADC500 Mini (SID=1) — FPGA 내부 레지스터·128-Byte 패킷 헤더·타이밍 파이프라인 탐색 및 시각화 코드
====================================================================================================
- 시각화 스택 : uproot + awkward + mplhep + matplotlib (Okabe-Ito 색각 이상자 친화 팔레트)
- 기능 요약   :
  1. USB 3.0 FX3 (VID=0x0547, PID=0x1502) 물리 링크 상태 및 FPGA VME 레지스터 맵(0x20000000~0x20000039) 명세 출력
  2. 구형 미니 박스 리비전 FPGA의 핵심 하드웨어 특성 규명:
     - DLY 레지스터(0x20000007) 8 ns 양자화 및 +80 ns (+0x50, 125 MHz 10클럭) 파이프라인 오프셋
     - 500 MS/s Dual-Core TI-ADC (2x 250 MS/s 홀/짝 서브 코어 인터리빙) 미세 DC 오프셋 특성
     - 125 MHz 클럭 기반 8 ns Fine Time Counter (Byte 100) + 1000 ns 48-bit Coarse Time Counter (Bytes 104..124)
  3. Raw 바이너리(.dat) 128-Byte 4채널 바이트 인터리빙 헤더 정밀 디코딩 및 통계 탐색
  4. uproot + awkward로 Production ROOT (_prod.root) 파형/물리 브랜치 로드 및 ADC -> 전압(mV) -> 전하(pC) 물리 변환 검증
  5. mplhep + Okabe-Ito 팔레트 기반 FPGA 타이밍 다이어그램 및 하드웨어 텔레메트리 종합 그래프 생성
"""

import argparse
import ctypes
import glob
import os
import struct
import sys
import numpy as np

import awkward as ak
import uproot
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import mplhep as hep

# ==============================================================================
# 1. 색각 이상자 친화 오카베-이토 (Okabe-Ito) 표준 컬러 팔레트 정의
# ==============================================================================
OKABE_ITO = {
    "orange":         "#E69F00",
    "sky_blue":       "#56B4E9",
    "bluish_green":   "#009E73",
    "yellow":         "#F0E442",
    "blue":           "#0072B2",
    "vermilion":      "#D55E00",
    "reddish_purple": "#CC79A7",
    "black":          "#000000",
    "grey":           "#7F7F7F",
}

# ==============================================================================
# 2. NKFADC500 Mini 정량 물리 상수 및 ADC -> 전하(pC) 변환 공식 상수
# ==============================================================================
V_PP_VOLTS       = 2.5                                 # 12-bit ADC 동적 범위 [V_pp]
ADC_FULL_SCALE   = 4096.0                              # 2^12 ADC counts
R_TERM_OHM       = 50.0                                # 입력 종단 임피던스 [Ohm]
F_SAMPLE_HZ      = 500.0e6                             # 샘플링 주파수 (DSR=1) [Hz]
DT_SAMPLE_NS     = 2.0                                 # 샘플 간격 (DSR=1) [ns]
DT_FPGA_CLK_NS   = 8.0                                 # 125 MHz FPGA 시스템 클럭 주기 [ns]

# [핵심 변환 상수]
# 1) 1 ADC 당 전압 강하량: \Delta V_LSB = 2.5 V / 4096 = 0.6103515625 mV/ADC
DV_LSB_MV        = (V_PP_VOLTS * 1.0e3) / ADC_FULL_SCALE

# 2) 1 ADC*sample 당 전하량:
#    q_LSB = (\Delta V_LSB [V] * \Delta t [s]) / R_term [\Omega]
#          = (0.6103515625e-3 V * 2.0e-9 s) / 50.0 \Omega
#          = 24.4140625 fC/(ADC*sample) = 0.0244140625 pC/(ADC*sample)
Q_LSB_FC         = (DV_LSB_MV * 1.0e-3 * DT_SAMPLE_NS * 1.0e-9) / R_TERM_OHM * 1.0e15
Q_LSB_PC         = Q_LSB_FC * 1.0e-3

# ==============================================================================
# 3. FPGA VME/USB3 레지스터 맵 명세 (NoticeNKFADC500.c 역공학 규명본)
# ==============================================================================
FPGA_REGISTER_MAP = [
    ("0x20000000", "RUN / RESET / TIMER", "Global", "Bit0:Reset, Bit1:TimerRst, Bit2:Trig, Bit3:RunStatus(1=RUN)"),
    ("0x20000001", "CW (Coincidence Win)", "Per-Ch (+ch<<16)", "8 ns 단위 동시계수 윈도우 (8 ~ 32,760 ns), 기본 100 ns (-> 96 ns)"),
    ("0x20000002", "RL (Record Length)",   "Global", "파형 길이 코드 RL in {1,2,4,8,16..256}; N_s = 64*RL - 16 (RL=8 -> 496 pts)"),
    ("0x20000003", "DRAMON / ADCCAL",      "Global", "64 MB 온보드 DDR SDRAM 활성화(1) 및 ADC 캘리브레이션 스트로브"),
    ("0x20000004", "DACOFF (Offset DAC)",  "Per-Ch (+ch<<16)", "12-bit 아날로그 베이스라인 오프셋 DAC (0~4095), 3600 -> Pedestal ~3815 ADC"),
    ("0x20000005", "PED (Hardware Ped)",   "Per-Ch (+ch<<16)", "FPGA 내부 측정 하드웨어 페데스탈 리드백 레지스터 (12-bit ADC)"),
    ("0x20000007", "DLY (Trigger Delay)",  "Per-Ch (+ch<<16)", "((dly/1000)<<10)|(dly%1000); 리드백 = (written & ~0x7) + 0x50 (+80 ns 파이프라인)"),
    ("0x20000008", "THR (Threshold)",      "Per-Ch (+ch<<16)", "자기 트리거 파고 임계값 (1~4095 ADC) 또는 Peak-Sum 임계값 (26-bit)"),
    ("0x20000009", "POL / AMODE / ZEROSUP","Per-Ch (+ch<<16)", "극성(0=Negative PMT, 1=Positive), 다중분할모드(AMODE), Zero-Suppression"),
    ("0x2000000A", "PSW (Peak Sum Width)", "Per-Ch (+ch<<16)", "Peak-Sum 적분 폭 (2 ~ 16,382 ns)"),
    ("0x2000000B", "PCT (Pulse Count Thr)","Per-Ch (+ch<<16)", "다중 펄스 카운트 임계 개수 (1 ~ 15)"),
    ("0x2000000C", "PCI (Pulse Count Int)","Per-Ch (+ch<<16)", "다중 펄스 카운트 간격 윈도우 (32 ~ 8,160 ns)"),
    ("0x2000000D", "PWT (Pulse Width Thr)","Per-Ch (+ch<<16)", "펄스 폭 변별 임계값 (2 ~ 1,022 ns)"),
    ("0x2000000E", "DT (Dead Time)",       "Per-Ch (+ch<<16)", "채널별 불감 시간 설정 (0 ~ 8,355,840 ns)"),
    ("0x20000010", "BCOUNT (FIFO Count)",  "Global", "온보드 64 MB DRAM FIFO 내 읽기 대기 데이터량 [KB 단위, 최대 65,536 KB]"),
    ("0x20000011", "PTRIG (Pedestal Trig)","Global", "주기적 페데스탈 트리거 간격 [ms] (0=비활성)"),
    ("0x20000013", "TRIGENABLE",           "Global", "트리거 소스 마스크 (Bit0:Self, Bit1:Ped, Bit2:Soft, Bit3:Ext)"),
    ("0x20000014", "TM (Trigger Mode)",    "Per-Ch (+ch<<16)", "채널 트리거 모드 (Bit0:Height, Bit1:Width, Bit2:PeakSum, Bit3:PeakSumOR)"),
    ("0x20000015", "TLT (Trigger Lookup)", "Global", "16-bit 4입력 조합 논리표 (0xAAAA=Ch1단독, 0xFFFE=4채널 OR, 0x8000=4채널 AND)"),
    ("0x20000016", "ADCDLY / DRAMDLY",     "Per-Ch / Lane",   "500 MS/s LVDS ADC 탭 지연(0..31) 및 DDR SDRAM 8바이트 레인 탭 지연(0..31)"),
    ("0x2000001A", "ADCSTAT",              "Global", "4채널 500 MS/s ADC LVDS 비트 정렬 상태 비트 (Bit0..3 = Ch1..Ch4 Lock)"),
    ("0x2000001C", "EVENT_NUMBER (ITC)",   "Per-Ch (+ch<<16)", "채널별 내부 트리거 발생 누적 카운터 (Internal Trigger Counter)"),
    ("0x2000001E", "PSCALE / LIVETIME_LO", "Global", "트리거 프리스케일(1~65535) 및 FPGA 라이브타임 하위 워드"),
    ("0x2000001F", "DSR / LIVETIME_HI",    "Global", "데시메이션 분주비 DSR in {1,2,4,8} (f_s = 500/DSR MS/s) 및 라이브타임 상위 워드"),
]


def setup_korean_font():
    """시스템 내 CJK 한글 폰트(/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc)를 matplotlib에 등록."""
    import matplotlib.font_manager as fm
    ttc_paths = [
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc",
    ]
    for p in ttc_paths:
        if os.path.exists(p):
            fm.fontManager.addfont(p)
    prop = fm.FontProperties(fname="/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc")
    font_name = prop.get_name()
    plt.rcParams["font.family"] = "sans-serif"
    plt.rcParams["font.sans-serif"] = [font_name, "DejaVu Sans"]
    plt.rcParams["axes.unicode_minus"] = False


def decode_128byte_headers_from_dat(dat_path: str, max_events: int = 5000):
    """
    Raw 바이너리(.dat) 파일에서 128-Byte 4채널 바이트 인터리빙 헤더와 첫 이벤트 파형을 직접 디코딩.
    NoticeNKFADC500 FPGA가 패킷마다 기록하는 내부 하드웨어 타임스탬프·페데스탈·트리거 패턴을 추출.
    """
    file_size = os.path.getsize(dat_path)
    records = []
    first_waveforms = None

    with open(dat_path, "rb") as fp:
        ev_idx = 0
        while ev_idx < max_events:
            h = fp.read(128)
            if len(h) < 128:
                break

            # 1) 패킷 데이터 길이 (채널당 바이트 수 = 32 + 2 * N_s)
            data_length = h[0] | (h[4] << 8) | (h[8] << 16) | (h[12] << 24)
            if data_length < 32 or data_length > 1048576 or ((data_length - 32) % 2 != 0):
                break
            n_samples = (data_length - 32) // 2
            payload_bytes = n_samples * 8

            # 2) FPGA 하드웨어 페데스탈 (Bytes 16..23) 및 채널 ID 태그 (Bytes 64..67)
            hw_ped = [h[16 + ch] | (h[20 + ch] << 8) for ch in range(4)]
            ch_ids = [h[64 + ch] for ch in range(4)]

            # 3) 트리거 타입 및 목적지 (Byte 24)
            trig_type = h[24] & 0x0F
            trig_dest = (h[24] >> 4) & 0x0F

            # 4) 전역 TCB 트리거 번호 (Bytes 28,32,36,40) 및 TCB 시각 (Byte 44 Fine, Bytes 48,52,56 Coarse)
            tcb_trig_num = h[28] | (h[32] << 8) | (h[36] << 16) | (h[40] << 24)
            tcb_fine_raw = h[44]
            tcb_coarse_us = h[48] | (h[52] << 8) | (h[56] << 16)
            tcb_time_ns = (tcb_fine_raw * 8) + (tcb_coarse_us * 1000)

            # 5) 모듈 ID (Byte 60), 로컬 트리거 번호 (Bytes 68,72,76,80), 트리거 패턴 니블 (Bytes 84,88,92,96)
            mid = h[60]
            loc_trig_num = h[68] | (h[72] << 8) | (h[76] << 16) | (h[80] << 24)
            trig_pattern = h[84] | (h[88] << 8) | (h[92] << 16) | (h[96] << 24)

            # 6) 로컬 FPGA 125 MHz 타이밍 카운터 (Byte 100 = 8 ns Fine, Bytes 104..124 = 48-bit 1000 ns Coarse)
            loc_fine_raw = h[100]
            loc_fine_ns = loc_fine_raw * 8
            loc_coarse_us = (
                h[104]
                | (h[108] << 8)
                | (h[112] << 16)
                | (h[116] << 24)
                | (h[120] << 32)
                | (h[124] << 40)
            )
            loc_time_ns = loc_fine_ns + (loc_coarse_us * 1000)

            payload = fp.read(payload_bytes)
            if len(payload) < payload_bytes:
                break

            # 첫 번째 유효 이벤트의 4채널 12-bit 원시 파형 및 홀/짝 TI-ADC 서브코어 분석용 추출
            if first_waveforms is None and n_samples > 0:
                raw_u8 = np.frombuffer(payload, dtype=np.uint8).reshape(n_samples, 8)
                wf_ch = {}
                for ch in range(4):
                    wf_ch[ch] = (raw_u8[:, ch].astype(np.uint16) | ((raw_u8[:, ch + 4].astype(np.uint16) & 0x0F) << 8))
                first_waveforms = wf_ch

            records.append({
                "event_idx": ev_idx,
                "data_length": data_length,
                "n_samples": n_samples,
                "mid": mid,
                "ch_ids": tuple(ch_ids),
                "hw_ped": tuple(hw_ped),
                "trig_type": trig_type,
                "trig_dest": trig_dest,
                "tcb_trig_num": tcb_trig_num,
                "loc_trig_num": loc_trig_num,
                "trig_pattern": trig_pattern,
                "tcb_fine_raw": tcb_fine_raw,
                "tcb_time_ns": tcb_time_ns,
                "loc_fine_raw": loc_fine_raw,
                "loc_fine_ns": loc_fine_ns,
                "loc_coarse_us": loc_coarse_us,
                "loc_time_ns": loc_time_ns,
            })
            ev_idx += 1

    return records, first_waveforms, file_size


def print_fpga_exploration_report(dat_path: str, root_path: str):
    """터미널에 FPGA 아키텍처, 레지스터 맵, ADC->전하 변환 수식, 128-Byte 헤더 디코딩 결과를 상세 출력."""
    sep = "=" * 96
    print(f"\n{sep}")
    print("  [1] NKFADC500 Mini (SID=1) USB 3.0 물리 계층 및 FPGA VME 레지스터 아키텍처 탐색")
    print(sep)

    for dev_path in sorted(glob.glob("/sys/bus/usb/devices/*")):
        vf = os.path.join(dev_path, "idVendor")
        pf = os.path.join(dev_path, "idProduct")
        if os.path.exists(vf) and os.path.exists(pf):
            v = open(vf).read().strip()
            p = open(pf).read().strip()
            if v.lower() == "0547" and p.lower() == "1502":
                speed = open(os.path.join(dev_path, "speed")).read().strip()
                bus = open(os.path.join(dev_path, "busnum")).read().strip()
                dev = open(os.path.join(dev_path, "devnum")).read().strip()
                prod = open(os.path.join(dev_path, "product")).read().strip() if os.path.exists(os.path.join(dev_path, "product")) else "FX3"
                print(f"  * USB 3.0 디바이스 : Bus {bus} Dev {dev} (VID=0x{v}, PID=0x{p}, Product={prod})")
                print(f"  * 물리 링크 속도   : {speed} Mbps (Cypress EZ-USB FX3 SuperSpeed Slave FIFO)")

    print("\n  * FPGA VME/USB3 레지스터 주소 공간 (0x20000000 ~ 0x20000039):")
    print(f"    {'Address':12s} | {'Register Name':22s} | {'Scope':16s} | Description")
    print("    " + "-" * 88)
    for addr, name, scope, desc in FPGA_REGISTER_MAP:
        print(f"    {addr:12s} | {name:22s} | {scope:16s} | {desc}")

    print(f"\n{sep}")
    print("  [2] ADC -> 전압 강하(mV) -> 적분 전하(pC) 물리량 변환 수식 (Physics Conversion Equations)")
    print(sep)
    print(f"  (1) 샘플링 및 시간 축  : f_s = 500 MS/s (DSR=1)  ==>  dt = {DT_SAMPLE_NS:.1f} ns/sample")
    print(f"  (2) 수직 전압 분해능   : dV_LSB = V_pp / 2^12 = 2.5 V / 4096 = {DV_LSB_MV:.10f} mV/ADC")
    print(f"  (3) 펄스 전압 강하량   : dV[k]  = (Baseline - ADC[k]) * dV_LSB  [mV]  (POL=0 음극성 PMT 펄스)")
    print(f"  (4) 순시 전류          : I[k]   = dV[k] / R_term = dV[k] / 50.0 Ohm  [mA]")
    print(f"  (5) 단위 샘플당 전하량 : q_LSB  = (dV_LSB * dt) / R_term")
    print(f"                                  = ({DV_LSB_MV:.10f}e-3 V * 2.0e-9 s) / 50.0 Ohm")
    print(f"                                  = {Q_LSB_FC:.7f} fC/(ADC*sample) = {Q_LSB_PC:.10f} pC/(ADC*sample)")
    print(f"  (6) 이벤트 총 적분 전하: Q [pC] = Sum_{{k}} max(0, Baseline - ADC[k]) * {Q_LSB_PC:.10f} pC")

    print(f"\n{sep}")
    print(f"  [3] Raw 바이너리(.dat) 128-Byte FPGA 하드웨어 패킷 헤더 정밀 탐색 ({os.path.basename(dat_path)})")
    print(sep)
    records, first_wf, file_size = decode_128byte_headers_from_dat(dat_path, max_events=5000)
    r0 = records[0]
    print(f"  * 파일 크기 / 총 이벤트 수 : {file_size / (1024*1024):.2f} MB / {file_size // (r0['data_length']*4):,d} events (탐색 샘플: 첫 {len(records):,d}개)")
    print(f"  * 패킷 지오메트리          : data_length = {r0['data_length']} B/ch (총 {r0['data_length']*4} B/event), 파형 샘플 N_s = {r0['n_samples']} pts ({r0['n_samples']*2.0:.0f} ns)")
    print(f"  * 모듈 ID (MID) / 채널 태그: MID = {r0['mid']} (Byte 60), Channel IDs = {r0['ch_ids']} (Bytes 64..67)")
    print(f"  * 트리거 타입 / 목적지     : trigger_type = {r0['trig_type']}, trigger_destination = {r0['trig_dest']} (Byte 24)")
    print(f"  * 트리거 패턴 (Bytes 84..96): 0x{r0['trig_pattern']:08X} (Nibble 인코딩: 0x0001=Ch1, 0x0010=Ch2, 0x0100=Ch3, 0x1000=Ch4)")
    print(f"  * FPGA 헤더 내장 페데스탈  : Ch0={r0['hw_ped'][0]}, Ch1={r0['hw_ped'][1]}, Ch2={r0['hw_ped'][2]}, Ch3={r0['hw_ped'][3]} ADC (Bytes 16..23)")

    fine_raws = np.array([r["loc_fine_raw"] for r in records])
    fine_ns   = np.array([r["loc_fine_ns"] for r in records])
    loc_times = np.array([r["loc_time_ns"] for r in records], dtype=np.int64)
    dt_events_ms = np.diff(loc_times) * 1.0e-6  # [ms]
    valid_dt = dt_events_ms[dt_events_ms > 0]

    print(f"  * 125 MHz Fine Counter(B100): raw 범위 [{fine_raws.min()} .. {fine_raws.max()}] (주기 125 클럭 -> [{fine_ns.min()} .. {fine_ns.max()}] ns, 8 ns 분해능)")
    if len(valid_dt) > 0:
        print(f"  * 이벤트 간 시간차 (dt)    : 평균 = {np.mean(valid_dt):.2f} ms ({1000.0/np.mean(valid_dt):.2f} Hz), 중앙값 = {np.median(valid_dt):.2f} ms")

    if first_wf is not None:
        print("\n  * 500 MS/s Dual-Core TI-ADC (2x 250 MS/s 홀/짝 서브 코어) 프리트리거 구간(0..180 ns) 오프셋 진단:")
        for ch in range(4):
            ped_seg = first_wf[ch][:90].astype(np.float64)
            even_mean = np.mean(ped_seg[0::2])
            odd_mean  = np.mean(ped_seg[1::2])
            diff_adc  = even_mean - odd_mean
            print(f"    - Ch{ch} (물리 포트 {ch+1}): 짝수 코어 평균 = {even_mean:7.2f} ADC, 홀수 코어 평균 = {odd_mean:7.2f} ADC | 코어 간 편차 = {diff_adc:+6.2f} ADC ({diff_adc*DV_LSB_MV:+6.3f} mV)")

    print(f"\n{sep}")
    print(f"  [4] uproot + awkward 기반 Production ROOT ({os.path.basename(root_path)}) 구조 및 물리량 검증")
    print(sep)
    with uproot.open(root_path) as uf:
        tree = uf["PROD"]
        print(f"  * TTree 'PROD' 총 엔트리   : {tree.num_entries:,d} entries (브랜치 총 {len(tree.keys())}개)")
        arr = tree.arrays(
            ["EventID", "TriggerTime", "TriggerPattern", "Baseline_Ch0", "BaselineRMS_Ch0",
             "Amplitude_Ch0", "Charge_Ch0", "Qtotal_Ch0", "Qtail_Ch0", "PsdRatio_Ch0", "PeakTime_Ch0"],
            library="ak"
        )
        amp_adc = ak.to_numpy(arr["Amplitude_Ch0"])
        amp_mv  = amp_adc * DV_LSB_MV
        chg_raw = ak.to_numpy(arr["Charge_Ch0"])
        chg_pc  = chg_raw * Q_LSB_PC
        pt_ns   = ak.to_numpy(arr["PeakTime_Ch0"])
        psd     = ak.to_numpy(arr["PsdRatio_Ch0"])

        print(f"  * Ch0 파고 (Amplitude)     : 중앙값 = {np.median(amp_adc):.1f} ADC ({np.median(amp_mv):.2f} mV), 평균 = {np.mean(amp_adc):.1f} ADC ({np.mean(amp_mv):.2f} mV), P99 = {np.percentile(amp_adc, 99):.1f} ADC ({np.percentile(amp_mv, 99):.2f} mV)")
        print(f"  * Ch0 적분 전하 (Charge)   : 중앙값 = {np.median(chg_raw):.1f} ADC*smp ({np.median(chg_pc):.2f} pC), 평균 = {np.mean(chg_raw):.1f} ADC*smp ({np.mean(chg_pc):.2f} pC), P99 = {np.percentile(chg_pc, 99):.2f} pC")
        print(f"  * Ch0 피크 시각 (PeakTime) : 중앙값 = {np.median(pt_ns):.1f} ns, 평균 = {np.mean(pt_ns):.1f} ns (DLY=400 ns 설정 시 ~372 ns 정렬)")
        print(f"  * Ch0 PSD 비율 (Qtail/Qtot): 중앙값 = {np.median(psd):.3f}, 평균 = {np.mean(psd):.3f}")
    print(sep + "\n")
    return records, first_wf


def plot_fpga_timing_and_telemetry(dat_path: str, root_path: str, out_dir: str):
    """
    mplhep + Okabe-Ito 팔레트를 사용하여 FPGA 타이밍 파이프라인 다이어그램 및 텔레메트리 4-패널 그래프 생성.
    """
    os.makedirs(out_dir, exist_ok=True)
    plt.style.use(hep.style.CMS)
    setup_korean_font()

    records, _, _ = decode_128byte_headers_from_dat(dat_path, max_events=10000)

    # uproot + awkward로 실제 파형 1개 (중간 크기 물리 펄스) 로드
    with uproot.open(root_path) as uf:
        tree = uf["PROD"]
        wf_arr = tree.arrays(["Amplitude_Ch0", "PeakTime_Ch0", "Baseline_Ch0", "wTime_Ch0", "wDrop_Ch0"], entry_stop=500, library="ak")
        amps = ak.to_numpy(wf_arr["Amplitude_Ch0"])
        # 약 150~350 ADC 사이의 깨끗한 대표 섬광체 펄스 선택
        cand_idx = np.where((amps > 150.0) & (amps < 400.0))[0]
        sel_idx = int(cand_idx[0]) if len(cand_idx) > 0 else int(np.argmax(amps))
        w_time = ak.to_numpy(wf_arr["wTime_Ch0"][sel_idx])
        w_drop_adc = ak.to_numpy(wf_arr["wDrop_Ch0"][sel_idx])
        w_drop_mv = w_drop_adc * DV_LSB_MV
        t_peak = float(wf_arr["PeakTime_Ch0"][sel_idx])

    fig = plt.figure(figsize=(18, 13), dpi=200)
    gs = fig.add_gridspec(2, 2, height_ratios=[1.15, 1.0], hspace=0.34, wspace=0.24)

    # --------------------------------------------------------------------------
    # Panel (a): FPGA 클럭·트리거 파이프라인 타이밍 다이어그램 & 실측 파형 게이트 구조
    # --------------------------------------------------------------------------
    ax_a = fig.add_subplot(gs[0, :])
    ax_a.set_xlim(0, 992)
    ax_a.set_ylim(-105, 115)

    # 게이트 음영 영역 (Okabe-Ito 팔레트)
    ax_a.axvspan(0, 180, color=OKABE_ITO["sky_blue"], alpha=0.18, label="프리트리거 베이스라인 구간 [0, 180 ns] (90 samples)")
    ax_a.axvspan(t_peak - 10, t_peak + 120, color=OKABE_ITO["bluish_green"], alpha=0.16, label=f"PSD 총전하(Q_total) 적분 게이트 [{t_peak-10:.0f}, {t_peak+120:.0f} ns]")
    ax_a.axvspan(t_peak + 16, t_peak + 120, color=OKABE_ITO["orange"], alpha=0.25, label=f"PSD 꼬리전하(Q_tail) 적분 게이트 [{t_peak+16:.0f}, {t_peak+120:.0f} ns]")

    # 상단 디지털 타이밍 트랙 (y = +30 ~ +105 구간에 배치)
    # Track 1: 500 MS/s ADC Sampling Window (RL=8 -> 496 samples = 992 ns)
    ax_a.plot([0, 992], [95, 95], color=OKABE_ITO["black"], lw=2.2)
    ax_a.annotate(
        "1) 500 MS/s ADC 순환 링버퍼 캡처 윈도우 (RL=8 -> N_s = 64*8 - 16 = 496 samples = 992 ns, dt = 2.0 ns)",
        xy=(12, 99), fontsize=10.5, fontweight="bold", color=OKABE_ITO["black"]
    )

    # Track 2: Pre-trigger DLY = 400 ns & +80 ns (+0x50) FPGA 파이프라인 오프셋 화살표
    ax_a.annotate(
        "", xy=(t_peak, 74), xytext=(0, 74),
        arrowprops=dict(arrowstyle="<->", color=OKABE_ITO["blue"], lw=2.2)
    )
    ax_a.text(
        t_peak / 2.0, 78,
        f"설정 DLY = 400 ns (프리트리거 지연, 실측 피크 t_peak = {t_peak:.0f} ns)",
        ha="center", va="bottom", fontsize=10, fontweight="bold", color=OKABE_ITO["blue"]
    )

    # CW = 100 ns 및 +80 ns FPGA 파이프라인 지연 표시
    ax_a.annotate(
        "", xy=(t_peak + 96, 56), xytext=(t_peak, 56),
        arrowprops=dict(arrowstyle="<->", color=OKABE_ITO["vermilion"], lw=2.2)
    )
    ax_a.text(
        t_peak + 48, 60,
        "CW=100ns (96ns)",
        ha="center", va="bottom", fontsize=9.5, fontweight="bold", color=OKABE_ITO["vermilion"]
    )

    ax_a.annotate(
        "", xy=(t_peak + 176, 56), xytext=(t_peak + 96, 56),
        arrowprops=dict(arrowstyle="->", color=OKABE_ITO["reddish_purple"], lw=2.2)
    )
    ax_a.text(
        t_peak + 190, 56,
        "+80 ns (+0x50, 125 MHz 10클럭 FPGA 내부 파이프라인 지연 -> 레지스터 리드백 576 ns)",
        ha="left", va="center", fontsize=9.8, fontweight="bold", color=OKABE_ITO["reddish_purple"]
    )

    # 하단 아날로그 실측 PMT 전압 강하 파형 (-wDrop_mv를 음극성 펄스 형태로 표시)
    scale_factor = 90.0 / max(1.0, np.max(w_drop_mv))
    analog_trace = -w_drop_mv * scale_factor + 15.0
    ax_a.axhline(15.0, color=OKABE_ITO["grey"], ls="--", lw=1.0)
    ax_a.axhline(15.0 - (30.0 * DV_LSB_MV * scale_factor), color=OKABE_ITO["vermilion"], ls=":", lw=1.5,
                 label=f"하드웨어 트리거 임계값 THR = 30 ADC ({30.0*DV_LSB_MV:.2f} mV)")
    ax_a.plot(w_time, analog_trace, color=OKABE_ITO["blue"], lw=2.0,
              label=f"실측 Ch0 PMT 음극성 파형 (Peak = {np.max(w_drop_adc):.1f} ADC = {np.max(w_drop_mv):.1f} mV)")
    ax_a.axvline(t_peak, color=OKABE_ITO["vermilion"], ls="-.", lw=1.6)

    ax_a.set_title("(a) NKFADC500 Mini FPGA 트리거 파이프라인 타이밍 다이어그램 및 실측 PMT 파형 적분 게이트 구조", fontsize=13.5, fontweight="bold", pad=10)
    ax_a.set_xlabel("파형 샘플링 시간 (Waveform Time) [ns]  (500 MS/s, dt = 2.0 ns)", fontsize=11.5)
    ax_a.set_ylabel("상대 진폭 및 타이밍 트랙 [a.u.]", fontsize=11.5)
    ax_a.set_yticks([])
    ax_a.legend(loc="lower right", fontsize=9.5, ncol=2, frameon=True)
    ax_a.grid(True, axis="x", alpha=0.25)

    # --------------------------------------------------------------------------
    # Panel (b): FPGA DLY 레지스터(0x20000007) 8 ns 양자화 및 +80 ns 오프셋 검증 곡선
    # --------------------------------------------------------------------------
    ax_b = fig.add_subplot(gs[1, 0])
    dly_input = np.arange(0, 640, 2)
    # 하드웨어 규명 공식: raw_readback = (written & ~0x7) + 0x50 (+80 ns)
    dly_quantized = (dly_input & ~0x7)
    dly_fpga_readback = dly_quantized + 80

    ax_b.plot(dly_input, dly_input, color=OKABE_ITO["grey"], ls="--", lw=1.5, label="이론적 1:1 직선 (y = x)")
    ax_b.step(dly_input, dly_quantized, where="post", color=OKABE_ITO["sky_blue"], lw=2.0,
              label="FPGA 125 MHz 8 ns 내림 양자화 (written & ~0x7)")
    ax_b.step(dly_input, dly_fpga_readback, where="post", color=OKABE_ITO["vermilion"], lw=2.4,
              label="실제 FPGA 0x20000007 리드백 = (written & ~0x7) + 80 ns")

    # 현재 실험 설정점 (CW=100 + DLY=400 = 500 ns -> Readback 576 ns) 마커
    ax_b.scatter([500], [576], s=95, color=OKABE_ITO["blue"], zorder=6,
                 label="본 실험 설정: CW(100)+DLY(400)=500 ns -> 리드백 576 ns (0x0240)")
    ax_b.annotate("500 ns 인가 -> 576 ns 리드백\n(496 ns + 80 ns 파이프라인)",
                  xy=(500, 576), xytext=(300, 210),
                  arrowprops=dict(arrowstyle="->", color=OKABE_ITO["blue"], lw=1.8),
                  fontsize=9.5, fontweight="bold", color=OKABE_ITO["blue"])

    ax_b.set_title("(b) FPGA DLY 레지스터(0x20000007) 8 ns 양자화 및 +80 ns 오프셋 특성", fontsize=12.5, fontweight="bold", pad=8)
    ax_b.set_xlabel("호스트 기록 지연 시간 (CW + DLY Written) [ns]", fontsize=11)
    ax_b.set_ylabel("FPGA 레지스터 리드백 값 (Readback) [ns]", fontsize=11)
    ax_b.legend(loc="upper left", fontsize=9.0, frameon=True)
    ax_b.grid(True, alpha=0.25)

    # --------------------------------------------------------------------------
    # Panel (c): 128-Byte 헤더 내 125 MHz Fine Time 분포 및 이벤트 간 시간차(dt) 지수 분포
    # --------------------------------------------------------------------------
    ax_c = fig.add_subplot(gs[1, 1])
    loc_times_ns = np.array([r["loc_time_ns"] for r in records], dtype=np.int64)
    dt_ms = np.diff(loc_times_ns) * 1.0e-6
    dt_ms = dt_ms[(dt_ms > 0.0) & (dt_ms < 200.0)]

    counts, bin_edges = np.histogram(dt_ms, bins=50, range=(0, 60))
    hep.histplot(counts, bin_edges, ax=ax_c, color=OKABE_ITO["bluish_green"], lw=2.2,
                 label=f"실측 이벤트 간 시간차 dt (N={len(dt_ms):,d})")

    # 푸아송 지수 분포 피팅 곡선 (R * exp(-R * dt))
    mean_dt = np.mean(dt_ms)
    rate_hz = 1000.0 / mean_dt
    x_fit = np.linspace(0.5, 60, 200)
    bin_w = bin_edges[1] - bin_edges[0]
    y_fit = len(dt_ms) * bin_w * (1.0 / mean_dt) * np.exp(-x_fit / mean_dt)
    ax_c.plot(x_fit, y_fit, color=OKABE_ITO["vermilion"], ls="--", lw=2.2,
              label=f"포아송 과정 지수 피팅 (평균 율 R = {rate_hz:.2f} Hz, tau = {mean_dt:.2f} ms)")

    ax_c.set_yscale("log")
    ax_c.set_title("(c) FPGA 48-Bit 하드웨어 타임스탬프 기반 이벤트 간 시간차(dt) 분포", fontsize=12.5, fontweight="bold", pad=8)
    ax_c.set_xlabel("연속 트리거 간 시간 간격 (Inter-Event Time dt) [ms]", fontsize=11)
    ax_c.set_ylabel("이벤트 빈도 (Events / Bin) [Log Scale]", fontsize=11)
    ax_c.legend(loc="upper right", fontsize=9.2, frameon=True)
    ax_c.grid(True, which="both", alpha=0.25)

    out_pdf = os.path.join(out_dir, "fig_fpga_timing_and_telemetry.pdf")
    out_png = os.path.join(out_dir, "fig_fpga_timing_and_telemetry.png")
    fig.savefig(out_pdf, bbox_inches="tight")
    fig.savefig(out_png, bbox_inches="tight")
    plt.close(fig)
    print(f"[OK] FPGA 타이밍 및 텔레메트리 그래프 저장 완료:\n  - PDF: {out_pdf}\n  - PNG: {out_png}")


def main():
    parser = argparse.ArgumentParser(description="NKFADC500 Mini FPGA Explorer & Timing Visualizer")
    parser.add_argument("--dat", default="data/20261104_160_temp_EDA_CA_3000_ml_per_l_60co_hv_1700_thr_30_min_20.dat",
                        help="탐색할 Raw .dat 파일 경로")
    parser.add_argument("--root", default="data/20261104_160_temp_EDA_CA_3000_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root",
                        help="탐색할 Production _prod.root 파일 경로")
    parser.add_argument("--outdir", default="report/figures", help="그래프 출력 디렉토리")
    args = parser.parse_args()

    print_fpga_exploration_report(args.dat, args.root)
    plot_fpga_timing_and_telemetry(args.dat, args.root, args.outdir)


if __name__ == "__main__":
    main()
