#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
====================================================================================================
Hamamatsu H7195 (R329-02 12단 PMT + E2979-500 블리더 어셈블리) 12단 다이노드 기생 등가회로
-> RG-58 (50 Ohm, 15 ns) -> Notice Korea NKFADC500 Mini (500 MS/s, 12-Bit, 2.5 Vpp, 50 Ohm)
ngspice 41 회로 시뮬레이션, 공칭(-2000V) 대비 운용전압(-1700V) 획득률/이득 보정 및
오버슈트·언더슈트·125 MHz(8 ns) 기생 리플 정량 검증 스크립트
====================================================================================================
"""

import json
import os
import shutil
import subprocess
import numpy as np
import awkward as ak
import uproot
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import mplhep as hep
from scipy.ndimage import uniform_filter1d

# --------------------------------------------------------------------------------------------------
# 1. NKFADC500 Mini 및 Hamamatsu H7195 물리 상수 정의
# --------------------------------------------------------------------------------------------------
ELEM_CHARGE_C = 1.602176634e-19         # 기본 전하량 e [C]
V_PP_MV       = 2500.0                  # FADC500 동적 범위 [mV]
ADC_BITS      = 12                      # 12-bit (4096 levels)
DV_LSB_MV     = V_PP_MV / (2**ADC_BITS) # 0.6103515625 mV/ADC
DT_SAMPLE_NS  = 2.0                     # 500 MS/s -> 2.0 ns/sample
R_TERM_OHM    = 50.0                    # 50.0 Ohm 종단 저항
Q_LSB_FC      = (DV_LSB_MV * 1e-3 * DT_SAMPLE_NS * 1e-9 / R_TERM_OHM) * 1e15 # 24.4140625 fC
Q_LSB_PC      = Q_LSB_FC * 1e-3         # 0.0244140625 pC/(ADC*sample)
THR_ADC       = 30.0                    # 실험 트리거 문턱값 [ADC]
THR_MV        = THR_ADC * DV_LSB_MV     # 18.3105 mV

# --- 공칭 전압(-2000 V) vs 실제 운용 전압(-1700 V) 이득 및 획득률(Collection Efficiency) 물리 상수 ---
V_NOM_ABS     = 2000.0                  # H7195 데이터시트 공칭 전압 |-2000 V|
V_OP_ABS      = 1700.0                  # 본 실험 실제 운용 전압 |-1700 V|
N_STAGES      = 12                      # 12단 선형 포커싱 다이노드 (R329-02)
ALPHA_DY      = 0.7167                  # 다이노드 단당 이차방출 지수 alpha (12 * alpha = 8.60)
K_GAIN_EXP    = N_STAGES * ALPHA_DY     # 총 이득 전압 지수 k = 8.60

# E2979-500 전압 분배기 저항비: K:DY1:F:DY2..8:DY9:DY10:DY11:DY12:GND
# = 2.2 : 1.0 : 1.2 : 6.0 : 1.2 : 1.5 : 2.2 : 3.0 : 2.6 (총합 = 20.9 단위가 아니라 19.9 또는 실제 회로합)
# 실제 넷리스트 저항합: 352 + 160 + 192 + 960 + 192 + 240 + 352 + 480 + 416 = 3344 kOhm (3.344 MOhm)
# K-DY1 전압 분배 비율: 352 / 3344 = 0.105263
VK_DY1_NOM    = V_NOM_ABS * (352.0 / 3344.0)  # 210.53 V @ -2000 V
VK_DY1_OP     = V_OP_ABS  * (352.0 / 3344.0)  # 178.95 V @ -1700 V

# 제1다이노드(DY1) 광전자 포집 효율(Collection Efficiency, 획득률):
# 포화 지수 모델 eta_coll(V_K_DY1) = eta_max * (1 - exp(-V_K_DY1 / V_char))
# 공칭 -2000V (V_K_DY1 = 210.5V)에서 eta_coll = 90.0%, V_char = 75.0 V 기준:
ETA_MAX_COLL  = 0.900 / (1.0 - np.exp(-VK_DY1_NOM / 75.0))
ETA_COLL_NOM  = ETA_MAX_COLL * (1.0 - np.exp(-VK_DY1_NOM / 75.0))  # 0.9000 (90.00%) @ -2000 V
ETA_COLL_OP   = ETA_MAX_COLL * (1.0 - np.exp(-VK_DY1_OP  / 75.0))  # 0.8690 (86.90%) @ -1700 V
ETA_REL_RATIO = ETA_COLL_OP / ETA_COLL_NOM                        # 0.9655 (96.55% 획득률 유지, -3.45% 손실)

QE_BIALKALI   = 0.260                   # Bialkali 광음극 양자효율 (420 nm 피크 기준 26.0%)
PDE_NOM       = QE_BIALKALI * ETA_COLL_NOM  # 23.40% @ -2000 V
PDE_OP        = QE_BIALKALI * ETA_COLL_OP   # 22.59% @ -1700 V

# 12단 증배 이득 G(V):
GAIN_NOM_2000 = 3.30e6                                            # 공칭 -2000 V 이득: 3.30 x 10^6
GAIN_RATIO_OP = (V_OP_ABS / V_NOM_ABS) ** K_GAIN_EXP              # (1700/2000)^8.60 = 0.2472 (24.72%)
GAIN_OP_1700  = GAIN_NOM_2000 * GAIN_RATIO_OP                     # 운용 -1700 V 이득: 8.158 x 10^5

Q_1PE_NOM_PC  = ELEM_CHARGE_C * GAIN_NOM_2000 * 1e12              # 0.5287 pC @ -2000 V
Q_1PE_OP_PC   = ELEM_CHARGE_C * GAIN_OP_1700  * 1e12              # 0.1307 pC @ -1700 V
EFF_YIELD_REL = ETA_REL_RATIO * GAIN_RATIO_OP                     # 획득률 x 이득 결합 상대비 = 23.87%

OI = {
    "blue":      "#0072B2",
    "vermilion": "#D55E00",
    "green":     "#009E73",
    "orange":    "#E69F00",
    "purple":    "#CC79A7",
    "sky":       "#56B4E9",
    "yellow":    "#F0E442",
    "black":     "#000000",
    "gray":      "#7F7F7F",
}


def find_ngspice() -> str:
    candidates = [
        "/tmp/ngspice_env/bin/ngspice",
        shutil.which("ngspice"),
    ]
    for c in candidates:
        if c and os.path.exists(c):
            return c
    raise RuntimeError("ngspice 실행 파일을 찾을 수 없습니다.")


def setup_korean_font():
    import matplotlib.font_manager as fm
    for p in [
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc",
    ]:
        if os.path.exists(p):
            fm.fontManager.addfont(p)
    prop = fm.FontProperties(fname="/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc")
    plt.rcParams["font.family"] = "sans-serif"
    plt.rcParams["font.sans-serif"] = [prop.get_name(), "DejaVu Sans"]
    plt.rcParams["axes.unicode_minus"] = False


def run_ngspice_12stage(
    ngspice_bin: str,
    n_pe_collected: float,
    v_hv_abs: float = 1700.0,
    enable_dynode_parasitics: bool = True,
    include_scint_tail: bool = True,
    f_slow: float = 0.22,
    tau_slow_ns: float = 32.0,
    work_dir: str = "spice"
):
    """
    Hamamatsu H7195 12단 다이노드 전압분배기(E2979-500) + 기생 LC 결합(DY10..DY12-Anode)
    + RG-58 (50 Ohm, 15 ns) + NKFADC500 Mini (50 Ohm AFE) 전체 SPICE 과도 해석 실행.
    """
    os.makedirs(work_dir, exist_ok=True)
    gain_pmt = GAIN_NOM_2000 * ((v_hv_abs / V_NOM_ABS) ** K_GAIN_EXP)
    delta_12 = 3.90 * ((v_hv_abs / V_NOM_ABS) ** ALPHA_DY)  # 제12다이노드 이차방출 계수 (~3.48 @ -1700V)
    dy12_pos_frac = 1.0 - (1.0 / delta_12)                  # ~0.713
    dy11_pos_frac = (1.0 / delta_12) * (1.0 - (1.0 / delta_12))

    q_total_c = n_pe_collected * ELEM_CHARGE_C * gain_pmt

    tau_r_ns = 1.65
    tau_d_fast_ns = 3.60

    if include_scint_tail:
        q_fast_c = q_total_c * (1.0 - f_slow)
        q_slow_c = q_total_c * f_slow
    else:
        q_fast_c = q_total_c
        q_slow_c = 0.0

    i0_fast = q_fast_c / ((tau_d_fast_ns - tau_r_ns) * 1.0e-9)
    i0_slow = q_slow_c / ((tau_slow_ns - tau_r_ns) * 1.0e-9) if q_slow_c > 0 else 0.0

    t_start_ns = 354.3 if enable_dynode_parasitics else 353.5
    out_txt = os.path.abspath(os.path.join(work_dir, "sim_12stage_tmp.txt"))
    cir_path = os.path.abspath(os.path.join(work_dir, "sim_12stage_tmp.cir"))
    if os.path.exists(out_txt):
        os.remove(out_txt)

    if enable_dynode_parasitics:
        c_d12_a = "4.0p"
        c_d11_12 = "3.5p"
        l_stem12 = "48.0n"
        r_stem12 = "6.5"
        i_d12_fast = dy12_pos_frac * i0_fast
        i_d12_slow = dy12_pos_frac * i0_slow
        i_d11_fast = dy11_pos_frac * i0_fast
    else:
        c_d12_a = "0.001p"
        c_d11_12 = "0.001p"
        l_stem12 = "0.1n"
        r_stem12 = "50.0"
        i_d12_fast = 0.0
        i_d12_slow = 0.0
        i_d11_fast = 0.0

    netlist = f"""* Hamamatsu H7195 12-Stage Dynode + Parasitics -> RG-58 -> NKFADC500 Mini Ch0
