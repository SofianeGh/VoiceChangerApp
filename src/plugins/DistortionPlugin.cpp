// "DistortionPlugin.cpp" : saturation douce (voix saturée / rock)
#include "PluginHelper.h"
#include <atomic>

class DistortionDSP {
public:
    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t) {
        float drive = m_drive.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < frameCount * channels; ++i) {
            buffer[i] = std::tanh(buffer[i] * drive) * 0.3f;
        }
    }

    void SetParam(int paramId, float value) {
        if (paramId == 0) m_drive.store(value, std::memory_order_relaxed); // Gain d'entrée
    }

private:
    std::atomic<float> m_drive{10.0f};
};

VC_DEFINE_PLUGIN(DistortionDSP, "Distorsion")
