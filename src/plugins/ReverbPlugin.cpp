// "ReverbPlugin.cpp" : réverbération de Schroeder (grande salle / cathédrale)
#include "PluginHelper.h"
#include <atomic>

class ReverbDSP {
public:
    ReverbDSP() {
        // Longueurs premières entre elles (en échantillons à 48 kHz) pour éviter les résonances
        static const size_t combSizes[kCombs] = {1687, 1601, 2053, 2251};
        static const size_t allpassSizes[kAllpasses] = {347, 113};
        for (int i = 0; i < kCombs; ++i) m_combs[i].assign(combSizes[i], 0.0f);
        for (int i = 0; i < kAllpasses; ++i) m_allpasses[i].assign(allpassSizes[i], 0.0f);
    }

    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t) {
        float mix = m_mix.load(std::memory_order_relaxed);
        const float feedback = 0.86f;
        const float damping = 0.25f;

        for (uint32_t i = 0; i < frameCount; ++i) {
            float input = 0.0f;
            for (uint32_t c = 0; c < channels; ++c) input += buffer[i * channels + c];
            input /= static_cast<float>(channels);

            // Filtres en peigne en parallèle
            float wet = 0.0f;
            for (int k = 0; k < kCombs; ++k) {
                float delayed = m_combs[k][m_combIndex[k]];
                m_combState[k] = delayed * (1.0f - damping) + m_combState[k] * damping;
                m_combs[k][m_combIndex[k]] = input + m_combState[k] * feedback;
                m_combIndex[k] = (m_combIndex[k] + 1) % m_combs[k].size();
                wet += delayed;
            }
            wet *= 0.25f;

            // Filtres passe-tout en série
            for (int k = 0; k < kAllpasses; ++k) {
                float delayed = m_allpasses[k][m_allpassIndex[k]];
                float out = delayed - 0.5f * wet;
                m_allpasses[k][m_allpassIndex[k]] = wet + 0.5f * out;
                m_allpassIndex[k] = (m_allpassIndex[k] + 1) % m_allpasses[k].size();
                wet = out;
            }

            for (uint32_t c = 0; c < channels; ++c) {
                buffer[i * channels + c] = buffer[i * channels + c] * (1.0f - mix * 0.5f) + wet * mix;
            }
        }
    }

    void SetParam(int paramId, float value) {
        if (paramId == 0) m_mix.store(value, std::memory_order_relaxed); // Dosage (0.0 à 1.0)
    }

private:
    static constexpr int kCombs = 4;
    static constexpr int kAllpasses = 2;

    std::vector<float> m_combs[kCombs];
    size_t m_combIndex[kCombs] = {};
    float m_combState[kCombs] = {};
    std::vector<float> m_allpasses[kAllpasses];
    size_t m_allpassIndex[kAllpasses] = {};
    std::atomic<float> m_mix{0.45f};
};

VC_DEFINE_PLUGIN(ReverbDSP, "Reverb (cathedrale)")
