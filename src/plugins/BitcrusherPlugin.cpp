// "BitcrusherPlugin.cpp" : réduction de résolution (voix de vieux jeu vidéo)
#include "PluginHelper.h"
#include <atomic>

class BitcrusherDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t) {
        uint32_t hold = static_cast<uint32_t>(m_hold.load(std::memory_order_relaxed));
        if (hold < 1) hold = 1;
        const float levels = 24.0f; // Nombre de paliers d'amplitude

        for (uint32_t i = 0; i < frameCount; ++i) {
            bool sample = (m_counter == 0);
            m_counter = (m_counter + 1) % hold;

            for (uint32_t c = 0; c < channels && c < kMaxChannels; ++c) {
                float& x = buffer[i * channels + c];
                if (sample) m_held[c] = std::round(x * levels) / levels;
                x = m_held[c];
            }
        }
    }

    void SetParam(int paramId, float value) {
        if (paramId == 0) m_hold.store(value, std::memory_order_relaxed); // Facteur de sous-échantillonnage
    }

private:
    float m_held[kMaxChannels] = {};
    uint32_t m_counter = 0;
    std::atomic<float> m_hold{8.0f};
};

VC_DEFINE_PLUGIN(BitcrusherDSP, "Bitcrusher (8-bit)")
