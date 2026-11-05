#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NKFADC500 Mini — EDA_CA 농도별(3000, 1000, 500, 150 ml/L) x 선원별(BKG, 137Cs, 60Co) 종합 물리 분석 시퀀스
========================================================================================================
- 분석 환경 : Python 3 + uproot + awkward + mplhep + matplotlib + numpy
- 컬러 팔레트: 색각 이상자 친화 오카베-이토 (Okabe-Ito) 팔레트 전수 적용
- 산출물    :
  1. report/figures/fig_charge_spectra_by_conc.pdf (.png)
     : 농도별(4종) 백그라운드 및 선원(137Cs, 60Co) 적분 전하 스펙트럼 분포 + 순(Net) 차감 스펙트럼
  2. report/figures/fig_charge_spectra_by_source.pdf (.png)
     : 선원 조건별(BKG, 137Cs, 60Co) 농도 변화(3000 -> 150 ml/L)에 따른 전하 분포 비교
  3. report/figures/fig_voltage_drop_timeseries_scatter.pdf (.png)
     : 1 us (0..992 ns) 구간 전압 강하(dV [mV]) 시계열 2D 빈도 산점도 (Persistence Density Plot)
  4. report/figures/fig_psd_and_summary_trends.pdf (.png)
     : 농도·선원별 PSD 비율(Q_tail / Q_total), 트리거율, 전하량 요약 트렌드 및 JSON 통계 덤프
