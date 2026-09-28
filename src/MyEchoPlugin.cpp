// "MyEchoPlugin.cpp" (Compilé en tant que MyEchoPlugin.dll)
#include "PluginAPI.h"
#include <vector>

// La classe C++ interne au plugin (invisible pour l'hôte)
class EchoDSP {
public:
    EchoDSP() : m_delayBuffer(48000, 0.0f), m_writeIndex(0), m_feedback(0.5f) {}

    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        for (uint32_t i = 0; i < frameCount * channels; ++i) {
            float input = buffer[i];
            float delayed = m_delayBuffer[m_writeIndex];
            
            buffer[i] = input + delayed * m_feedback; // Mix
            
            m_delayBuffer[m_writeIndex] = input + delayed * 0.3f; // Update buffer
            
            m_writeIndex = (m_writeIndex + 1) % m_delayBuffer.size();
        }
    }

    void SetParam(int paramId, float value) {
        if (paramId == 0) m_feedback = value;
    }

private:
    std::vector<float> m_delayBuffer;
    size_t m_writeIndex;
    float m_feedback;
};

// --- Implémentation de l'API C exportée ---

extern "C" {
    __declspec(dllexport) PluginHandle Plugin_Create() {
        return new EchoDSP();
    }

    __declspec(dllexport) void Plugin_Destroy(PluginHandle handle) {
        delete static_cast<EchoDSP*>(handle);
    }

    __declspec(dllexport) const char* Plugin_GetName() {
        return "Simple Echo Plugin";
    }

    __declspec(dllexport) void Plugin_Process(PluginHandle handle, float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        static_cast<EchoDSP*>(handle)->Process(buffer, frameCount, channels, sampleRate);
    }

    __declspec(dllexport) void Plugin_SetParameter(PluginHandle handle, int paramId, float value) {
        static_cast<EchoDSP*>(handle)->SetParam(paramId, value);
    }
}