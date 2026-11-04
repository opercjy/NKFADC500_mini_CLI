#include "Fadc500Device.hh"
#include "ELog.hh"
#include "NoticeFadc500Spec.hh"

extern "C" {
    #include "usb3com.h"
    #include "NoticeNKFADC500.h"
}

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <unistd.h>
#include <vector>

Fadc500Device::Fadc500Device(int sid) : fSid(sid), fIsOpen(false) {
    USB3Init(nullptr);
    int status = NKFADC500open(fSid, nullptr);
    if (status < 0) {
        USB3Exit(nullptr);
        ELog::Print(ELog::FATAL, Form("Cannot open FADC500 Mini (MID: %d). Please check power, udev rules, and USB 3.0 cable!", fSid));
    }
    fIsOpen = true;
}

Fadc500Device::~Fadc500Device() {
    if (fIsOpen) {
        NKFADC500stop(fSid);
        NKFADC500close(fSid);
        USB3Exit(nullptr);
        fIsOpen = false;
    }
}

bool Fadc500Device::EnableDramWithTimeout(bool enable) {
    constexpr unsigned long kRegDramOn = 0x20000003UL;
    if (!enable) {
        USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, fSid, kRegDramOn, 0);
        return true;
    }

    // Replicates NoticeNKFADC500.c:105-119 with a bounded timeout instead of an infinite while(!status) loop
    const unsigned long current = USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, fSid, kRegDramOn);
    if (current != 0UL && current != 0xFFFFFFFFUL) {
        USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, fSid, kRegDramOn, 0);
        usleep(1000);
    }
    USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, fSid, kRegDramOn, 1);

    for (int attempt = 0; attempt < 2000; ++attempt) {
        const unsigned long status = USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, fSid, kRegDramOn);
        if (status != 0UL && status != 0xFFFFFFFFUL) {
            return true;
        }
        usleep(1000);
    }

    ELog::Print(ELog::ERROR, Form("[HW INIT] DRAM controller (MID: %d) did not report ready within 2.0 s!", fSid));
    return false;
}

void Fadc500Device::SafeAlignADC500() {
    // Replicates vendor NKFADC500_ADCALIGN_500 (NoticeNKFADC500.c:545-594) while fixing
    // the count==0 divide-by-zero SIGFPE crash at NoticeNKFADC500.c:579.
    NKFADC500send_ADCRST(fSid);
    usleep(500000);
    NKFADC500send_ADCCAL(fSid);
    NKFADC500write_ADCALIGN(fSid, 1);

    std::array<unsigned long, 4> delays = {0, 0, 0, 0};
    for (unsigned long ch = 1; ch <= 4; ++ch) {
        int count = 0;
        int sum = 0;
        int flag = 0;

        for (unsigned long dly = 0; dly < 32; ++dly) {
            NKFADC500write_ADCDLY(fSid, ch, dly);
            const unsigned long value = (NKFADC500read_ADCSTAT(fSid) >> (ch - 1)) & 0x1UL;
            if (!value) {
                flag = 1;
                count += 1;
                sum += static_cast<int>(dly);
            } else if (flag) {
                break;
            }
        }

        unsigned long gdly = 16;
        if (count > 0) {
            const int center = sum / count;
            gdly = static_cast<unsigned long>((center < 11) ? (center + 11) : (center - 11));
        } else {
            ELog::Print(ELog::WARNING,
                Form("[ADC ALIGN] Ch%lu reported 0 bad taps across 0..31; using safe midpoint tap %lu.", ch, gdly));
        }

        NKFADC500write_ADCDLY(fSid, ch, gdly);
        delays[ch - 1] = gdly;
    }

    NKFADC500write_ADCALIGN(fSid, 0);
    NKFADC500send_ADCCAL(fSid);

    ELog::Print(ELog::INFO,
        Form("[ADC ALIGN] 500 MS/s ADC tap delays set: Ch1=%lu, Ch2=%lu, Ch3=%lu, Ch4=%lu",
             delays[0], delays[1], delays[2], delays[3]));
}