"""

import json
import os
import sys
os.environ.setdefault("MPLCONFIGDIR", "/tmp/mpl_cache")
import numpy as np
import awkward as ak
import uproot
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm
import mplhep as hep

# ==============================================================================
# 1. 색각 이상자 친화 오카베-이토 (Okabe-Ito) 팔레트
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

SOURCE_COLORS = {
    "bkg":   OKABE_ITO["black"],
    "137cs": OKABE_ITO["blue"],
    "60co":  OKABE_ITO["vermilion"],
}
SOURCE_LABELS = {
    "bkg":   "배경잡음 (BKG)",
    "137cs": r"$^{137}\mathrm{Cs}$ (661.7 keV $\gamma$)",
    "60co":  r"$^{60}\mathrm{Co}$ (1.17 / 1.33 MeV $\gamma$)",
}

CONC_COLORS = {
    3000: OKABE_ITO["blue"],
    1000: OKABE_ITO["bluish_green"],
    500:  OKABE_ITO["orange"],
    150:  OKABE_ITO["reddish_purple"],
    50:   OKABE_ITO["vermilion"],
}

# ==============================================================================
# 2. 물리 상수 (ADC -> 전압 mV -> 적분 전하 pC)
# ==============================================================================
V_PP_VOLTS     = 2.5
ADC_FULL_SCALE = 4096.0
R_TERM_OHM     = 50.0
DT_SAMPLE_NS   = 2.0

DV_LSB_MV      = (V_PP_VOLTS * 1.0e3) / ADC_FULL_SCALE              # 0.6103515625 mV/ADC
Q_LSB_PC       = (DV_LSB_MV * 1.0e-3 * DT_SAMPLE_NS * 1.0e-9) / R_TERM_OHM * 1.0e12  # 0.0244140625 pC/(ADC*sample)

CONCENTRATIONS = [3000, 1000, 500, 150, 50]
SOURCES        = ["bkg", "137cs", "60co"]


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


def root_path_for(conc: int, src: str) -> str:
    for date_prefix in ("20261104", "20261105", "20261106"):
        candidate = f"data/{date_prefix}_160_temp_EDA_CA_{conc}_ml_per_l_{src}_hv_1700_thr_30_min_20_prod.root"
        if os.path.exists(candidate):
            return candidate
    return f"data/20261104_160_temp_EDA_CA_{conc}_ml_per_l_{src}_hv_1700_thr_30_min_20_prod.root"


def dat_path_for(conc: int, src: str) -> str:
    for date_prefix in ("20261104", "20261105", "20261106"):
        candidate = f"data/{date_prefix}_160_temp_EDA_CA_{conc}_ml_per_l_{src}_hv_1700_thr_30_min_20.dat"
        if os.path.exists(candidate):
            return candidate
    return f"data/20261104_160_temp_EDA_CA_{conc}_ml_per_l_{src}_hv_1700_thr_30_min_20.dat"


def load_scalar_metrics(conc: int, src: str):
    rpath = root_path_for(conc, src)
    dpath = dat_path_for(conc, src)
    if not os.path.exists(rpath):
        return None
    with uproot.open(rpath) as uf:
        tree = uf["PROD"]
        arr = tree.arrays(
            ["Baseline_Ch0", "BaselineRMS_Ch0", "Amplitude_Ch0", "Charge_Ch0",
             "Qtotal_Ch0", "Qtail_Ch0", "PsdRatio_Ch0", "PeakTime_Ch0", "TriggerTime"],
            library="ak"
        )
        base_adc = ak.to_numpy(arr["Baseline_Ch0"])
        brms_adc = ak.to_numpy(arr["BaselineRMS_Ch0"])
        amp_adc  = ak.to_numpy(arr["Amplitude_Ch0"])
        amp_mv   = amp_adc * DV_LSB_MV
        chg_raw  = ak.to_numpy(arr["Charge_Ch0"])
        chg_pc   = chg_raw * Q_LSB_PC
        qtot_pc  = ak.to_numpy(arr["Qtotal_Ch0"]) * Q_LSB_PC
        qtail_pc = ak.to_numpy(arr["Qtail_Ch0"]) * Q_LSB_PC
        psd      = ak.to_numpy(arr["PsdRatio_Ch0"])
        pt_ns    = ak.to_numpy(arr["PeakTime_Ch0"])
        tt_ns    = ak.to_numpy(arr["TriggerTime"])

    n_ev = len(amp_adc)
    duration_s = 1200.02
    return {
        "conc": conc,
        "source": src,
        "root_path": rpath,
        "dat_size_mb": os.path.getsize(dpath) / (1024 * 1024) if os.path.exists(dpath) else 0.0,
        "root_size_mb": os.path.getsize(rpath) / (1024 * 1024),
        "n_events": int(n_ev),
        "rate_hz": float(n_ev / duration_s),
        "base_mean_adc": float(np.mean(base_adc)),
        "brms_mean_adc": float(np.mean(brms_adc)),
        "amp_med_adc": float(np.median(amp_adc)),
        "amp_mean_adc": float(np.mean(amp_adc)),
        "amp_p90_adc": float(np.percentile(amp_adc, 90)),
        "amp_p99_adc": float(np.percentile(amp_adc, 99)),
        "amp_med_mv": float(np.median(amp_mv)),
        "amp_mean_mv": float(np.mean(amp_mv)),
        "amp_p99_mv": float(np.percentile(amp_mv, 99)),
        "chg_med_pc": float(np.median(chg_pc)),
        "chg_mean_pc": float(np.mean(chg_pc)),
        "chg_p90_pc": float(np.percentile(chg_pc, 90)),
        "chg_p99_pc": float(np.percentile(chg_pc, 99)),
        "qtot_med_pc": float(np.median(qtot_pc)),
        "qtot_mean_pc": float(np.mean(qtot_pc)),
        "psd_med": float(np.median(psd)),
        "psd_mean": float(np.mean(psd)),
        "pt_med_ns": float(np.median(pt_ns)),
        "pt_mean_ns": float(np.mean(pt_ns)),
        # Raw arrays for plotting
        "_chg_pc": chg_pc,
        "_qtot_pc": qtot_pc,
        "_amp_mv": amp_mv,
        "_psd": psd,
        "_pt_ns": pt_ns,
    }


def plot_charge_spectra(dataset: dict, out_dir: str):
    """
    1) 농도별(3000, 1000, 500, 150) 백그라운드 및 선원(137Cs, 60Co) 펄스 적분 전하 스펙트럼
       - 적응형 구간 빈(Adaptive Binning: 0~42 pC는 1.0 pC, 42~264 pC는 6.0 pC) 적용으로 우주선 영역 톱니 노이즈 제거 및 시인성 극대화
       - 저전하 선원/환경 감마선 컴프턴 능선·엣지 및 고전하 우주선 기원 능선·엣지 동시 표기
    2) 선원별·영역별 2x2 고시인성 비교 도면:
       - (a) BKG (배경잡음) 광대역 분포: 환경 감마/우주선 2차 전자 저전하 능선·엣지 vs 고전하 우주선 뮤온 능선·엣지
       - (b) 60Co 감마선 광대역 분포: 선원 감마선 컴프턴 산란(5~38 pC) 및 우주선 연속체(55~264 pC)
       - (c) 저전하(4~36 pC) 선형 확대: 배경 차감 순수 60Co 선원 컴프턴 연속 능선(Q_ridge) 및 컴프턴 엣지(50%/20%)
       - (d) 고전하(50~260 pC) 선형 확대 [mHz/pC]: 우주선 기원 고전하 연속 능선(Q_CR,ridge) 및 우주선 낙하 엣지(50%/20%)
    """
    plt.style.use(hep.style.CMS)
    setup_korean_font()

    # 1. 농도별 (A) 저전하 60Co 순수 컴프턴 능선·엣지 및 (B) 고전하 우주선 기원 능선·엣지 사전 계산
    ce_info = {}
    cr_info = {}
    bins_fine = np.linspace(0, 42, 85)  # 0.5 pC bin
    centers_fine = 0.5 * (bins_fine[:-1] + bins_fine[1:])
    bins_cr = np.arange(48, 272, 8.0)   # 8.0 pC bin for cosmic-ray linear spectrum
    centers_cr = 0.5 * (bins_cr[:-1] + bins_cr[1:])

    for conc in CONCENTRATIONS:
        d_bkg = dataset.get((conc, "bkg"))
        d_cs  = dataset.get((conc, "137cs"))
        d_co  = dataset.get((conc, "60co"))
        if d_bkg is not None and d_co is not None:
            c_bkg, _ = np.histogram(d_bkg["_chg_pc"], bins=bins_fine)
            c_co,  _ = np.histogram(d_co["_chg_pc"],  bins=bins_fine)
            net_rate = (c_co - c_bkg) / (1200.02 * 0.5)  # [Hz / pC]
            idx_pk = int(np.argmax(net_rate[:40]))
            q_ridge = float(centers_fine[idx_pk])
            r_ridge = float(net_rate[idx_pk])
            rq = centers_fine[idx_pk:65]
            rr = net_rate[idx_pk:65]
            q_ce50 = float(np.interp(0.50 * r_ridge, rr[::-1], rq[::-1]))
            q_ce20 = float(np.interp(0.20 * r_ridge, rr[::-1], rq[::-1]))

            # BKG 자체의 저전하 능선/엣지 (환경 감마 + 우주선 2차 전자 샤워 기원)
            r_bkg_fine = c_bkg / (1200.02 * 0.5)
            idx_bpk = int(np.argmax(r_bkg_fine[8:36])) + 8
            q_bkg_ridge = float(centers_fine[idx_bpk])
            r_bkg_pk = float(r_bkg_fine[idx_bpk])
            q_bkg_50 = float(np.interp(0.50 * r_bkg_pk, r_bkg_fine[idx_bpk:65][::-1], centers_fine[idx_bpk:65][::-1]))

            ce_info[conc] = {
                "q_ridge": q_ridge,
                "r_ridge": r_ridge,
                "q_ce50": q_ce50,
                "q_ce20": q_ce20,
                "q_bkg_ridge": q_bkg_ridge,
                "q_bkg_50": q_bkg_50,
                "net_rate": net_rate,
                "centers": centers_fine,
            }

        # 고전하(55~265 pC) 우주선 기원 능선 및 낙하 엣지 (농도당 3개 런 = 3600s 통합으로 통계 오차 최소화)
        cr_arrays = [d["_chg_pc"] for d in (d_bkg, d_cs, d_co) if d is not None]
        if cr_arrays:
            q_all = np.concatenate(cr_arrays)
            total_t = 1200.02 * len(cr_arrays)
            c_cr, _ = np.histogram(q_all, bins=bins_cr)
            r_cr_mhz = (c_cr / (total_t * 8.0)) * 1000.0  # [mHz / pC]
            r_cr_smooth = np.convolve(r_cr_mhz, [0.25, 0.50, 0.25], mode="same")
            mask_cr_pk = (centers_cr >= 85.0) & (centers_cr <= 145.0)
            idx_cr_pk = int(np.where(mask_cr_pk)[0][np.argmax(r_cr_smooth[mask_cr_pk])])
            q_cr_ridge = float(centers_cr[idx_cr_pk])
            v_cr_ridge = float(r_cr_smooth[idx_cr_pk])
            after_cr_q = centers_cr[idx_cr_pk:]
            after_cr_r = r_cr_smooth[idx_cr_pk:]
            q_cr50 = float(np.interp(0.50 * v_cr_ridge, after_cr_r[::-1], after_cr_q[::-1]))
            q_cr20 = float(np.interp(0.20 * v_cr_ridge, after_cr_r[::-1], after_cr_q[::-1]))
            q_cr_sub = q_all[(q_all >= 55.0) & (q_all <= 265.0)]
            cr_info[conc] = {
                "centers": centers_cr,
                "rate_mhz": r_cr_mhz,
                "rate_smooth_mhz": r_cr_smooth,
                "q_cr_ridge": q_cr_ridge,
                "v_cr_ridge_mhz": v_cr_ridge,
                "q_cr50": q_cr50,
                "q_cr20": q_cr20,
                "q_cr_med": float(np.median(q_cr_sub)),
                "r_cr_int_hz": float(len(q_cr_sub) / total_t),
            }

    # 적응형 빈(Adaptive Bins): 저전하(0~42 pC)는 1.0 pC 간격, 고전하(42~264 pC)는 6.0 pC 간격
    bins_adaptive = np.concatenate([np.arange(0.0, 42.0, 1.0), np.arange(42.0, 265.0, 6.0)])
    widths_adaptive = np.diff(bins_adaptive)

    # --- Figure A (그림 4): 농도별 광대역(0~264 pC) + 저전하/고전하 능선·엣지 고시인성 표시 ---
    conc_mass_labels = {
        3000: "3 g/L (3000 ml/L)",
        1000: "1 g/L (1000 ml/L)",
        500:  "0.5 g/L (500 ml/L)",
        150:  "150 mg/L (150 ml/L)",
        50:   "50 mg/L (50 ml/L)",
    }
    avail_concs = [c for c in CONCENTRATIONS if any((c, s) in dataset for s in SOURCES)]
    if not avail_concs:
        avail_concs = CONCENTRATIONS
    n_rows_a = (len(avail_concs) + 1) // 2
    fig, axes = plt.subplots(n_rows_a, 2, figsize=(18.5, 7.0 * n_rows_a), dpi=200, squeeze=False)

    for idx, conc in enumerate(avail_concs):
        ax = axes[idx // 2, idx % 2]
        ax.axvspan(5.0, 38.0, color="#E69F00", alpha=0.11, label="① 선원/환경 γ 컴프턴 산란 구간 (5~38 pC)")
        ax.axvspan(55.0, 260.0, color="#CC79A7", alpha=0.09, label="② 우주선(μ/샤워) 고전하 연속 구간 (55~260 pC)")

        for src in SOURCES:
            d = dataset.get((conc, src))
            if d is None:
                continue
            counts, edges = np.histogram(d["_chg_pc"], bins=bins_adaptive)
            rate_density = counts / (1200.02 * widths_adaptive)  # [Hz / pC]
            hep.histplot(
                rate_density, edges, ax=ax,
                color=SOURCE_COLORS[src], lw=2.3 if src == "60co" else 1.9,
                label=f"{SOURCE_LABELS[src]} (R={d['rate_hz']:.1f} Hz, <Q>={d['chg_mean_pc']:.1f} pC)"
            )

        if conc in ce_info:
            ci = ce_info[conc]
            ax.axvline(ci["q_ridge"], color="#009E73", ls=":", lw=1.8)
            ax.axvline(ci["q_ce20"], color="#D55E00", ls="--", lw=1.8)

        if conc in cr_info:
            cri = cr_info[conc]
            # 우주선 평균 평활 곡선 및 능선/엣지 수직선 표시
            ax.plot(cri["centers"], cri["rate_smooth_mhz"] * 1e-3, "-", color="#CC79A7", lw=2.8, alpha=0.90)
            ax.axvline(cri["q_cr_ridge"], color="#0072B2", ls="-.", lw=1.7, alpha=0.85)
            ax.axvline(cri["q_cr50"], color="#CC79A7", ls="--", lw=1.7, alpha=0.85)
            ax.text(
                cri["q_cr_ridge"] - 4.0, 8.2e-3,
                f"우주선 능선\n{cri['q_cr_ridge']:.0f} pC\n(중앙 {cri['q_cr_med']:.0f} pC)",
                color="#0072B2", fontsize=8.6, fontweight="bold", ha="center",
                bbox=dict(boxstyle="round,pad=0.22", fc="white", ec="#0072B2", alpha=0.92, lw=1.0)
            )
            ax.text(
                cri["q_cr50"] + 6.0, 6.8e-3,
                f"우주선 반치엣지(50%)\n{cri['q_cr50']:.0f} pC\n(20% 견부: {cri['q_cr20']:.0f} pC)",
                color="#882255", fontsize=8.5, fontweight="bold", ha="left",
                bbox=dict(boxstyle="round,pad=0.22", fc="white", ec="#CC79A7", alpha=0.92, lw=1.0)
            )

        ax.set_yscale("log")
        ax.set_xlim(0, 264)
        ax.set_ylim(2.5e-4, 32.0)
        ax.set_title(f"EDA_CA {conc_mass_labels[conc]} — 광대역 펄스 적분 전하(Q) 분포", fontsize=13.0, fontweight="bold", pad=8)
        ax.set_xlabel(r"펄스 적분 전하 $Q = \sum (B - \mathrm{ADC}[k]) \cdot q_{\mathrm{LSB}}$ [pC]", fontsize=11.5)
        ax.set_ylabel("차등 계수율 밀도 (dR/dQ) [Hz / pC]", fontsize=11.5)
        ax.legend(loc="upper right", fontsize=8.6, frameon=True, facecolor="white", framealpha=0.94)
        ax.grid(True, which="both", alpha=0.28)

        # 인셋: 4 ~ 36 pC 감마선 컴프턴 연속 능선 및 엣지 확대도 (우주선 곡선 위쪽 빈 공간에 배치)
        ax_in = ax.inset_axes([0.34, 0.46, 0.63, 0.30])
        ax_in.patch.set_facecolor("white")
        ax_in.patch.set_alpha(0.96)
        for src in SOURCES:
            d = dataset.get((conc, src))
            if d is None:
                continue
            c_in, e_in = np.histogram(d["_chg_pc"], bins=bins_fine)
            r_in = c_in / (1200.02 * 0.5)
            ax_in.step(e_in[:-1], r_in, where="post", color=SOURCE_COLORS[src], lw=1.8 if src == "60co" else 1.4)
        if conc in ce_info:
            ci = ce_info[conc]
            ax_in.axvline(ci["q_ridge"], color="#009E73", ls=":", lw=1.7, label=f"60Co 능선 {ci['q_ridge']:.1f}pC")
            ax_in.axvline(ci["q_ce50"], color="#0072B2", ls="-.", lw=1.7, label=f"60Co 반치(50%) {ci['q_ce50']:.1f}pC")
            ax_in.axvline(ci["q_ce20"], color="#D55E00", ls="--", lw=1.7, label=f"60Co 견부(20%) {ci['q_ce20']:.1f}pC")
        ax_in.set_yscale("log")
        ax_in.set_xlim(4, 36)
        ax_in.set_ylim(1.5e-2, 25.0)
        ax_in.set_title("① 저전하 감마선 컴프턴 영역 확대 (4~36 pC)", fontsize=9.0, fontweight="bold")
        ax_in.tick_params(labelsize=8.0)
        ax_in.legend(loc="upper right", fontsize=7.6, frameon=True, facecolor="white", framealpha=0.92)
        ax_in.grid(True, which="both", alpha=0.25)

    for extra_idx in range(len(avail_concs), n_rows_a * 2):
        axes[extra_idx // 2, extra_idx % 2].set_visible(False)

    fig.tight_layout()
    p1_pdf = os.path.join(out_dir, "fig_charge_spectra_by_conc.pdf")
    p1_png = os.path.join(out_dir, "fig_charge_spectra_by_conc.png")
    fig.savefig(p1_pdf, bbox_inches="tight")
    fig.savefig(p1_png, bbox_inches="tight")
    plt.close(fig)

    # --- Figure B (그림 5): 선원별 광대역 비교 + (c) 저전하 선원 컴프턴 능선/엣지 vs (d) 고전하 우주선 능선/엣지 (2x2) ---
    fig2, axes2 = plt.subplots(2, 2, figsize=(18.5, 13.8), dpi=200)

    # Panel (a): 배경잡음 (BKG) — 환경 감마/우주선 2차 전자 저전하 능선·엣지 vs 고전하 우주선 뮤온 능선·엣지
    ax_a = axes2[0, 0]
    ax_a.axvspan(5.0, 38.0, color="#E69F00", alpha=0.10, label="환경 γ / 우주선 2차 전자 컴프턴 영역 (5~38 pC)")
    ax_a.axvspan(55.0, 264.0, color="#CC79A7", alpha=0.09, label="우주선 뮤온 관통 고전하 연속 영역 (55~264 pC)")
    for conc in CONCENTRATIONS:
        d = dataset.get((conc, "bkg"))
        if d is None:
            continue
        counts, edges = np.histogram(d["_chg_pc"], bins=bins_adaptive)
        rate_density = counts / (1200.02 * widths_adaptive)
        hep.histplot(
            rate_density, edges, ax=ax_a,
            color=CONC_COLORS[conc], lw=2.0,
            label=f"{conc_mass_labels[conc]} (R={d['rate_hz']:.1f} Hz, P99={d['chg_p99_pc']:.1f} pC)"
        )
    ax_a.axvline(10.5, color="#009E73", ls=":", lw=1.8)
    ax_a.axvline(18.0, color="#D55E00", ls="--", lw=1.8)
    ax_a.axvline(118.0, color="#0072B2", ls="-.", lw=1.8)
    ax_a.axvline(176.0, color="#CC79A7", ls="--", lw=1.8)
    ax_a.text(21.0, 0.45, "기저 저전하 능선 ~10.5 pC\n반치 엣지 ~16~19 pC\n(환경 γ + 우주선 2차 샤워)",
              fontsize=8.8, fontweight="bold", color="#D55E00",
              bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#D55E00", alpha=0.92))
    ax_a.text(118.0, 1.5e-2, "우주선 뮤온 고전하 능선\n~102~126 pC (중앙 ~118 pC)",
              fontsize=8.8, fontweight="bold", color="#0072B2", ha="center",
              bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#0072B2", alpha=0.92))
    ax_a.text(180.0, 6.5e-3, "우주선 고전하 엣지\n50%: ~162~193 pC\n20%: ~185~219 pC",
              fontsize=8.5, fontweight="bold", color="#882255", ha="left",
              bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#CC79A7", alpha=0.92))
    ax_a.set_yscale("log")
    ax_a.set_xlim(0, 264)
    ax_a.set_ylim(2.5e-4, 32.0)
    ax_a.set_title("(a) 배경잡음 (BKG) — 기저 저전하 능선·엣지 및 고전하 우주선 능선·엣지 구조", fontsize=12.2, fontweight="bold", pad=8)
    ax_a.set_xlabel("펄스 적분 전하 Q [pC]", fontsize=11.5)
    ax_a.set_ylabel("차등 계수율 밀도 (dR/dQ) [Hz / pC]", fontsize=11.5)
    ax_a.legend(loc="upper right", fontsize=8.4, frameon=True, facecolor="white", framealpha=0.94)
    ax_a.grid(True, which="both", alpha=0.28)

    # Panel (b): 60Co (1.17 / 1.33 MeV) — 농도별 광대역 분포 (선원 컴프턴 vs 우주선 뮤온)
    ax_b = axes2[0, 1]
    ax_b.axvspan(5.0, 38.0, color="#E69F00", alpha=0.10, label=r"$^{60}\mathrm{Co}$ 감마선 컴프턴 산란 우세 구간 (5~38 pC)")
    ax_b.axvspan(55.0, 264.0, color="#CC79A7", alpha=0.09, label="우주선 뮤온 연속 분포 구간 (55~264 pC, 선원 무관 불변)")
    for conc in CONCENTRATIONS:
        d = dataset.get((conc, "60co"))
        if d is None:
            continue
        counts, edges = np.histogram(d["_chg_pc"], bins=bins_adaptive)
        rate_density = counts / (1200.02 * widths_adaptive)
        hep.histplot(
            rate_density, edges, ax=ax_b,
            color=CONC_COLORS[conc], lw=2.1,
            label=f"{conc_mass_labels[conc]} (R={d['rate_hz']:.1f} Hz, <Q>={d['chg_mean_pc']:.1f} pC)"
        )
    ax_b.text(22.0, 1.8, r"$^{60}\mathrm{Co}$ 감마선 유입 시" + "\n저전하 컴프턴 능선(9.2~9.8 pC)\n계수율 ~8~12배 급증",
              fontsize=9.0, fontweight="bold", color="#D55E00",
              bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#D55E00", alpha=0.92))
    ax_b.text(135.0, 1.3e-2, "우주선 고전하 영역 (55~260 pC)\n선원 유무·농도 무관 일정 (~0.43 Hz)",
              fontsize=8.8, fontweight="bold", color="#882255", ha="center",
              bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#CC79A7", alpha=0.92))
    ax_b.set_yscale("log")
    ax_b.set_xlim(0, 264)
    ax_b.set_ylim(2.5e-4, 32.0)
    ax_b.set_title(r"(b) $^{60}\mathrm{Co}$ (1.17 / 1.33 MeV $\gamma$) — 농도별 광대역 펄스 적분 전하 분포 (0~264 pC)", fontsize=12.2, fontweight="bold", pad=8)
    ax_b.set_xlabel("펄스 적분 전하 Q [pC]", fontsize=11.5)
    ax_b.set_ylabel("차등 계수율 밀도 (dR/dQ) [Hz / pC]", fontsize=11.5)
    ax_b.legend(loc="upper right", fontsize=8.4, frameon=True, facecolor="white", framealpha=0.94)
    ax_b.grid(True, which="both", alpha=0.28)

    # Panel (c): 저전하(4~36 pC) 확대 — 배경 차감 순수 60Co 선원 컴프턴 연속 능선 및 컴프턴 엣지(50%/20%)
    ax_c = axes2[1, 0]
    for conc in CONCENTRATIONS:
        if conc not in ce_info:
            continue
        ci = ce_info[conc]
        col = CONC_COLORS[conc]
        ax_c.plot(
            ci["centers"], np.maximum(0.0, ci["net_rate"]), "o-", color=col, lw=2.3, ms=4.2,
            label=f"{conc_mass_labels[conc]}: 능선={ci['q_ridge']:.1f}pC | 반치(50%)={ci['q_ce50']:.1f}pC | 견부(20%)={ci['q_ce20']:.1f}pC"
        )
        ax_c.axvline(ci["q_ce50"], color=col, ls="--", lw=1.6, alpha=0.80)
        ax_c.axvline(ci["q_ce20"], color=col, ls=":", lw=1.6, alpha=0.80)

    ax_c.annotate("컴프턴 연속 능선 피크\n(Q_ridge = 9.2 ~ 9.8 pC)",
                  xy=(9.5, 19.8), xytext=(15.5, 14.2),
                  arrowprops=dict(arrowstyle="->", color="#000000", lw=1.5),
                  fontsize=9.2, fontweight="bold",
                  bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#000000", alpha=0.92))
    ax_c.annotate("150 mg/L 희석 시 컴프턴 엣지 좌측 이동\n(50%: 15.0→12.9 pC, 20%: 19.1→15.2 pC)",
                  xy=(12.9, 9.9), xytext=(17.2, 8.8),
                  arrowprops=dict(arrowstyle="->", color="#CC79A7", lw=1.8),
                  fontsize=9.0, fontweight="bold", color="#882255",
                  bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#CC79A7", alpha=0.94))
    ax_c.set_xlim(4, 36)
    ax_c.set_ylim(0, 21.5)
    ax_c.set_title(r"(c) 저전하(4~36 pC) 확대: 순수 $^{60}\mathrm{Co}$ 선원 컴프턴 연속 능선 및 엣지(50%/20%)", fontsize=12.0, fontweight="bold", pad=8)
    ax_c.set_xlabel("펄스 적분 전하 Q [pC] (감마선 컴프턴 산란 영역)", fontsize=11.5)
    ax_c.set_ylabel(r"배경 차감 순수 감마선 밀도 $\Delta(\mathrm{d}R/\mathrm{d}Q)$ [Hz / pC]", fontsize=11.5)
    ax_c.legend(loc="upper right", fontsize=8.3, frameon=True, facecolor="white", framealpha=0.94)
    ax_c.grid(True, which="both", alpha=0.28)

    # Panel (d): 고전하(50~260 pC) 선형 확대 — 우주선 기원 고전하 연속 능선 및 낙하 엣지(50%/20%)
    ax_d = axes2[1, 1]
    ax_d.axvspan(85.0, 145.0, color="#0072B2", alpha=0.08, label="우주선 뮤온 관통 능선 대역 (85~145 pC)")
    ax_d.axvspan(160.0, 220.0, color="#CC79A7", alpha=0.09, label="우주선 최대 경로차 낙하 엣지 대역 (160~220 pC)")
    for conc in CONCENTRATIONS:
        if conc not in cr_info:
            continue
        cri = cr_info[conc]
        col = CONC_COLORS[conc]
        ax_d.plot(
            cri["centers"], cri["rate_smooth_mhz"], "s-", color=col, lw=2.3, ms=4.5,
            label=f"{conc_mass_labels[conc]}: 능선={cri['q_cr_ridge']:.0f}pC (중앙 {cri['q_cr_med']:.0f}pC) | 50%엣지={cri['q_cr50']:.0f}pC | 20%엣지={cri['q_cr20']:.0f}pC"
        )
        ax_d.axvline(cri["q_cr50"], color=col, ls="--", lw=1.4, alpha=0.75)

    ax_d.annotate("우주선 기원 고전하 연속 능선\n(Landau-Vavilov 피크: ~102~126 pC,\n구간 중앙값 115~123 pC)",
                  xy=(114.0, 3.65), xytext=(62.0, 4.45),
                  arrowprops=dict(arrowstyle="->", color="#0072B2", lw=1.6),
                  fontsize=9.0, fontweight="bold", color="#0072B2",
                  bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#0072B2", alpha=0.94))
    ax_d.annotate("우주선 기원 고전하 낙하 엣지\n(반치 50%: 162~193 pC,\n견부 20%: 185~219 pC)",
                  xy=(175.0, 1.80), xytext=(182.0, 2.75),
                  arrowprops=dict(arrowstyle="->", color="#882255", lw=1.6),
                  fontsize=9.0, fontweight="bold", color="#882255",
                  bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#CC79A7", alpha=0.94))
    ax_d.set_xlim(50, 260)
    ax_d.set_ylim(0, 5.6)
    ax_d.set_title("(d) 고전하(50~260 pC) 선형 확대: 우주선 기원 연속 능선 및 낙하 엣지(50%/20%) 비교", fontsize=12.0, fontweight="bold", pad=8)
    ax_d.set_xlabel("펄스 적분 전하 Q [pC] (우주선 뮤온 관통 영역 50~260 pC)", fontsize=11.5)
    ax_d.set_ylabel("우주선 차등 계수율 밀도 (dR/dQ) [mHz / pC]", fontsize=11.5)
    ax_d.legend(loc="upper right", fontsize=8.1, frameon=True, facecolor="white", framealpha=0.94)
    ax_d.grid(True, which="both", alpha=0.28)

    fig2.tight_layout()
    p2_pdf = os.path.join(out_dir, "fig_charge_spectra_by_source.pdf")
    p2_png = os.path.join(out_dir, "fig_charge_spectra_by_source.png")
    fig2.savefig(p2_pdf, bbox_inches="tight")
    fig2.savefig(p2_png, bbox_inches="tight")
    plt.close(fig2)
    print(f"[OK] 전하 스펙트럼 분포 그래프 저장 완료:\n  - {p1_pdf}\n  - {p2_pdf}")


def plot_voltage_drop_timeseries_scatter(out_dir: str, max_wf_events: int = 3000, max_scatter_pts: int = 45000):
    """
    전압 강하 시계열 2D 빈도 산점도 (Point-Cloud Scatter Plot + 평균 궤적 오버레이):
    - 수평축 : 1 마이크로초 구간 (0 ~ 992 ns, 496 samples @ 2.0 ns/sample)
    - 수직축 : 전압 강하 -dV = -(Baseline - ADC[k]) * 0.61035 mV [mV]
    - 산점도 : 실제 개별 샘플 점(ax.scatter)을 찍고, 각 점의 국소 빈도(Frequency Density)를 색(LogNorm)으로 매핑
    - 평균궤적: 샘플 시각 t별 앙상블 평균 궤적 < -dV(t) > 및 P10~P90 궤적 밴드를 산점도 위에 중첩 표시
    - 추가 도면: 농도별·선원별 평균 파형 궤적(< -dV(t) >) 직접 비교 그래프 (fig_mean_waveform_trajectories.pdf)
    """
    plt.style.use(hep.style.CMS)
    setup_korean_font()
    import matplotlib.patheffects as pe

    avail_concs = [c for c in CONCENTRATIONS if any(os.path.exists(root_path_for(c, s)) for s in SOURCES)]
    if not avail_concs:
        avail_concs = CONCENTRATIONS
    n_rows = len(avail_concs)
    fig, axes = plt.subplots(n_rows, 3, figsize=(19, 4.6 * n_rows), dpi=200, sharex=True, sharey=True, squeeze=False)
    time_edges = np.linspace(0, 992, 249)      # 빈도 계산용 4 ns 그리드
    vdrop_edges = np.linspace(-260, 20, 141)   # 빈도 계산용 2 mV 그리드

    mean_trajectories = {}  # (conc, src) -> (t_axis, mean_v, p90_v)
    last_sc = None
    rng = np.random.default_rng(160)

    for r_idx, conc in enumerate(avail_concs):
        for c_idx, src in enumerate(SOURCES):
            ax = axes[r_idx, c_idx]
            rpath = root_path_for(conc, src)
            if not os.path.exists(rpath):
                ax.text(0.5, 0.5, "데이터 미수집", ha="center", va="center", transform=ax.transAxes)
                continue

            with uproot.open(rpath) as uf:
                tree = uf["PROD"]
                n_read = min(max_wf_events, tree.num_entries)
                wf = tree.arrays(["wTime_Ch0", "wDrop_Ch0"], entry_stop=n_read, library="ak")
                # (N_events, 496) 정형 2D 행렬 변환으로 시각별 평균 궤적 계산
                t_mat = ak.to_numpy(wf["wTime_Ch0"])
                v_mat = -ak.to_numpy(wf["wDrop_Ch0"]) * DV_LSB_MV  # 음극성 전압 강하 [-mV]

            t_axis = t_mat[0]
            mean_v = np.mean(v_mat, axis=0)
            p10_v  = np.percentile(v_mat, 10, axis=0)  # 상위 10% 강한 음극성 강하 궤적
            mean_trajectories[(conc, src)] = (t_axis, mean_v, p10_v)

            # 1D 평탄화 후 각 점의 2D 국소 빈도(Frequency) 계산
            t_flat = t_mat.ravel()
            v_flat = v_mat.ravel()
            H, _, _ = np.histogram2d(t_flat, v_flat, bins=[time_edges, vdrop_edges])

            # 산점도 렌더링을 위한 서브샘플링 (펄스 영역 가중 포함 + 균일 샘플링)
            if len(t_flat) > max_scatter_pts:
                sel = rng.choice(len(t_flat), size=max_scatter_pts, replace=False)
                t_pts = t_flat[sel]
                v_pts = v_flat[sel]
            else:
                t_pts = t_flat
                v_pts = v_flat

            ix = np.clip(np.searchsorted(time_edges, t_pts, side="right") - 1, 0, H.shape[0] - 1)
            iy = np.clip(np.searchsorted(vdrop_edges, v_pts, side="right") - 1, 0, H.shape[1] - 1)
            freq_pts = np.maximum(1.0, H[ix, iy])

            # 빈도가 높은 점이 위에 오도록 정렬 후 산점도(ax.scatter) 플롯
            order = np.argsort(freq_pts)
            t_pts = t_pts[order]
            v_pts = v_pts[order]
            freq_pts = freq_pts[order]

            last_sc = ax.scatter(
                t_pts, v_pts, c=freq_pts,
                s=2.8, alpha=0.55, cmap="viridis",
                norm=LogNorm(vmin=1, vmax=max(100.0, float(np.max(H)))),
                edgecolors="none", rasterized=True
            )

            # 평균 궤적(Mean Trajectory) 및 상위 10% 포락선(P10 Envelope) 오버레이
            ax.plot(
                t_axis, p10_v, color=OKABE_ITO["sky_blue"], ls="--", lw=1.4, zorder=5,
                label="상위 10% 궤적 (P10 Envelope)"
            )
            line_mean, = ax.plot(
                t_axis, mean_v, color=OKABE_ITO["vermilion"], lw=2.3, zorder=6,
                label=f"평균 궤적 < -dV(t) > (Peak={np.min(mean_v):.1f} mV)"
            )
            line_mean.set_path_effects([pe.Stroke(linewidth=3.6, foreground="white"), pe.Normal()])

            ax.axhline(-30.0 * DV_LSB_MV, color=OKABE_ITO["orange"], ls=":", lw=1.2,
                       label="THR = -18.31 mV")
            ax.set_xlim(0, 992)
            ax.set_ylim(-250, 15)
            ax.set_title(f"{conc} ml/L — {SOURCE_LABELS[src]}", fontsize=11.2, fontweight="bold", pad=5)
            ax.legend(loc="lower right", fontsize=8.0, frameon=True, framealpha=0.88)
            ax.grid(True, alpha=0.22)
            if r_idx == n_rows - 1:
                ax.set_xlabel("파형 시간 (Time) [ns] (1 us 윈도우)", fontsize=10.5)
            if c_idx == 0:
                ax.set_ylabel("전압 강하 -dV [mV]", fontsize=10.5)

    fig.subplots_adjust(right=0.91, hspace=0.22, wspace=0.10)
    cbar_ax = fig.add_axes([0.925, 0.12, 0.018, 0.76])
    if last_sc is not None:
        cb = fig.colorbar(last_sc, cax=cbar_ax)
        cb.set_label("산점도 국소 빈도 (Point Frequency / Bin) [Log Scale]", fontsize=12)

    p3_pdf = os.path.join(out_dir, "fig_voltage_drop_timeseries_scatter.pdf")
    p3_png = os.path.join(out_dir, "fig_voltage_drop_timeseries_scatter.png")
    fig.savefig(p3_pdf, bbox_inches="tight")
    fig.savefig(p3_png, bbox_inches="tight")
    plt.close(fig)

    # --------------------------------------------------------------------------
    # 추가 도면: 농도별·선원별 평균 파형 궤적 (< -dV(t) >) 확대 비교 (320 ~ 480 ns 피크 영역)
    # --------------------------------------------------------------------------
    fig_m, axes_m = plt.subplots(1, 3, figsize=(19, 5.8), dpi=200, sharey=True)
    for idx, src in enumerate(SOURCES):
        ax = axes_m[idx]
        for conc in CONCENTRATIONS:
            if (conc, src) not in mean_trajectories:
                continue
            t_ax, m_v, p10_v = mean_trajectories[(conc, src)]
            ax.plot(t_ax, m_v, lw=2.3, color=CONC_COLORS[conc],
                    label=f"{conc} ml/L 평균 궤적 (Peak={np.min(m_v):.1f} mV)")
            ax.plot(t_ax, p10_v, lw=1.2, ls="--", color=CONC_COLORS[conc], alpha=0.75)
        ax.set_xlim(330, 470)
        ax.axhline(0.0, color=OKABE_ITO["grey"], lw=0.9, ls=":")
        ax.axhline(-30.0 * DV_LSB_MV, color=OKABE_ITO["vermilion"], lw=1.1, ls=":", label="THR = -18.31 mV")
        ax.set_title(f"{SOURCE_LABELS[src]} — 농도별 평균 파형 궤적 비교", fontsize=12.2, fontweight="bold", pad=8)
        ax.set_xlabel("파형 시간 (Time) [ns] (피크 주변 330~470 ns 확대)", fontsize=10.8)
        if idx == 0:
            ax.set_ylabel("평균 전압 강하 < -dV(t) > [mV] (점선: P10 궤적)", fontsize=11)
        ax.legend(loc="lower right", fontsize=8.8, frameon=True)
        ax.grid(True, alpha=0.25)

    fig_m.tight_layout()
    pm_pdf = os.path.join(out_dir, "fig_mean_waveform_trajectories.pdf")
    pm_png = os.path.join(out_dir, "fig_mean_waveform_trajectories.png")
    fig_m.savefig(pm_pdf, bbox_inches="tight")
    fig_m.savefig(pm_png, bbox_inches="tight")
    plt.close(fig_m)

    print(f"[OK] 1 us 전압 강하 산점도 및 평균 궤적 비교 저장 완료:\n  - {p3_pdf}\n  - {pm_pdf}")


def plot_psd_and_summary_trends(dataset: dict, out_dir: str):
    """농도·선원별 PSD 비율 분포, 평균 트리거율 및 평균/P99 전하량 트렌드 비교 그래프."""
    plt.style.use(hep.style.CMS)
    setup_korean_font()

    fig, axes = plt.subplots(1, 3, figsize=(19, 5.8), dpi=200)

    # Panel 1: 농도별 평균 트리거율 (Hz)
    ax0 = axes[0]
    for src in SOURCES:
        concs_avail = [c for c in CONCENTRATIONS if (c, src) in dataset]
        rates = [dataset[(c, src)]["rate_hz"] for c in concs_avail]
        ax0.plot(concs_avail, rates, marker="o", ms=8, lw=2.2, color=SOURCE_COLORS[src], label=SOURCE_LABELS[src])
    ax0.set_title("(a) 농도 및 선원별 평균 트리거 계수율", fontsize=12.5, fontweight="bold")
    ax0.set_xlabel("EDA_CA 농도 [ml/L]", fontsize=11)
    ax0.set_ylabel("평균 트리거율 (Trigger Rate) [Hz]", fontsize=11)
    ax0.legend(loc="best", fontsize=9.5, frameon=True)
    ax0.grid(True, alpha=0.25)

    # Panel 2: 농도별 평균 적분 전하 <Q> 및 중앙값 Q_med [pC]
    ax1 = axes[1]
    for src in SOURCES:
        concs_avail = [c for c in CONCENTRATIONS if (c, src) in dataset]
        q_means = [dataset[(c, src)]["chg_mean_pc"] for c in concs_avail]
        q_meds  = [dataset[(c, src)]["chg_med_pc"] for c in concs_avail]
        ax1.plot(concs_avail, q_means, marker="s", ms=7, lw=2.2, ls="-", color=SOURCE_COLORS[src],
                 label=f"{SOURCE_LABELS[src]} (평균 <Q>)")
        ax1.plot(concs_avail, q_meds, marker="^", ms=6, lw=1.6, ls="--", color=SOURCE_COLORS[src],
                 label=f"{SOURCE_LABELS[src]} (중앙값 Q_med)")
    ax1.set_title("(b) 농도 및 선원별 적분 전하(Q) 경향성", fontsize=12.5, fontweight="bold")
    ax1.set_xlabel("EDA_CA 농도 [ml/L]", fontsize=11)
    ax1.set_ylabel("적분 전하 Q [pC]", fontsize=11)
    ax1.legend(loc="best", fontsize=8.2, ncol=2, frameon=True)
    ax1.grid(True, alpha=0.25)

    # Panel 3: PSD Ratio (Q_tail / Q_total) 분포 비교 (농도 1000 ml/L 대표)
    ax2 = axes[2]
    psd_bins = np.linspace(0, 0.7, 71)
    rep_conc = 1000 if (1000, "bkg") in dataset else CONCENTRATIONS[0]
    for src in SOURCES:
        d = dataset.get((rep_conc, src))
        if d is None:
            continue
        cnt, edg = np.histogram(d["_psd"], bins=psd_bins, density=True)
        hep.histplot(cnt, edg, ax=ax2, color=SOURCE_COLORS[src], lw=2.2,
                     label=f"{SOURCE_LABELS[src]} (중앙값={d['psd_med']:.3f})")
    ax2.set_title(f"(c) 파형 형상 변별(PSD = Q_tail / Q_total) 분포 ({rep_conc} ml/L)", fontsize=12.5, fontweight="bold")
    ax2.set_xlabel("PSD Ratio (Q_tail / Q_total)", fontsize=11)
    ax2.set_ylabel("정규화 밀도 (Normalized Density)", fontsize=11)
    ax2.legend(loc="upper right", fontsize=9.2, frameon=True)
    ax2.grid(True, alpha=0.25)

    fig.tight_layout()
    p4_pdf = os.path.join(out_dir, "fig_psd_and_summary_trends.pdf")
    p4_png = os.path.join(out_dir, "fig_psd_and_summary_trends.png")
    fig.savefig(p4_pdf, bbox_inches="tight")
    fig.savefig(p4_png, bbox_inches="tight")
    plt.close(fig)
    print(f"[OK] PSD 및 농도별 요약 트렌드 그래프 저장 완료:\n  - {p4_pdf}\n  - {p4_png}")


def main():
    out_dir = "report/figures"
    os.makedirs(out_dir, exist_ok=True)

    dataset = {}
    summary_rows = []
    for conc in CONCENTRATIONS:
        for src in SOURCES:
            d = load_scalar_metrics(conc, src)
            if d is not None:
                dataset[(conc, src)] = d
                row = {k: v for k, v in d.items() if not k.startswith("_")}
                summary_rows.append(row)
                print(f"[LOADED] {conc:4d} ml/L | {src:5s} : N={d['n_events']:7,d} ({d['rate_hz']:6.2f} Hz), "
                      f"<Amp>={d['amp_mean_mv']:5.2f} mV, <Q>={d['chg_mean_pc']:5.2f} pC, P99(Q)={d['chg_p99_pc']:6.2f} pC, <PSD>={d['psd_mean']:.3f}")
            else:
                print(f"[SKIP]   {conc:4d} ml/L | {src:5s} : 파일 미존재")

    json_path = "report/analysis_summary.json"
    with open(json_path, "w", encoding="utf-8") as fp:
        json.dump(summary_rows, fp, indent=2, ensure_ascii=False)
    print(f"[OK] 요약 물리 지표 JSON 저장: {json_path}")

    plot_charge_spectra(dataset, out_dir)
    if "--only-spectra" not in sys.argv:
        plot_voltage_drop_timeseries_scatter(out_dir)
        plot_psd_and_summary_trends(dataset, out_dir)


if __name__ == "__main__":
    main()
