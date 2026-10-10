// "VibratoPlugin.cpp" : ondulation de la hauteur (voix chevrotante)
#include "PluginHelper.h"
#include <atomic>

class VibratoDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        float fs = static_cast<float>(sampleRate);
        float rate = m_rate.load(std::memory_order_relaxed);

        for (uint32_t i = 0; i < frameCount; ++i) {
            float delay = (0.006f + 0.004f * m_lfo.Next(rate, fs)) * fs;

            for (uint32_t c = 0; c < channels && c < kMaxChannels; ++c) {
                m_delays[c].Write(buffer[i * channels + c]);
                buffer[i * channels + c] = m_delays[c].Read(delay);
            }
        }
    }

    void SetParam(int paramId, float value) {
        if (paramId == 0) m_rate.store(value, std::memory_order_relaxed); // Vitesse (Hz)
    }

private:
    DelayLine m_delays[kMaxChannels];
    Lfo m_lfo;
    std::atomic<float> m_rate{6.0f};
};

VC_DEFINE_PLUGIN(VibratoDSP, "Vibrato")