bool Fadc500Device::SafeAlignDRAM() {
    // Replicates vendor NKFADC500_ADCALIGN_DRAM (NoticeNKFADC500.c:597-696) with explicit lane status verification
    if (!EnableDramWithTimeout(true)) {
        return false;
    }

    NKFADC500write_DRAMTEST(fSid, 1);
    NKFADC500send_ADCCAL(fSid);
    NKFADC500write_DRAMTEST(fSid, 2);

    int alignedLanes = 0;
    for (unsigned long lane = 0; lane < 8; ++lane) {
        int count = 0;
        int sum = 0;
        int flag = 0;

        for (unsigned long dly = 0; dly < 32; ++dly) {
            NKFADC500write_DRAMDLY(fSid, lane, dly);
            NKFADC500write_DRAMTEST(fSid, 3);
            const unsigned long pattern = NKFADC500read_DRAMTEST(fSid, lane);

            const bool match =
                (pattern == 0xFFAA5500UL) || (pattern == 0xAA5500FFUL) ||
                (pattern == 0x5500FFAAUL) || (pattern == 0x00FFAA55UL);

            if (match) {
                count += 1;
                sum += static_cast<int>(dly);
                if (count > 4) flag = 1;
            } else if (flag) {
                break;
            } else {
                count = 0;
                sum = 0;
            }
        }

        const unsigned long gdly = (count > 0) ? static_cast<unsigned long>(sum / count) : 9UL;
        NKFADC500write_DRAMDLY(fSid, lane, gdly);

        bool laneAligned = false;
        for (int bitslip = 0; bitslip < 4; ++bitslip) {
            NKFADC500write_DRAMTEST(fSid, 3);
            const unsigned long pattern = NKFADC500read_DRAMTEST(fSid, lane);
            if (pattern == 0xFFAA5500UL) {
                laneAligned = true;
                break;
            }
            NKFADC500write_BITSLIP(fSid, lane);
        }

        if (laneAligned) {
            alignedLanes++;
        } else {
            ELog::Print(ELog::ERROR, Form("[DRAM ALIGN] Failed to align DRAM byte-lane %lu (tap=%lu)!", lane, gdly));
        }
    }

    NKFADC500write_DRAMTEST(fSid, 0);
    ELog::Print((alignedLanes == 8) ? ELog::INFO : ELog::WARNING,
                Form("[DRAM ALIGN] %d / 8 onboard DRAM byte-lanes aligned.", alignedLanes));
    return alignedLanes == 8;
}

