// "TremoloPlugin.cpp" : variation rapide du volume (voix tremblante)
#include "PluginHelper.h"
#include <atomic>

class TremoloDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        float rate = m_rate.load(std::memory_order_relaxed);
        const float depth = 0.85f;

        for (uint32_t i = 0; i < frameCount; ++i) {
            float gain = 1.0f - depth * (0.5f + 0.5f * m_lfo.Next(rate, static_cast<float>(sampleRate)));
            for (uint32_t c = 0; c < channels; ++c) {
                buffer[i * channels + c] *= gain;
            }
        }
    }

    void SetParam(int paramId, float value) {
        if (paramId == 0) m_rate.store(value, std::memory_order_relaxed); // Vitesse (Hz)
    }

private:
    Lfo m_lfo;
    std::atomic<float> m_rate{7.0f};
};

VC_DEFINE_PLUGIN(TremoloDSP, "Tremolo")
