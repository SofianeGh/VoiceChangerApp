// "MegaphonePlugin.cpp" : médiums agressifs et écrêtage dur (mégaphone / talkie-walkie)
#include "PluginHelper.h"
#include <algorithm>

class MegaphoneDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        if (sampleRate != m_sampleRate) {
            m_sampleRate = sampleRate;
            for (uint32_t c = 0; c < kMaxChannels; ++c) {
                m_highpass[c].SetHighpass(static_cast<float>(sampleRate), 700.0f);
                m_lowpass[c].SetLowpass(static_cast<float>(sampleRate), 2400.0f);
                m_peak[c].SetBandpass(static_cast<float>(sampleRate), 1500.0f, 1.5f);
            }
        }

        for (uint32_t i = 0; i < frameCount; ++i) {
            for (uint32_t c = 0; c < channels && c < kMaxChannels; ++c) {
                float x = m_lowpass[c].Process(m_highpass[c].Process(buffer[i * channels + c]));
                x += m_peak[c].Process(x) * 1.5f;
                buffer[i * channels + c] = std::clamp(x * 8.0f, -0.6f, 0.6f) * 0.6f;
            }
        }
    }

    void SetParam(int, float) {}

private:
    Biquad m_highpass[kMaxChannels];
    Biquad m_lowpass[kMaxChannels];
    Biquad m_peak[kMaxChannels];
    uint32_t m_sampleRate = 0;
};

VC_DEFINE_PLUGIN(MegaphoneDSP, "Megaphone")
