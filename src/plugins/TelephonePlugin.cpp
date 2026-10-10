// "TelephonePlugin.cpp" : bande passante étroite (voix au téléphone)
#include "PluginHelper.h"

class TelephoneDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        if (sampleRate != m_sampleRate) {
            m_sampleRate = sampleRate;
            for (uint32_t c = 0; c < kMaxChannels; ++c) {
                for (int s = 0; s < 2; ++s) {
                    m_highpass[c][s].SetHighpass(static_cast<float>(sampleRate), 400.0f);
                    m_lowpass[c][s].SetLowpass(static_cast<float>(sampleRate), 3000.0f);
                }
            }
        }

        for (uint32_t i = 0; i < frameCount; ++i) {
            for (uint32_t c = 0; c < channels && c < kMaxChannels; ++c) {
                float x = buffer[i * channels + c];
                for (int s = 0; s < 2; ++s) {
                    x = m_lowpass[c][s].Process(m_highpass[c][s].Process(x));
                }
                buffer[i * channels + c] = std::tanh(x * 2.5f) * 0.7f;
            }
        }
    }

    void SetParam(int, float) {}

private:
    Biquad m_highpass[kMaxChannels][2];
    Biquad m_lowpass[kMaxChannels][2];
    uint32_t m_sampleRate = 0;
};

VC_DEFINE_PLUGIN(TelephoneDSP, "Telephone")
