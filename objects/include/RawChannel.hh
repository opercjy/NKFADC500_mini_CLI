#ifndef RAWCHANNEL_HH
#define RAWCHANNEL_HH

#include "TObject.h"
#include <vector>

class RawChannel : public TObject {
public:
    RawChannel();
    virtual ~RawChannel();

    void Clear(Option_t* opt = "") override;

    void SetChId(int id) { fChId = id; }
    int  GetChId() const { return fChId; }

    // 12-bit ADC masking on sample insertion
    inline void AddSample(unsigned short val) {
        fSamples.push_back(val & 0x0FFF);
    }

    const std::vector<unsigned short>& GetSamples() const { return fSamples; }
    int GetNPoints() const { return static_cast<int>(fSamples.size()); }

    // Physics metrics getters
    double GetBaseline() const { return fBaseline; }
    double GetCharge() const { return fCharge; }
    unsigned short GetPeak() const { return fPeak; }
    double GetAmplitude() const { return fAmplitude; }

    // Raw unsigned sum / max (backward-compatible) and baseline/polarity-aware overload
    void ComputeMetrics();
    void ComputeMetrics(int nPedSamples, int polarity = 0);

private:
    int fChId;
    std::vector<unsigned short> fSamples;
    double fBaseline;
    double fCharge;
    double fAmplitude;
    unsigned short fPeak;

    ClassDefOverride(RawChannel, 3)
};

#endif