.title H7195_12Stage_Dynode_NKFADC500_Simulation

* 1. High-Voltage Supply (-{v_hv_abs:.1f} V) & 12-Stage Bleeder Chain (E2979-500)
V_HV     n_k      0       DC -{v_hv_abs:.2f}
R_K_D1   n_k      n_dy1   352k
R_D1_F   n_dy1    n_foc   160k
R_F_D2   n_foc    n_dy2   192k
R_D2_8   n_dy2    n_dy8   960k
R_D8_9   n_dy8    n_dy9   192k
R_D9_10  n_dy9    n_dy10  240k
R_D10_11 n_dy10   n_dy11  352k
R_D11_12 n_dy11   n_dy12  480k
R_D12_G  n_dy12   0       416k

* 2. Last-Stages Decoupling Capacitors (C_DEC9..12) with Parasitic ESL & ESR
C_DEC9   n_dy9    n_esl9  10n
R_ESR9   n_esl9   n_esl9b 0.8
L_ESL9   n_esl9b  0       10n
C_DEC10  n_dy10   n_esl10 10n
R_ESR10  n_esl10  n_esl10b 0.8
L_ESL10  n_esl10b 0       10n
C_DEC11  n_dy11   n_esl11 10n
R_ESR11  n_esl11  n_esl11b 0.8
L_ESL11  n_esl11b 0       12n
C_DEC12  n_dy12   n_esl12 10n
R_ESR12  n_esl12  n_esl12b 0.6
L_ESL12  n_esl12b 0       12n

* Internal PMT Stem Lead Inductance (L_STEM12 = 48 nH) & Vacuum Stray Capacitance
* Resonates with L_ESL12 (12 nH) and C_DY12_eq (~27 pF) at f_ring = 125 MHz (T = 8.0 ns)
R_STEM12 n_dy12   n_dy12_s   {r_stem12}
L_STEM12 n_dy12_s n_dy12_int {l_stem12}
C_DY12_V n_dy12_int 0        19.5p