void Fadc500Device::VerifyRegistersAndLogPedestals(FadcBD* bdConfig) {
    if (!bdConfig) return;

    const unsigned long rb_dsr = NKFADC500read_DSR(fSid);
    const unsigned long rb_rl  = NKFADC500read_RL(fSid);
    const unsigned long rb_dram = NKFADC500read_DRAMON(fSid);
    const unsigned long rb_tlt = NKFADC500read_TLT(fSid);
    const unsigned long rb_ps  = NKFADC500read_PSCALE(fSid);
    const unsigned long rb_te  = NKFADC500read_TRIGENABLE(fSid);

    if (rb_dsr != static_cast<unsigned long>(bdConfig->GetSAMPLING()) ||
        rb_rl  != static_cast<unsigned long>(bdConfig->GetRL()) ||
        rb_dram == 0UL || rb_dram == 0xFFFFFFFFUL) {
        ELog::Print(ELog::WARNING,
            Form("[HW VERIFY] Global register mismatch (DSR=%lu [cfg %d], RL=%lu [cfg %d], DRAMON=%lu, TLT=0x%lX, PSCALE=%lu, TRIGEN=0x%lX).",
                 rb_dsr, bdConfig->GetSAMPLING(), rb_rl, bdConfig->GetRL(), rb_dram, rb_tlt, rb_ps, rb_te));
    }

    std::ostringstream pedLog;
    pedLog << "[HW VERIFY] Pedestals (ADC): ";
    for (int ch = 0; ch < bdConfig->NCHANNEL(); ++ch) {
        const unsigned long cid = static_cast<unsigned long>(ch + 1);
        const unsigned long ped = NKFADC500read_PED(fSid, cid);
        const unsigned long rb_thr = NKFADC500read_THR(fSid, cid);
        const unsigned long rb_dly = NKFADC500read_DLY(fSid, cid);
        const unsigned long rb_tm  = NKFADC500read_TM(fSid, cid);
        if (ped != 0xFFFFFFFFUL) {
            bdConfig->SetPED(ch, static_cast<int>(ped & 0x0FFFUL));
        }

        const unsigned long exp_dly = std::min<unsigned long>(
            static_cast<unsigned long>(NoticeFadc500Spec::kMaxDlyNs),
            static_cast<unsigned long>(std::max(0, bdConfig->GetCW(ch)) + std::max(0, bdConfig->GetDLY(ch))));

        // Legacy NKFADC500 Mini FPGA quantizes DLY (0x20000007) to 8 ns steps (& ~0x7) and adds a fixed
        // +80 ns (+0x50 = 10 clocks @ 125 MHz) internal pipeline offset to the 16-bit register word:
        // raw_read = (raw_written & ~0x7UL) + 0x50UL.
        const unsigned long raw_dly_written = ((exp_dly / 1000UL) << 10) | (exp_dly % 1000UL);
        const unsigned long raw_dly_hw_exp  = (raw_dly_written & ~0x7UL) + 0x50UL;
        const unsigned long exp_dly_hw_api  = ((raw_dly_hw_exp >> 10) * 1000UL) + (raw_dly_hw_exp & 0x3FFUL);
        const bool dly_ok = (rb_dly == exp_dly) || (rb_dly == exp_dly_hw_api);

        if (rb_thr != static_cast<unsigned long>(bdConfig->GetTHR(ch)) ||
            !dly_ok ||
            rb_tm  != static_cast<unsigned long>(bdConfig->GetTMODE(ch))) {
            ELog::Print(ELog::WARNING,
                Form("[HW VERIFY] Ch%lu readback mismatch: THR=%lu(%d), DLY=%lu(exp %lu / hw %lu), TM=%lu(%d)",
                     cid, rb_thr, bdConfig->GetTHR(ch), rb_dly, exp_dly, exp_dly_hw_api, rb_tm, bdConfig->GetTMODE(ch)));
        }

        if (ped != 0xFFFFFFFFUL && (ped <= 10UL || ped >= 4085UL)) {
            ELog::Print(ELog::WARNING,
                Form("[HW VERIFY] Ch%lu pedestal (%lu ADC) is near 12-bit rail [0..4095]! Adjust DACOFF (%d) for POL=%d.",
                     cid, ped, bdConfig->GetDACOFF(ch), bdConfig->GetPOL(ch)));
        }

        if (ch > 0) pedLog << " | ";
        pedLog << "Ch" << ch << "=" << ped;
    }
    ELog::Print(ELog::INFO, pedLog.str());
}

