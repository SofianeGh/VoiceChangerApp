// "NoiseGatePlugin.cpp" : coupe le son sous un seuil (supprime le bruit de fond entre les phrases)
#include "PluginHelper.h"
#include <algorithm>
#include <atomic>

class NoiseGateDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        float fs = static_cast<float>(sampleRate);
        float threshold = m_threshold.load(std::memory_order_relaxed);
        float envelopeRelease = std::exp(-1.0f / (0.050f * fs));
        float gainAttack = 1.0f - std::exp(-1.0f / (0.003f * fs));
        float gainRelease = 1.0f - std::exp(-1.0f / (0.120f * fs));

        for (uint32_t i = 0; i < frameCount; ++i) {
            float peak = 0.0f;
            for (uint32_t c = 0; c < channels; ++c) peak = std::max(peak, std::fabs(buffer[i * channels + c]));

            // Suiveur d'enveloppe : montée instantanée, descente lente
            m_envelope = std::max(peak, m_envelope * envelopeRelease);

            float target = m_envelope > threshold ? 1.0f : 0.0f;
            m_gain += (target - m_gain) * (target > m_gain ? gainAttack : gainRelease);

            for (uint32_t c = 0; c < channels; ++c) buffer[i * channels + c] *= m_gain;
        }
    }

    void SetParam(int paramId, float value) {
        if (paramId == 0) m_threshold.store(value, std::memory_order_relaxed); // Seuil (0.0 à 1.0)
    }

private:
    float m_envelope = 0.0f;
    float m_gain = 0.0f;
    std::atomic<float> m_threshold{0.02f};
};

VC_DEFINE_PLUGIN(NoiseGateDSP, "Noise gate (anti-bruit)")