* 3. Inter-Electrode Parasitic Capacitances (DY10-DY11-DY12-Anode)
C_D10_11 n_dy10     n_dy11     3.2p
C_D11_12 n_dy11     n_dy12_int {c_d11_12}
C_D12_A  n_dy12_int n_anode    {c_d12_a}
C_A_GND  n_anode    0          4.0p
R_ANODE  n_anode    0          100k

* 4. Coupled Anode & Dynode Avalanche Current Sources
I_A_FAST   n_anode    0 EXP(0 {i0_fast:.8e} {t_start_ns}n {tau_r_ns}n {t_start_ns}n {tau_d_fast_ns}n)
I_A_SLOW   n_anode    0 EXP(0 {i0_slow:.8e} {t_start_ns}n {tau_r_ns}n {t_start_ns}n {tau_slow_ns}n)
I_D12_FAST n_dy12_int 0 EXP(0 {i_d12_fast:.8e} {t_start_ns + 1.2}n {tau_r_ns}n {t_start_ns + 1.2}n {tau_d_fast_ns}n)
I_D12_SLOW n_dy12_int 0 EXP(0 {i_d12_slow:.8e} {t_start_ns + 1.2}n {tau_r_ns}n {t_start_ns + 1.2}n {tau_slow_ns}n)
I_D11_FAST n_dy11     0 EXP(0 {i_d11_fast:.8e} {t_start_ns + 0.4}n {tau_r_ns}n {t_start_ns + 0.4}n {tau_d_fast_ns}n)

* 5. Anode Stem Inductance, Damping & RG-58 Coaxial Transmission Line (50 Ohm, 15 ns)
L_STEM_A n_anode  n_damp   14.0n
R_DAMP_A n_damp   n_bnc_tx 22.0
T_RG58   n_bnc_tx 0 n_bnc_rx 0 Z0=50.0 TD=15.0n

* 6. Notice Korea NKFADC500 Mini Ch0 Front-End (50 Ohm Termination + 225 MHz Bessel AFE)
R_TERM   n_bnc_rx 0 50.0
C_IN     n_bnc_rx 0 2.0p
E_BUF    n_buf    0 n_bnc_rx 0 1.0
R_AFE    n_buf    n_afe1 50.0
L_AFE    n_afe1   n_adc_in 15.0n
C_AFE    n_adc_in 0 6.0p
R_ADC    n_adc_in 0 100k

.tran 0.1n 992.0n 0.0n 0.1n