void Fadc500Device::Initialize(FadcBD* bdConfig) {
    if (!bdConfig) {
        ELog::Print(ELog::FATAL, "Fadc500Device::Initialize called with null FadcBD configuration.");
        return;
    }

    ELog::Print(ELog::INFO, Form("Initializing FADC500 Mini (MID: %d) with Vendor-Verified Sequence...", fSid));

    // Drain any residual FIFO/DRAM bytes prior to ADC/DRAM clock alignment
    ClearAndFlushUSB();

    // Exact vendor register ordering from notice/test/mini_nkfadc500/nkfadc500_4_test.C:112-175
    const int normDsr = NoticeFadc500Spec::NormalizeSamplingDivider(bdConfig->GetSAMPLING());
    const int normRl  = NoticeFadc500Spec::NormalizeRecordLengthCode(bdConfig->GetRL());
    bdConfig->SetSAMPLING(normDsr);
    bdConfig->SetRL(normRl);

    NKFADC500write_DSR(fSid, static_cast<unsigned long>(normDsr));
    NKFADC500resetTIMER(fSid);
    NKFADC500reset(fSid);
    SafeAlignADC500();
    SafeAlignDRAM();

    NKFADC500write_PTRIG(fSid, static_cast<unsigned long>(
        std::clamp(bdConfig->GetPTRIG(), 0, NoticeFadc500Spec::kMaxPtrigMs)));

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        const unsigned long cid = static_cast<unsigned long>(ch + 1);
        const int cw = std::clamp(bdConfig->GetCW(ch), NoticeFadc500Spec::kMinCwNs, NoticeFadc500Spec::kMaxCwNs);
        bdConfig->SetCW(ch, cw);
        NKFADC500write_CW(fSid, cid, static_cast<unsigned long>(cw));
    }

    NKFADC500write_RL(fSid, static_cast<unsigned long>(normRl));
    EnableDramWithTimeout(true);

    // Explicitly program ZEROSUP register (0x20000016) so stale state from prior sessions never persists
    NKFADC500write_ZEROSUP(fSid, 1, static_cast<unsigned long>(std::clamp(bdConfig->GetZEROSUP(), 0, 0x0F)));

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        const unsigned long cid = static_cast<unsigned long>(ch + 1);
        const int dacoff = std::clamp(bdConfig->GetDACOFF(ch), 0, NoticeFadc500Spec::kMaxDacOff);
        bdConfig->SetDACOFF(ch, dacoff);
        NKFADC500write_DACOFF(fSid, cid, static_cast<unsigned long>(dacoff));
    }

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        NKFADC500measure_PED(fSid, static_cast<unsigned long>(ch + 1));
    }
    usleep(10000);

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        const unsigned long cid = static_cast<unsigned long>(ch + 1);
        const int tmode = std::clamp(bdConfig->GetTMODE(ch), 0, NoticeFadc500Spec::kMaxTmodeMask);
        const int maxThr = NoticeFadc500Spec::IsPeakSumTriggerMode(tmode)
            ? NoticeFadc500Spec::kMaxThrPeakSum
            : NoticeFadc500Spec::kMaxThrHeight;
        const int thr = std::clamp(bdConfig->GetTHR(ch), NoticeFadc500Spec::kMinThrHeight, maxThr);

        // Guard against 5-bit coarse delay register overflow in NoticeNKFADC500.c:179 ((data/1000)<<10)
        const unsigned long apply_dly = std::min<unsigned long>(
            static_cast<unsigned long>(NoticeFadc500Spec::kMaxDlyNs),
            static_cast<unsigned long>(std::max(0, bdConfig->GetCW(ch)) + std::max(0, bdConfig->GetDLY(ch))));

        NKFADC500write_DLY(fSid, cid, apply_dly);
        NKFADC500write_THR(fSid, cid, static_cast<unsigned long>(thr));
        NKFADC500write_POL(fSid, cid, static_cast<unsigned long>(std::clamp(bdConfig->GetPOL(ch), 0, 1)));
        NKFADC500write_PSW(fSid, cid, static_cast<unsigned long>(
            std::clamp(bdConfig->GetPSW(ch), NoticeFadc500Spec::kMinPswNs, NoticeFadc500Spec::kMaxPswNs)));
        NKFADC500write_AMODE(fSid, cid, static_cast<unsigned long>(std::clamp(bdConfig->GetAMODE(ch), 0, 1)));
        NKFADC500write_PCT(fSid, cid, static_cast<unsigned long>(
            std::clamp(bdConfig->GetPCT(ch), NoticeFadc500Spec::kMinPct, NoticeFadc500Spec::kMaxPct)));
        NKFADC500write_PCI(fSid, cid, static_cast<unsigned long>(
            std::clamp(bdConfig->GetPCI(ch), NoticeFadc500Spec::kMinPciNs, NoticeFadc500Spec::kMaxPciNs)));
        NKFADC500write_PWT(fSid, cid, static_cast<unsigned long>(
            std::clamp(bdConfig->GetPWT(ch), NoticeFadc500Spec::kMinPwtNs, NoticeFadc500Spec::kMaxPwtNs)));
        NKFADC500write_DT(fSid, cid, static_cast<unsigned long>(
            std::clamp(bdConfig->GetDT(ch), 0, NoticeFadc500Spec::kMaxDtNs)));
        NKFADC500write_TM(fSid, cid, static_cast<unsigned long>(tmode));
    }

    NKFADC500write_TLT(fSid, static_cast<unsigned long>(std::clamp(bdConfig->GetTLT(), 0, 0xFFFF)));
    NKFADC500write_PSCALE(fSid, static_cast<unsigned long>(
        std::clamp(bdConfig->GetPRESCALE(), NoticeFadc500Spec::kMinPrescale, NoticeFadc500Spec::kMaxPrescale)));
    NKFADC500write_TRIGENABLE(fSid, static_cast<unsigned long>(
        std::clamp(bdConfig->GetTRIGEN(), 0, NoticeFadc500Spec::kMaxTrigEnMask)));

    VerifyRegistersAndLogPedestals(bdConfig);

    NKFADC500reset(fSid);
    ELog::Print(ELog::INFO, "Hardware initialization complete.");
}

