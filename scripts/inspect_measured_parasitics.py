#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import numpy as np
import awkward as ak
import uproot

Q_LSB_PC = 0.0244140625  # pC/(ADC*sample)

def main():
    for c in [3000, 1000, 500, 150]:
        print(f"\n================ Concentration {c} ml/L ================")
        hists = {}
        bins = np.linspace(0, 320, 161) # 2 pC bins up to 320 pC
        centers = 0.5 * (bins[:-1] + bins[1:])
        for s in ["bkg", "137cs", "60co"]:
            p = f"data/20261104_160_temp_EDA_CA_{c}_ml_per_l_{s}_hv_1700_thr_30_min_20_prod.root"
            with uproot.open(p) as f:
                chg = ak.to_numpy(f["PROD"]["Charge_Ch0"].array(library="ak")) * Q_LSB_PC
            counts, _ = np.histogram(chg, bins=bins)
            rate_density = counts / (1200.0 * 2.0)  # Hz / pC
            hists[s] = rate_density

        # 60Co - BKG 순수 감마선 차등 계수율에서 능선(Ridge) 피크, 반치점(50%, 10% Edge), 변곡점 추정
        net_60co = hists["60co"] - hists["bkg"]
        idx_ridge = int(np.argmax(net_60co[:30]))
        q_ridge = centers[idx_ridge]
        r_ridge = net_60co[idx_ridge]

        # 능선 우측에서 50% (Half-Max), 20% (Compton Edge Shoulder), 5% (Endpoint) 지점 탐색
        right_q = centers[idx_ridge:35]
        right_r = net_60co[idx_ridge:35]
        q_ce_50 = float(np.interp(0.50 * r_ridge, right_r[::-1], right_q[::-1]))
        q_ce_20 = float(np.interp(0.20 * r_ridge, right_r[::-1], right_q[::-1]))
        q_ce_05 = float(np.interp(0.05 * r_ridge, right_r[::-1], right_q[::-1]))

        # 우주선 뮤온 영역 (Q in [60, 240] pC) 적분 계수율 및 중앙 전하
        mu_mask = (centers >= 60.0) & (centers <= 260.0)
        for s in ["bkg", "137cs", "60co"]:
            mu_rate = float(np.sum(hists[s][mu_mask]) * 2.0)
            mu_mean_q = float(np.sum(centers[mu_mask] * hists[s][mu_mask]) / np.sum(hists[s][mu_mask]))
            print(f"  [{s:5s}] Cosmic-ray region (60~260 pC): Integral Rate = {mu_rate:.3f} Hz, <Q_mu> = {mu_mean_q:.1f} pC")

        print(f"  [60Co Net Gamma] Compton Ridge Peak = {q_ridge:.1f} pC ({r_ridge:.2f} Hz/pC) | "
              f"50% Half-Max = {q_ce_50:.1f} pC | 20% Edge Shoulder = {q_ce_20:.1f} pC | 5% Endpoint = {q_ce_05:.1f} pC")

if __name__ == "__main__":
    main()
