#include "RawChannel.hh"
#include <algorithm>

ClassImp(RawChannel)

RawChannel::RawChannel()
    : fChId(-1), fBaseline(0.0), fCharge(0.0), fAmplitude(0.0), fPeak(0) {
    fSamples.reserve(2048);
}

RawChannel::~RawChannel() {}

void RawChannel::Clear(Option_t* /*opt*/) {
    fChId = -1;
    fSamples.clear();
    fBaseline = 0.0;
    fCharge = 0.0;
    fAmplitude = 0.0;
    fPeak = 0;
}

void RawChannel::ComputeMetrics() {
    ComputeMetrics(20, 0);
}

void RawChannel::ComputeMetrics(int nPedSamples, int polarity) {
    fBaseline = 0.0;
    fCharge = 0.0;
    fAmplitude = 0.0;
    fPeak = 0;

    if (fSamples.empty()) return;

    const int nPts = static_cast<int>(fSamples.size());
    const int nPed = std::clamp(nPedSamples, 1, nPts);
    double pedSum = 0.0;
    for (int i = 0; i < nPed; ++i) {
        pedSum += static_cast<double>(fSamples[i]);
    }
    fBaseline = pedSum / static_cast<double>(nPed);

    for (unsigned short val : fSamples) {
        if (val > fPeak) fPeak = val;
        const double pulse = (polarity == 0)
            ? (fBaseline - static_cast<double>(val))
            : (static_cast<double>(val) - fBaseline);
        if (pulse > 0.0) fCharge += pulse;
        if (pulse > fAmplitude) fAmplitude = pulse;
    }
}
