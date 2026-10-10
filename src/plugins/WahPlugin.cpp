// "WahPlugin.cpp" : filtre résonant qui balaie les médiums (effet wah-wah)
#include "PluginHelper.h"

class WahDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        float fs = static_cast<float>(sampleRate);

        for (uint32_t i = 0; i < frameCount; ++i) {
            // Les coefficients sont recalculés par blocs de 32 échantillons, suffisant pour un balayage lent
            float sweep = m_lfo.Next(1.8f, fs);
            if (m_counter == 0) {
                float frequency = 500.0f * std::pow(5.0f, 0.5f + 0.5f * sweep); // 500 Hz à 2500 Hz
                for (uint32_t c = 0; c < kMaxChannels; ++c) m_filters[c].SetBandpass(fs, frequency, 3.0f);
            }
            m_counter = (m_counter + 1) % 32;

            for (uint32_t c = 0; c < channels && c < kMaxChannels; ++c) {
                float dry = buffer[i * channels + c];
                buffer[i * channels + c] = dry * 0.2f + m_filters[c].Process(dry) * 2.5f;
            }
        }
    }

    void SetParam(int, float) {}

private:
    Biquad m_filters[kMaxChannels];
    Lfo m_lfo;
    uint32_t m_counter = 0;
};

VC_DEFINE_PLUGIN(WahDSP, "Wah-wah")
