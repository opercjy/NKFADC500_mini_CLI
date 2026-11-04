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

CONCENTRATIONS = [3000, 1000, 500, 150]
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
    return f"data/20261104_160_temp_EDA_CA_{conc}_ml_per_l_{src}_hv_1700_thr_30_min_20_prod.root"


def dat_path_for(conc: int, src: str) -> str:
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
    1) 농도별(3000, 1000, 500, 150) 백그라운드 및 선원(137Cs, 60Co) 전하 스펙트럼 분포 (2x2 그리드)
    2) 선원별(BKG, 137Cs, 60Co) 농도 비교 전하 스펙트럼 분포 (1x3 그리드)
    """
    plt.style.use(hep.style.CMS)
    setup_korean_font()

    # --- Figure A: 농도별 (각 패널 안: BKG vs 137Cs vs 60Co) ---
    fig, axes = plt.subplots(2, 2, figsize=(16, 12), dpi=200)
    bins_q = np.linspace(0, 60, 121)  # 0 ~ 60 pC (0.5 pC/bin)

    for idx, conc in enumerate(CONCENTRATIONS):
        ax = axes[idx // 2, idx % 2]
        for src in SOURCES:
            d = dataset.get((conc, src))
            if d is None:
                continue
            counts, edges = np.histogram(d["_chg_pc"], bins=bins_q)
            rate_bins = counts / 1200.02  # [Hz / 0.5 pC]
            hep.histplot(
                rate_bins, edges, ax=ax,
                color=SOURCE_COLORS[src], lw=2.2,
                label=f"{SOURCE_LABELS[src]} (R={d['rate_hz']:.1f} Hz, <Q>={d['chg_mean_pc']:.2f} pC)"
            )
        ax.set_yscale("log")
        ax.set_xlim(0, 60)
        ax.set_title(f"EDA_CA 농도 {conc} ml/L — 선원별 적분 전하(Q) 스펙트럼", fontsize=13, fontweight="bold", pad=8)
        ax.set_xlabel(r"적분 전하 $Q = \sum (B - \mathrm{ADC}[k]) \cdot q_{\mathrm{LSB}}$ [pC]", fontsize=11)
        ax.set_ylabel("차등 계수율 (Differential Rate) [Hz / 0.5 pC]", fontsize=11)
        ax.legend(loc="upper right", fontsize=9.2, frameon=True)
        ax.grid(True, which="both", alpha=0.25)

    fig.tight_layout()
    p1_pdf = os.path.join(out_dir, "fig_charge_spectra_by_conc.pdf")
    p1_png = os.path.join(out_dir, "fig_charge_spectra_by_conc.png")
    fig.savefig(p1_pdf, bbox_inches="tight")
    fig.savefig(p1_png, bbox_inches="tight")
    plt.close(fig)

    # --- Figure B: 선원별 (각 패널 안: 농도 3000, 1000, 500, 150 비교) ---
    fig2, axes2 = plt.subplots(1, 3, figsize=(19, 6.2), dpi=200)
    bins_wide = np.linspace(0, 160, 81)  # 고전하 꼬리까지 관찰 (0 ~ 160 pC)

    for idx, src in enumerate(SOURCES):
        ax = axes2[idx]
        for conc in CONCENTRATIONS:
            d = dataset.get((conc, src))
            if d is None:
                continue
            counts, edges = np.histogram(d["_chg_pc"], bins=bins_wide)
            rate_bins = counts / 1200.02
            hep.histplot(
                rate_bins, edges, ax=ax,
                color=CONC_COLORS[conc], lw=2.1,
                label=f"{conc} ml/L (R={d['rate_hz']:.1f} Hz, P99={d['chg_p99_pc']:.1f} pC)"
            )
        ax.set_yscale("log")
        ax.set_xlim(0, 160)
        ax.set_title(f"{SOURCE_LABELS[src]} — 농도별 전하 분포 비교", fontsize=12.5, fontweight="bold", pad=8)
        ax.set_xlabel("적분 전하 Q [pC]", fontsize=11)
        ax.set_ylabel("차등 계수율 [Hz / 2.0 pC]", fontsize=11)
        ax.legend(loc="upper right", fontsize=8.8, frameon=True)
        ax.grid(True, which="both", alpha=0.25)

    fig2.tight_layout()
    p2_pdf = os.path.join(out_dir, "fig_charge_spectra_by_source.pdf")
    p2_png = os.path.join(out_dir, "fig_charge_spectra_by_source.png")
    fig2.savefig(p2_pdf, bbox_inches="tight")
    fig2.savefig(p2_png, bbox_inches="tight")
    plt.close(fig2)
    print(f"[OK] 전하 스펙트럼 분포 그래프 저장 완료:\n  - {p1_pdf}\n  - {p2_pdf}")


def plot_voltage_drop_timeseries_scatter(out_dir: str, max_wf_events: int = 15000):
    """
    전압 강하 시계열 2D 빈도 산점도 (Digital Persistence Density Plot):
    - 수평축 : 1 마이크로초 구간 (0 ~ 992 ns, 496 samples @ 2.0 ns/sample)
    - 수직축 : 전압 강하 dV = (Baseline - ADC[k]) * 0.61035 mV [mV] (음극성 강하 -dV로 표시)
    - 색상   : 샘플-전압 빈도 (LogNorm 컬러맵)
    - 구조   : 4개 농도(행: 3000, 1000, 500, 150) x 3개 선원(열: BKG, 137Cs, 60Co) 12-패널 종합 비교
    """
    plt.style.use(hep.style.CMS)
    setup_korean_font()

    fig, axes = plt.subplots(4, 3, figsize=(19, 18), dpi=200, sharex=True, sharey=True)
    time_edges = np.linspace(0, 992, 249)      # 4 ns bin width along 0..992 ns
    vdrop_edges = np.linspace(-260, 20, 141)   # 전압 변화 [-260 mV .. +20 mV]

    last_mesh = None
    for r_idx, conc in enumerate(CONCENTRATIONS):
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
                # awkward jagged -> 1D flat numpy array 변환 (고속 벡터화)
                t_flat = ak.to_numpy(ak.flatten(wf["wTime_Ch0"]))
                # 음극성 전압 강하: -wDrop_ADC * dV_LSB [mV]
                v_flat = -ak.to_numpy(ak.flatten(wf["wDrop_Ch0"])) * DV_LSB_MV

            H, xedges, yedges = np.histogram2d(t_flat, v_flat, bins=[time_edges, vdrop_edges])
            H_plot = np.ma.masked_less_equal(H.T, 0)
            last_mesh = ax.pcolormesh(
                xedges, yedges, H_plot,
                norm=LogNorm(vmin=1, vmax=max(100.0, float(np.max(H)))),
                cmap="viridis", rasterized=True
            )
            ax.axhline(-30.0 * DV_LSB_MV, color=OKABE_ITO["vermilion"], ls="--", lw=1.2,
                       label="THR = -18.31 mV (30 ADC)")
            ax.axvline(372.0, color=OKABE_ITO["orange"], ls=":", lw=1.2)
            ax.set_title(f"{conc} ml/L — {SOURCE_LABELS[src]} (N={n_read:,d} evts)", fontsize=11, fontweight="bold", pad=5)
            ax.grid(True, alpha=0.2)
            if r_idx == 3:
                ax.set_xlabel("파형 시간 (Time) [ns] (1 us 윈도우)", fontsize=10.5)
            if c_idx == 0:
                ax.set_ylabel("전압 변화 -dV [mV]", fontsize=10.5)

    fig.subplots_adjust(right=0.91, hspace=0.22, wspace=0.12)
    cbar_ax = fig.add_axes([0.925, 0.12, 0.018, 0.76])
    if last_mesh is not None:
        cb = fig.colorbar(last_mesh, cax=cbar_ax)
        cb.set_label("샘플 빈도 (Sample Count / Bin) [Log Scale]", fontsize=12)

    p3_pdf = os.path.join(out_dir, "fig_voltage_drop_timeseries_scatter.pdf")
    p3_png = os.path.join(out_dir, "fig_voltage_drop_timeseries_scatter.png")
    fig.savefig(p3_pdf, bbox_inches="tight")
    fig.savefig(p3_png, bbox_inches="tight")
    plt.close(fig)
    print(f"[OK] 1 us 전압 강하 시계열 2D 산점도 저장 완료:\n  - {p3_pdf}\n  - {p3_png}")


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
    plot_voltage_drop_timeseries_scatter(out_dir)
    plot_psd_and_summary_trends(dataset, out_dir)


if __name__ == "__main__":
    main()