void Fadc500Device::ClearAndFlushUSB() {
    NKFADC500stop(fSid);
    // Allow any in-flight triggered event in the FPGA delay/digitizer pipeline (CW+DLY+RL <= 163 us)
    // to finish committing to SDRAM before reading BCOUNT or resetting pointers.
    usleep(2000);

    // IMPORTANT: Do NOT mask raw_bcount with 0xFFFF!
    // Full 64 MB onboard DRAM reports 65,536 KB = 0x10000 (NoticeNKFADC500.c:538,709).
    unsigned long raw_bcount = NKFADC500read_BCOUNT(fSid);

    if (raw_bcount != 0xFFFFFFFFUL && raw_bcount > 0UL) {
        unsigned int bcount_kb = static_cast<unsigned int>(
            std::min<unsigned long>(raw_bcount, NoticeFadc500Spec::kMaxDramKb));

        ELog::Print(ELog::WARNING, Form("[HW FLUSH] Draining %u KB residual DRAM/FIFO buffer...", bcount_kb));

        constexpr unsigned int chunk_kb = 1024U;
        std::vector<char> safe_dummy(chunk_kb * 1024ULL);

        for (int iter = 0; iter < 128 && bcount_kb > 0U; ++iter) {
            const unsigned int read_kb = std::min(bcount_kb, chunk_kb);
            NKFADC500read_DATA(fSid, static_cast<int>(read_kb), safe_dummy.data());

            raw_bcount = NKFADC500read_BCOUNT(fSid);
            if (raw_bcount == 0xFFFFFFFFUL || raw_bcount == 0UL) break;

            const unsigned int next_bcount = static_cast<unsigned int>(
                std::min<unsigned long>(raw_bcount, NoticeFadc500Spec::kMaxDramKb));
            if (next_bcount >= bcount_kb) break; // Guard against non-draining USB fault
            bcount_kb = next_bcount;
        }
    }

    NKFADC500resetTIMER(fSid);
    NKFADC500reset(fSid);
    usleep(1000);

    // Second-pass check after FPGA DAQ reset in case prefetched FX3 FIFO blocks remained
    raw_bcount = NKFADC500read_BCOUNT(fSid);
    if (raw_bcount != 0xFFFFFFFFUL && raw_bcount > 0UL) {
        const unsigned int rem_kb = static_cast<unsigned int>(
            std::min<unsigned long>(raw_bcount, 1024UL));
        std::vector<char> safe_dummy(rem_kb * 1024ULL);
        NKFADC500read_DATA(fSid, static_cast<int>(rem_kb), safe_dummy.data());
        NKFADC500resetTIMER(fSid);
        NKFADC500reset(fSid);
    }
}

void Fadc500Device::StartDAQ() {
    ClearAndFlushUSB();
    NKFADC500start(fSid);
}

void Fadc500Device::StopAcquisitionOnly() {
    NKFADC500stop(fSid);
}

void Fadc500Device::StopDAQ() {
    NKFADC500stop(fSid);
    ClearAndFlushUSB();
}

unsigned int Fadc500Device::ReadBCOUNT() {
    const unsigned long raw = NKFADC500read_BCOUNT(fSid);
    if (raw == 0xFFFFFFFFUL) {
        return NoticeFadc500Spec::kUsbReadErrorWord;
    }
    return static_cast<unsigned int>(std::min<unsigned long>(raw, NoticeFadc500Spec::kMaxDramKb));
}

void Fadc500Device::ReadDATA(unsigned int bcount_kb, unsigned char* dest) {
    if (bcount_kb > 0 && dest != nullptr) {
        NKFADC500read_DATA(fSid, static_cast<int>(bcount_kb), reinterpret_cast<char*>(dest));
    }
}
