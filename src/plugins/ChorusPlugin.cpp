// "ChorusPlugin.cpp" : copies légèrement désaccordées (plusieurs voix en même temps)
#include "PluginHelper.h"

class ChorusDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        float fs = static_cast<float>(sampleRate);

        for (uint32_t i = 0; i < frameCount; ++i) {
            float lfoA = m_lfoA.Next(0.8f, fs);
            float lfoB = m_lfoB.Next(1.3f, fs);

            for (uint32_t c = 0; c < channels && c < kMaxChannels; ++c) {
                float dry = buffer[i * channels + c];
                m_delays[c].Write(dry);

                // Deux voix supplémentaires, modulées différemment
                float voiceA = m_delays[c].Read((0.020f + 0.005f * lfoA) * fs);
                float voiceB = m_delays[c].Read((0.032f + 0.007f * lfoB) * fs);
                buffer[i * channels + c] = dry * 0.5f + (voiceA + voiceB) * 0.35f;
            }
        }
    }

    void SetParam(int, float) {}

private:
    DelayLine m_delays[kMaxChannels];
    Lfo m_lfoA;
    Lfo m_lfoB;
};

VC_DEFINE_PLUGIN(ChorusDSP, "Chorus (chorale)")
