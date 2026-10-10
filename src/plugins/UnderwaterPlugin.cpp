// "UnderwaterPlugin.cpp" : aigus étouffés et léger flottement (voix sous l'eau)
#include "PluginHelper.h"

class UnderwaterDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        float fs = static_cast<float>(sampleRate);
        if (sampleRate != m_sampleRate) {
            m_sampleRate = sampleRate;
            for (uint32_t c = 0; c < kMaxChannels; ++c) {
                for (int s = 0; s < 2; ++s) m_lowpass[c][s].SetLowpass(fs, 450.0f, 1.2f);
            }
        }

        for (uint32_t i = 0; i < frameCount; ++i) {
            float wobble = m_lfo.Next(2.5f, fs);
            float delay = (0.008f + 0.003f * wobble) * fs;
            float gain = 1.2f * (1.0f + 0.25f * wobble);

            for (uint32_t c = 0; c < channels && c < kMaxChannels; ++c) {
                float x = buffer[i * channels + c];
                for (int s = 0; s < 2; ++s) x = m_lowpass[c][s].Process(x);
                m_delays[c].Write(x);
                buffer[i * channels + c] = m_delays[c].Read(delay) * gain;
            }
        }
    }

    void SetParam(int, float) {}

private:
    Biquad m_lowpass[kMaxChannels][2];
    DelayLine m_delays[kMaxChannels];
    Lfo m_lfo;
    uint32_t m_sampleRate = 0;
};

VC_DEFINE_PLUGIN(UnderwaterDSP, "Sous l'eau")