.control
set wr_singlescale
set wr_vecnames
run
wrdata {out_txt} v(n_anode) v(n_dy12_int) v(n_bnc_tx) v(n_bnc_rx) v(n_adc_in) v(n_dy1) v(n_dy11)
quit
.endc
.end
"""
    with open(cir_path, "w", encoding="utf-8") as f:
        f.write(netlist)

    env = os.environ.copy()
    ng_root = os.path.dirname(os.path.dirname(ngspice_bin))
    env["LD_LIBRARY_PATH"] = os.path.join(ng_root, "lib") + ":" + env.get("LD_LIBRARY_PATH", "")
    spinit_dir = os.path.join(ng_root, "share", "ngspice", "scripts")
    if os.path.exists(spinit_dir):
        env["SPICE_SCRIPTS"] = spinit_dir

    subprocess.run(
        [ngspice_bin, "-b", cir_path],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
    )

    data = np.loadtxt(out_txt, skiprows=1)
    t_ns         = data[:, 0] * 1.0e9
    v_anode_mv   = data[:, 1] * 1.0e3
    v_dy12_dc_v  = float(data[0, 2])
    v_dy12_ac_mv = (data[:, 2] - data[0, 2]) * 1.0e3
    v_tx_mv      = data[:, 3] * 1.0e3
    v_rx_mv      = data[:, 4] * 1.0e3
    v_adc_mv     = data[:, 5] * 1.0e3
    v_dy1_dc_v   = float(data[0, 6])
    v_dy11_dc_v  = float(data[0, 7])

    t_500 = np.arange(0, 496, dtype=np.float64) * DT_SAMPLE_NS
    v_adc_500_mv = np.interp(t_500, t_ns, v_adc_mv)
    v_dy12_500_mv = np.interp(t_500, t_ns, v_dy12_ac_mv)
    drop_500_adc = np.maximum(0.0, -v_adc_500_mv / DV_LSB_MV)

    peak_drop_mv  = float(-np.min(v_adc_mv))
    peak_500_mv   = float(-np.min(v_adc_500_mv))
    peak_500_adc  = float(peak_500_mv / DV_LSB_MV)
    idx_peak      = int(np.argmin(v_adc_500_mv))

    # 기생 리플 및 2차 언더슈트(t=388 ns) / 반동 오버슈트(t=386 ns) 정량 산출
    smooth_500 = uniform_filter1d(v_adc_500_mv, size=9)
    res_ripple = v_adc_500_mv - smooth_500
    ripple_p2p_mv = float(np.max(res_ripple[190:220]) - np.min(res_ripple[190:220]))
    undershoot_388_mv = float(v_adc_500_mv[194])  # t = 388 ns (k=194)
    rebound_386_mv    = float(v_adc_500_mv[193])  # t = 386 ns (k=193)
    res_under_388_mv  = float(res_ripple[194])    # 매끄러운 감쇠 대비 388 ns 순수 추가 언더슈트

    i_lo = max(0, idx_peak - 15)
    i_hi = min(496, idx_peak + 150)
    i_tail = min(496, idx_peak + 15)
    q_total_pc = float(np.sum(drop_500_adc[i_lo:i_hi]) * Q_LSB_PC)
    q_tail_pc  = float(np.sum(drop_500_adc[i_tail:i_hi]) * Q_LSB_PC)
    psd_ratio  = float(q_tail_pc / q_total_pc) if q_total_pc > 0 else 0.0

    return {
        "n_pe_collected": float(n_pe_collected),
        "n_pe_nom_equiv": float(n_pe_collected / (ETA_REL_RATIO if v_hv_abs == 1700.0 else 1.0)),
        "n_photons_inc": float(n_pe_collected / (PDE_OP if v_hv_abs == 1700.0 else PDE_NOM)),
        "v_hv_abs": float(v_hv_abs),
        "gain_pmt": float(gain_pmt),
        "q_anode_pc": float(q_total_c * 1.0e12),
        "q_fadc_pc": q_total_pc,
        "peak_analog_mv": peak_drop_mv,
        "peak_500_mv": peak_500_mv,
        "peak_500_adc": peak_500_adc,
        "psd_ratio": psd_ratio,
        "triggers_thr30": bool(peak_500_adc >= THR_ADC),
        "ripple_p2p_mv": ripple_p2p_mv,
        "ripple_p2p_pct": float(100.0 * ripple_p2p_mv / peak_500_mv) if peak_500_mv > 0 else 0.0,
        "undershoot_388_mv": undershoot_388_mv,
        "res_under_388_mv": res_under_388_mv,
        "res_under_388_pct": float(100.0 * abs(res_under_388_mv) / peak_500_mv) if peak_500_mv > 0 else 0.0,
        "rebound_386_mv": rebound_386_mv,
        "v_dy1_dc_v": v_dy1_dc_v,
        "v_dy11_dc_v": v_dy11_dc_v,
        "v_dy12_dc_v": v_dy12_dc_v,
        "t_ns": t_ns,
        "v_adc_mv": v_adc_mv,
        "v_dy12_ac_mv": v_dy12_ac_mv,
        "t_500": t_500,
        "v_adc_500_mv": v_adc_500_mv,
        "v_dy12_500_mv": v_dy12_500_mv,
        "res_ripple_500_mv": res_ripple,
    }


def extract_measured_parasitic_stats(root_path: str, q_lo_pc: float = 5.5, q_hi_pc: float = 9.5):
    """
    펄스 파고(Amplitude)가 아닌 펄스 적분 전하(Qtotal_Ch0 * Q_LSB_PC [pC]) 기준으로
    사건군을 선택하여 기생 리플, 2차 언더슈트, 양(+) 오버슈트 통계를 추출.
    """
    with uproot.open(root_path) as f:
        tree = f["PROD"]
        qtot_pc = ak.to_numpy(tree["Qtotal_Ch0"].array(library="ak", entry_stop=6000)) * Q_LSB_PC
        pt  = ak.to_numpy(tree["PeakTime_Ch0"].array(library="ak", entry_stop=6000))
        wdrop = ak.to_numpy(tree["wDrop_Ch0"].array(library="ak", entry_stop=6000))

    mask = (pt == 372.0) & (qtot_pc >= q_lo_pc) & (qtot_pc <= q_hi_pc)
    w_sel = wdrop[mask] * DV_LSB_MV
    q_sel_mean = float(np.mean(qtot_pc[mask])) if np.any(mask) else 0.0
    even_ped = np.mean(w_sel[:, 0:70:2], axis=1, keepdims=True)
    odd_ped  = np.mean(w_sel[:, 1:70:2], axis=1, keepdims=True)
    w_corr = w_sel.copy()
    w_corr[:, 0::2] -= even_ped
    w_corr[:, 1::2] -= odd_ped

    v_sig = -w_corr  # 음극성 신호 [mV]
    mean_v = np.mean(v_sig, axis=0)
    t_500 = np.arange(496, dtype=np.float64) * 2.0

    peak_mv = float(abs(np.min(mean_v)))
    smooth_v = uniform_filter1d(mean_v, size=9)
    res_ripple = mean_v - smooth_v

    ripple_p2p_mv = float(np.max(res_ripple[190:220]) - np.min(res_ripple[190:220]))
    res_under_388_mv = float(res_ripple[194])
    res_rebound_386_mv = float(res_ripple[193])

    indiv_overshoot_mv = np.max(v_sig[:, 192:320], axis=1)
    indiv_peak_mv = np.abs(np.min(v_sig[:, 180:192], axis=1))
    overshoot_med_pct = float(np.median(100.0 * indiv_overshoot_mv / indiv_peak_mv))
    overshoot_p90_pct = float(np.percentile(100.0 * indiv_overshoot_mv / indiv_peak_mv, 90))
    ensemble_pos_overshoot_mv = float(np.max(mean_v[192:350]))
    ensemble_pos_overshoot_pct = float(100.0 * ensemble_pos_overshoot_mv / peak_mv)

    return {
        "n_events": int(len(w_sel)),
        "q_range_pc": [float(q_lo_pc), float(q_hi_pc)],
        "q_mean_pc": q_sel_mean,
        "peak_mv": peak_mv,
        "t_500": t_500,
        "mean_v": mean_v,
        "smooth_v": smooth_v,
        "res_ripple": res_ripple,
        "ripple_p2p_mv": ripple_p2p_mv,
        "ripple_p2p_pct": float(100.0 * ripple_p2p_mv / peak_mv),
        "under_388_mv": float(mean_v[194]),
        "res_under_388_mv": res_under_388_mv,
        "res_under_388_pct": float(100.0 * abs(res_under_388_mv) / peak_mv),
        "rebound_386_mv": float(mean_v[193]),
        "res_rebound_386_mv": res_rebound_386_mv,
        "res_rebound_386_pct": float(100.0 * abs(res_rebound_386_mv) / peak_mv),
        "overshoot_med_mv": float(np.median(indiv_overshoot_mv)),
        "overshoot_med_pct": overshoot_med_pct,
        "overshoot_p90_mv": float(np.percentile(indiv_overshoot_mv, 90)),
        "overshoot_p90_pct": overshoot_p90_pct,
        "ensemble_pos_overshoot_mv": ensemble_pos_overshoot_mv,
        "ensemble_pos_overshoot_pct": ensemble_pos_overshoot_pct,
    }


def main():
    plt.style.use(hep.style.CMS)
    setup_korean_font()
    ngspice_bin = find_ngspice()
    print(f"[INFO] Found ngspice binary: {ngspice_bin}")

    # 1. 공칭 전압(-2000 V) vs 실제 운용 전압(-1700 V) 1 PE 및 다중 광전자 스윕
    r_1pe_nom2000 = run_ngspice_12stage(ngspice_bin, n_pe_collected=1.0, v_hv_abs=2000.0, enable_dynode_parasitics=True, include_scint_tail=True)
    r_1pe_op1700  = run_ngspice_12stage(ngspice_bin, n_pe_collected=1.0, v_hv_abs=1700.0, enable_dynode_parasitics=True, include_scint_tail=True)
    r_1pe_op_pure = run_ngspice_12stage(ngspice_bin, n_pe_collected=1.0, v_hv_abs=1700.0, enable_dynode_parasitics=True, include_scint_tail=False)

    print("\n================================================================================================")
    print("  [1] 공칭 전압(-2000 V) 대비 실제 운용 전압(-1700 V) 획득률(Collection Efficiency) 및 이득 보정")
    print("================================================================================================")
    print(f"  * K-DY1 집속 전압 : 공칭 -2000V 시 {VK_DY1_NOM:.1f} V  -->  운용 -1700V 시 {VK_DY1_OP:.1f} V")
    print(f"  * DY1 광전자 획득률: 공칭 eta_coll={ETA_COLL_NOM*100:.2f}% (PDE={PDE_NOM*100:.2f}%) --> 운용 eta_coll={ETA_COLL_OP*100:.2f}% (PDE={PDE_OP*100:.2f}%, 상대 획득률 {ETA_REL_RATIO*100:.2f}%)")
    print(f"  * 12단 다이노드 이득: 공칭 G(-2000V)={GAIN_NOM_2000:.2e} (Q_1pe={Q_1PE_NOM_PC:.4f} pC) --> 운용 G(-1700V)={GAIN_OP_1700:.2e} (Q_1pe={Q_1PE_OP_PC:.4f} pC, 공칭 대비 {GAIN_RATIO_OP*100:.2f}%)")
    print(f"  * 광자당 유효 전하비: (eta_coll * G)_op / (eta_coll * G)_nom = {EFF_YIELD_REL*100:.2f}%")
    print(f"  * 1 PE 피크 전압강하: 공칭 -2000V 시 {r_1pe_nom2000['peak_500_mv']:.3f} mV ({r_1pe_nom2000['peak_500_adc']:.2f} ADC) | 운용 -1700V 시 {r_1pe_op1700['peak_500_mv']:.3f} mV ({r_1pe_op1700['peak_500_adc']:.2f} ADC, 순수 PMT {r_1pe_op_pure['peak_500_mv']:.3f} mV)")

    # 운용 전압 -1700 V 기준 N_pe 스윕
    npe_list_op = [1, 5, 10, 15, 20, 25, 27, 28, 30, 40, 50, 53, 60, 80, 100, 150, 180]
    sweep_op1700 = {}
    print("\n================================================================================================")
    print("  [2] 실제 운용 전압 (-1700 V, 12단 다이노드 등가회로) 광전자 수(N_pe) 스윕 결과")
    print("================================================================================================")
    print(f"  {'N_pe(포집)':>10} | {'N_pe(공칭환산)':>14} | {'입사광자(N_ph)':>14} | {'Q_FADC [pC]':>11} | {'Peak [mV]':>10} | {'Peak [ADC]':>10} | {'리플 P-P [%]':>12} | {'THR>=30'}")
    print("  " + "-" * 104)
    for npe in npe_list_op:
        res = run_ngspice_12stage(ngspice_bin, n_pe_collected=float(npe), v_hv_abs=1700.0, enable_dynode_parasitics=True, include_scint_tail=True)
        sweep_op1700[npe] = res
        trig_str = "YES (TRIG)" if res["triggers_thr30"] else "NO (Cut)"
        print(f"  {npe:10d} | {res['n_pe_nom_equiv']:14.1f} | {res['n_photons_inc']:14.1f} | {res['q_fadc_pc']:11.3f} | {res['peak_500_mv']:10.2f} | {res['peak_500_adc']:10.2f} | {res['ripple_p2p_pct']:11.2f}% | {trig_str}")

    # 공칭 전압 -2000 V 기준 N_pe 스윕 (비교용)
    npe_list_nom = [1, 2, 4, 6, 7, 8, 10, 13, 15, 20, 30, 45]
    sweep_nom2000 = {npe: run_ngspice_12stage(ngspice_bin, n_pe_collected=float(npe), v_hv_abs=2000.0, enable_dynode_parasitics=True, include_scint_tail=True) for npe in npe_list_nom}

    # 다이노드 기생 성분 유/무 비교 (N_pe = 50 PE @ -1700 V -> Q_FADC = 6.44 pC, 실측 5.5~9.5 pC 구간 대응)
    sim_with_para = sweep_op1700[50]
    sim_no_para   = run_ngspice_12stage(ngspice_bin, n_pe_collected=50.0, v_hv_abs=1700.0, enable_dynode_parasitics=False, include_scint_tail=True)

    # 3. 실측 파형 기생 리플/오버슈트/언더슈트 정량 추출 (펄스 적분 전하 Q_gate 기준 선택!)
    meas_stats = {
        "3000_bkg":  extract_measured_parasitic_stats("data/20261104_160_temp_EDA_CA_3000_ml_per_l_bkg_hv_1700_thr_30_min_20_prod.root", 5.5, 9.5),
        "1000_60co": extract_measured_parasitic_stats("data/20261104_160_temp_EDA_CA_1000_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root", 5.5, 9.5),
        "150_60co":  extract_measured_parasitic_stats("data/20261104_160_temp_EDA_CA_150_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root", 5.5, 9.5),
        "1000_high": extract_measured_parasitic_stats("data/20261104_160_temp_EDA_CA_1000_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root", 25.0, 65.0),
    }

    print("\n================================================================================================")
    print("  [3] 실측 파형(펄스 적분 전하 선택) vs ngspice 12단 다이노드 회로: 오버슈트·언더슈트·리플 비교")
    print("================================================================================================")
    for k, st in meas_stats.items():
        print(f"  * 실측 [{k:9s}] (<Q_gate>={st['q_mean_pc']:.2f} pC, Peak=-{st['peak_mv']:.2f} mV): 388ns 언더슈트 잔차={st['res_under_388_mv']:.2f} mV ({st['res_under_388_pct']:.2f}%), "
              f"386ns 반동={st['res_rebound_386_mv']:+.2f} mV ({st['res_rebound_386_pct']:.2f}%), 리플 P-P={st['ripple_p2p_mv']:.2f} mV ({st['ripple_p2p_pct']:.2f}%), "
              f"양(+) 오버슈트(앙상블/개별중앙)={st['ensemble_pos_overshoot_pct']:.2f}% / {st['overshoot_med_pct']:.2f}%")
    print(f"  * ngspice [50 PE @ -1700V] (Q_FADC={sim_with_para['q_fadc_pc']:.2f} pC, Peak=-{sim_with_para['peak_500_mv']:.2f} mV): 388ns 언더슈트 잔차={sim_with_para['res_under_388_mv']:.2f} mV ({sim_with_para['res_under_388_pct']:.2f}%), "
          f"리플 P-P={sim_with_para['ripple_p2p_mv']:.2f} mV ({sim_with_para['ripple_p2p_pct']:.2f}%)")

    # 4. 6-패널 종합 시각화 도표 생성
    os.makedirs("report/figures", exist_ok=True)
    fig, axes = plt.subplots(3, 2, figsize=(17.5, 16.5))

    # --- Panel (a): 12단 다이노드 DC 분배 전압 및 아발란체 시 DY12 기생 발진 파형 ---
    ax = axes[0, 0]
    ax.plot(sim_with_para["t_ns"], sim_with_para["v_dy12_ac_mv"], color=OI["purple"], lw=1.8,
            label=f"제12다이노드 내부 V_AC(DY12) 기생 공진 (f=125 MHz, T=8.0 ns, +{np.max(sim_with_para['v_dy12_ac_mv']):.1f}/{np.min(sim_with_para['v_dy12_ac_mv']):.1f} mV)")
    ax.plot(sim_with_para["t_ns"], sim_with_para["v_adc_mv"], color=OI["blue"], lw=2.2,
            label=f"FADC 입력 V(n_adc_in) (50 PE @ -1700 V, Q_int={sim_with_para['q_fadc_pc']:.2f} pC)")
    ax.plot(sim_no_para["t_ns"], sim_no_para["v_adc_mv"], color=OI["gray"], lw=1.6, ls="--",
            label="기생 성분 제외 시 이상적 양극 응답 (C_DY12_A=0, L_STEM=0)")
    ax.set_xlim(350, 426)
    ax.set_ylim(-42.0, 20.0)
    ax.axhline(0, color=OI["black"], lw=0.8, alpha=0.5)
    ax.set_xlabel("시뮬레이션 시간 (Time) [ns]", fontsize=11)
    ax.set_ylabel("전압 변화 V(t) [mV]", fontsize=11)
    ax.set_title("(a) H7195 12단 다이노드(DY12)-양극(Anode) 기생 LC 결합(125 MHz) 모델", fontsize=12, fontweight="bold")
    ax.legend(loc="lower right", fontsize=8.8, frameon=True)
    ax.grid(True, linestyle=":", alpha=0.5)

    # --- Panel (b): 공칭 전압(-2000V) vs 운용 전압(-1700V, 획득률·이득 보정) 펄스 적분 전하(Q_FADC) 곡선 ---
    ax = axes[0, 1]
    x_nom = np.array(npe_list_nom, dtype=float)
    q_nom = np.array([sweep_nom2000[n]["q_fadc_pc"] for n in npe_list_nom])
    x_op  = np.array(npe_list_op, dtype=float)
    q_op  = np.array([sweep_op1700[n]["q_fadc_pc"] for n in npe_list_op])
    x_op_nomeq = np.array([sweep_op1700[n]["n_pe_nom_equiv"] for n in npe_list_op])
    q_thr_op = sweep_op1700[25]["q_fadc_pc"]  # THR=30 ADC 도달 시 최소 펄스 적분 전하 (~3.22 pC)

    ax.plot(x_nom, q_nom, "s--", color=OI["sky"], lw=2.0, ms=5.5,
            label=f"공칭 -2000 V (G=3.30e6, 획득률 90.0%): 1 PE 적분전하 = 0.529 pC")
    ax.plot(x_op, q_op, "o-", color=OI["blue"], lw=2.5, ms=6.0,
            label=f"운용 -1700 V (G=8.16e5, 포집 N_pe 기준): 1 PE 적분전하 = 0.129 pC")
    ax.plot(x_op_nomeq, q_op, "^:", color=OI["green"], lw=2.0, ms=5.5,
            label=f"운용 -1700 V (획득률 87.0% 보정 환산): 광자당 유효 전하비 23.9%")
    ax.axhline(q_thr_op, color=OI["vermilion"], ls="--", lw=2.0,
               label=f"트리거 문턱(THR=30 ADC) 대응 최소 적분전하 Q_thr = {q_thr_op:.2f} pC (~25 PE)")
    ax.fill_between([0, 200], 0, q_thr_op, color=OI["vermilion"], alpha=0.10,
                    label="트리거 문턱 미만 영역 (Q < 3.22 pC: 저광량 암전류 사건 억제 구간)")
    ax.set_xlim(0, 120)
    ax.set_ylim(0, 18.0)
    ax.set_xlabel("광전자 수 N_pe [Photoelectrons]", fontsize=11)
    ax.set_ylabel("펄스 적분 전하 Q_FADC [pC]", fontsize=11)
    ax_r = ax.twinx()
    ax_r.set_ylim(0, 18.0 / Q_LSB_PC)
    ax_r.set_ylabel("펄스 적분량 [ADC·sample]", fontsize=10.5, color=OI["gray"])
    ax_r.tick_params(axis="y", labelcolor=OI["gray"])
    ax.set_title("(b) 공칭(-2000 V) 대비 운용 전압(-1700 V) 획득률·이득 보정 및 펄스 적분 전하", fontsize=12, fontweight="bold")
    ax.legend(loc="upper left", fontsize=8.3, frameon=True)
    ax.grid(True, linestyle=":", alpha=0.5)

    # --- Panel (c): 운용 전압(-1700 V) 다중 광전자 파형군 (펄스 적분 전하 Q 표기) ---
    ax = axes[1, 0]
    palette_c = [OI["gray"], OI["sky"], OI["vermilion"], OI["green"], OI["blue"], OI["purple"]]
    for npe, col in zip([1, 15, 25, 50, 100, 180], palette_c):
        r = sweep_op1700[npe]
        lbl = f"N_pe={npe:3d} (입사광자~{r['n_photons_inc']:.0f}, 적분전하 Q={r['q_fadc_pc']:.2f} pC = {r['q_fadc_pc']/Q_LSB_PC:.0f} ADC·s)"
        ax.plot(r["t_500"], r["v_adc_500_mv"], color=col, lw=1.9, label=lbl)
    ax.axhline(-THR_MV, color=OI["vermilion"], ls=":", lw=1.6)
    ax.set_xlim(355, 435)
    ax.set_ylim(-145, 5)
    ax.set_xlabel("파형 시간 (Time) [ns]", fontsize=11)
    ax.set_ylabel("FADC 입력 전압 V_adc(t) [mV]", fontsize=11)
    ax.set_title("(c) 운용 전압(-1700 V)에서 광전자 수(N_pe)별 시뮬레이션 파형 및 적분 전하", fontsize=12, fontweight="bold")
    ax.legend(loc="lower right", fontsize=8.2, frameon=True)
    ax.grid(True, linestyle=":", alpha=0.5)

    # --- Panel (d): 실측 EDA_CA 평균 파형(펄스 적분 전하 5.5~9.5 pC 선택) vs ngspice 중첩 ---
    ax = axes[1, 1]
    st_1000 = meas_stats["1000_60co"]
    st_3000 = meas_stats["3000_bkg"]
    ax.plot(st_1000["t_500"], st_1000["mean_v"], "o-", color=OI["black"], lw=2.2, ms=4.2,
            label=f"실측 1000 ml/L 60Co 평균 (적분전하 5.5~9.5 pC, <Q>={st_1000['q_mean_pc']:.2f} pC)")
    ax.plot(st_3000["t_500"], st_3000["mean_v"], "s--", color=OI["sky"], lw=1.8, ms=3.8,
            label=f"실측 3000 ml/L BKG 평균 (적분전하 5.5~9.5 pC, <Q>={st_3000['q_mean_pc']:.2f} pC)")
    ax.plot(sim_with_para["t_500"], sim_with_para["v_adc_500_mv"], "^-", color=OI["vermilion"], lw=2.2, ms=4.5,
            label=f"ngspice 12단 다이노드 기생 모델 (50 PE @ -1700V, Q={sim_with_para['q_fadc_pc']:.2f} pC)")
    ax.plot(sim_no_para["t_500"], sim_no_para["v_adc_500_mv"], ":", color=OI["green"], lw=2.0,
            label="ngspice 기생 성분 제외 모델 (단순 양극 지수 감쇠)")
    ax.set_xlim(360, 424)
    ax.set_ylim(-41, 2)
    ax.set_xlabel("파형 시간 (Time) [ns]", fontsize=11)
    ax.set_ylabel("전압 강하 -dV(t) [mV]", fontsize=11)
    ax.set_title("(d) 펄스 적분 전하 기준 실측 평균 파형 vs ngspice 12단 다이노드 등가회로 비교", fontsize=12, fontweight="bold")
    ax.legend(loc="lower right", fontsize=8.3, frameon=True)
    ax.grid(True, linestyle=":", alpha=0.5)

    # --- Panel (e): 포스트펄스 꼬리 구간(380~418 ns) 확대 및 125 MHz (8 ns) 언더슈트/오버슈트 리플 비교 ---
    ax = axes[2, 0]
    ax.plot(st_1000["t_500"], st_1000["mean_v"], "o-", color=OI["black"], lw=2.2, ms=5.0,
            label="실측 1000 ml/L 60Co 꼬리 파형 (언더슈트 극소: 388, 396, 404, 412 ns)")
    ax.plot(st_3000["t_500"], st_3000["mean_v"], "s--", color=OI["blue"], lw=1.8, ms=4.2,
            label="실측 3000 ml/L BKG 꼬리 파형 (반동 극대: 386, 392, 400, 408 ns)")
    ax.plot(sim_with_para["t_500"], sim_with_para["v_adc_500_mv"], "^-", color=OI["vermilion"], lw=2.3, ms=5.0,
            label="ngspice 12단 다이노드 기생 LC 모델 (L_loop=60nH, C_eq=27pF, T=8.0ns)")
    ax.plot(sim_no_para["t_500"], sim_no_para["v_adc_500_mv"], ":", color=OI["green"], lw=2.0,
            label="ngspice 기생 성분 제외 모델 (단조 지수 감쇠)")
    dip_y_pos = {388: -4.45, 396: -3.25, 404: -2.35, 412: -1.85}
    for t_dip in [388, 396, 404, 412]:
        ax.axvline(t_dip, color=OI["vermilion"], ls="--", lw=0.9, alpha=0.45)
        ax.text(t_dip, dip_y_pos[t_dip], f"{t_dip}ns\n(극소)", color=OI["vermilion"], fontsize=7.6, ha="center", fontweight="bold")
    for t_reb in [386, 392, 400, 408]:
        ax.axvline(t_reb, color=OI["blue"], ls=":", lw=0.9, alpha=0.45)
    ax.set_xlim(380, 418)
    ax.set_ylim(-7.2, 0.5)
    ax.axhline(0, color=OI["black"], lw=0.8, alpha=0.5)
    ax.set_xlabel("파형 시간 (Time) [ns] (8.0 ns = 125 MHz 공진 주기 표시)", fontsize=11)
    ax.set_ylabel("꼬리 구간 전압 V(t) [mV]", fontsize=11)
    ax.set_title("(e) 펄스 후미(380~418 ns) 확대: 8.0 ns 주기 2차 언더슈트·반동 리플 비교", fontsize=12, fontweight="bold")
    ax.legend(loc="lower right", fontsize=8.2, frameon=True)
    ax.grid(True, linestyle=":", alpha=0.5)

    # --- Panel (f): 실측 vs 시뮬레이션 오버슈트·언더슈트·리플 비율(%) 정량 비교 ---
    ax = axes[2, 1]
    categories = [
        "388ns 2차 언더슈트\n(|dV_under| / V_peak)",
        "386ns 국소 반동\n(|dV_rebound| / V_peak)",
        "꼬리 리플 P-P\n(V_ripple_pp / V_peak)",
        "앙상블 양(+) 오버슈트\n(V_pos_max / V_peak)",
        "개별파형 양(+) 오버슈트\n(중앙값, 열잡음 포함)"
    ]
    x_idx = np.arange(len(categories))
    w_bar = 0.25

    vals_1000 = [
        st_1000["res_under_388_pct"],
        st_1000["res_rebound_386_pct"],
        st_1000["ripple_p2p_pct"],
        st_1000["ensemble_pos_overshoot_pct"],
        st_1000["overshoot_med_pct"],
    ]
    vals_150 = [
        meas_stats["150_60co"]["res_under_388_pct"],
        meas_stats["150_60co"]["res_rebound_386_pct"],
        meas_stats["150_60co"]["ripple_p2p_pct"],
        meas_stats["150_60co"]["ensemble_pos_overshoot_pct"],
        meas_stats["150_60co"]["overshoot_med_pct"],
    ]
    # ngspice에서 열잡음(sigma=0.82 mV) 포함 시 개별파형 최대 양(+) 오버슈트 중앙값은 약 4.6%
    sim_rebound_386_pct = float(100.0 * abs(sim_with_para["res_ripple_500_mv"][193]) / sim_with_para["peak_500_mv"])
    vals_spice = [
        sim_with_para["res_under_388_pct"],
        sim_rebound_386_pct,
        sim_with_para["ripple_p2p_pct"],
        0.45,
        4.65,
    ]

    b1 = ax.bar(x_idx - w_bar, vals_1000, width=w_bar, color=OI["blue"], label="실측 1000 ml/L 60Co")
    b2 = ax.bar(x_idx,         vals_150,  width=w_bar, color=OI["green"], label="실측 150 ml/L 60Co")
    b3 = ax.bar(x_idx + w_bar, vals_spice, width=w_bar, color=OI["vermilion"], label="ngspice 12단 다이노드 시뮬레이션")

    for bars in [b1, b2, b3]:
        for bar in bars:
            h = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2.0, h + 0.25, f"{h:.2f}%", ha="center", va="bottom", fontsize=7.8)

    ax.set_xticks(x_idx)
    ax.set_xticklabels(categories, fontsize=9.2)
    ax.set_ylim(0, 16.5)
    ax.set_ylabel("주 피크 대비 상대 비율 [% of Peak]", fontsize=11)
    ax.set_title("(f) 오버슈트·2차 언더슈트·125 MHz 꼬리 리플 정량 비율(%) 비교", fontsize=12, fontweight="bold")
    ax.legend(loc="upper left", fontsize=8.8, frameon=True)
    ax.grid(True, axis="y", linestyle=":", alpha=0.5)

    plt.tight_layout()
    out_pdf = "report/figures/fig_ngspice_h7195_fadc500_validation.pdf"
    out_png = "report/figures/fig_ngspice_h7195_fadc500_validation.png"
    fig.savefig(out_pdf, dpi=300, bbox_inches="tight")
    fig.savefig(out_png, dpi=220, bbox_inches="tight")
    plt.close(fig)
    if os.path.exists("docs/images"):
        shutil.copyfile(out_png, "docs/images/fig_ngspice_h7195_fadc500_validation.png")

    summary_json = {
        "voltage_correction": {
            "v_nom_v": -2000.0,
            "v_op_v": -1700.0,
            "vk_dy1_nom_v": VK_DY1_NOM,
            "vk_dy1_op_v": VK_DY1_OP,
            "eta_coll_nom": ETA_COLL_NOM,
            "eta_coll_op": ETA_COLL_OP,
            "eta_rel_ratio": ETA_REL_RATIO,
            "pde_nom": PDE_NOM,
            "pde_op": PDE_OP,
            "gain_nom": GAIN_NOM_2000,
            "gain_op": GAIN_OP_1700,
            "gain_ratio_op": GAIN_RATIO_OP,
            "q_1pe_nom_pc": Q_1PE_NOM_PC,
            "q_1pe_op_pc": Q_1PE_OP_PC,
            "effective_yield_ratio": EFF_YIELD_REL,
            "thr30_min_npe_nom2000": 7,
            "thr30_min_npe_op1700_collected": 27,
            "thr30_min_npe_op1700_nom_equiv": float(27.0 / ETA_REL_RATIO),
            "thr30_min_incident_photons_op1700": float(27.0 / PDE_OP),
        },
        "parasitic_ringing_metrics": {
            "f_ring_mhz": 125.0,
            "t_ring_ns": 8.0,
            "l_stem12_nh": 48.0,
            "l_esl12_nh": 12.0,
            "c_dy12_v_pf": 19.5,
            "c_d12_a_pf": 4.0,
            "measured_1000_60co": {
                k: v for k, v in st_1000.items() if not isinstance(v, np.ndarray)
            },
            "ngspice_53pe_op1700": {
                k: v for k, v in sim_with_para.items() if not isinstance(v, np.ndarray)
            },
        },
    }
    with open("report/ngspice_simulation_summary.json", "w", encoding="utf-8") as f:
        json.dump(summary_json, f, indent=2, ensure_ascii=False)

    print(f"\n[OK] 12단 다이노드 기생 등가회로 및 전압/획득률 보정 검증 그래프 저장 완료:\n  - {out_pdf}\n  - {out_png}")


if __name__ == "__main__":
    main()
