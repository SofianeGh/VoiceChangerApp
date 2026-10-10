// "PluginHelper.h" : briques communes aux plugins (export de l'API C, filtres, lignes à retard)
#pragma once
#include "PluginAPI.h"
#include <cmath>
#include <cstddef>
#include <vector>

// Nombre maximal de canaux gérés par les plugins (les canaux au-delà ne sont pas traités)
constexpr uint32_t kMaxChannels = 8;
constexpr float kTwoPi = 6.28318530718f;

// Génère l'API C exportée à partir d'une classe DSP qui expose :
//   void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate);
//   void SetParam(int paramId, float value);
#define VC_DEFINE_PLUGIN(DspClass, DisplayName)                                                        \
    extern "C" {                                                                                       \
        __declspec(dllexport) PluginHandle Plugin_Create() { return new DspClass(); }                  \
        __declspec(dllexport) void Plugin_Destroy(PluginHandle handle) {                               \
            delete static_cast<DspClass*>(handle);                                                     \
        }                                                                                              \
        __declspec(dllexport) const char* Plugin_GetName() { return DisplayName; }                     \
        __declspec(dllexport) void Plugin_Process(PluginHandle handle, float* buffer,                  \
                                                  uint32_t frameCount, uint32_t channels,              \
                                                  uint32_t sampleRate) {                               \
            static_cast<DspClass*>(handle)->Process(buffer, frameCount, channels, sampleRate);         \
        }                                                                                              \
        __declspec(dllexport) void Plugin_SetParameter(PluginHandle handle, int paramId, float value) {\
            static_cast<DspClass*>(handle)->SetParam(paramId, value);                                  \
        }                                                                                              \
    }

// Filtre du second ordre (formules RBJ), forme directe II transposée
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void SetLowpass(float sampleRate, float frequency, float q = 0.707f) {
        float w = kTwoPi * frequency / sampleRate;
        float alpha = std::sin(w) / (2.0f * q);
        float c = std::cos(w);
        float a0 = 1.0f + alpha;
        b0 = (1.0f - c) * 0.5f / a0;
        b1 = (1.0f - c) / a0;
        b2 = b0;
        a1 = -2.0f * c / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void SetHighpass(float sampleRate, float frequency, float q = 0.707f) {
        float w = kTwoPi * frequency / sampleRate;
        float alpha = std::sin(w) / (2.0f * q);
        float c = std::cos(w);
        float a0 = 1.0f + alpha;
        b0 = (1.0f + c) * 0.5f / a0;
        b1 = -(1.0f + c) / a0;
        b2 = b0;
        a1 = -2.0f * c / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void SetBandpass(float sampleRate, float frequency, float q) {
        float w = kTwoPi * frequency / sampleRate;
        float alpha = std::sin(w) / (2.0f * q);
        float c = std::cos(w);
        float a0 = 1.0f + alpha;
        b0 = alpha / a0;
        b1 = 0.0f;
        b2 = -alpha / a0;
        a1 = -2.0f * c / a0;
        a2 = (1.0f - alpha) / a0;
    }

    float Process(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Ligne à retard circulaire avec lecture interpolée (allouée une fois, hors du thread audio)
class DelayLine {
public:
    explicit DelayLine(size_t size = 96000) : m_buffer(size, 0.0f), m_writeIndex(0) {}

    void Write(float x) {
        m_buffer[m_writeIndex] = x;
        m_writeIndex = (m_writeIndex + 1) % m_buffer.size();
    }

    // Lit l'échantillon écrit il y a "delay" échantillons (valeur fractionnaire acceptée)
    float Read(float delay) const {
        float maxDelay = static_cast<float>(m_buffer.size() - 2);
        if (delay < 1.0f) delay = 1.0f;
        if (delay > maxDelay) delay = maxDelay;

        float position = static_cast<float>(m_writeIndex) - delay;
        if (position < 0.0f) position += static_cast<float>(m_buffer.size());

        size_t i0 = static_cast<size_t>(position);
        size_t i1 = (i0 + 1) % m_buffer.size();
        float frac = position - static_cast<float>(i0);
        return m_buffer[i0] * (1.0f - frac) + m_buffer[i1] * frac;
    }

private:
    std::vector<float> m_buffer;
    size_t m_writeIndex;
};

// Oscillateur basse fréquence sinusoïdal
struct Lfo {
    float phase = 0.0f;

    // Avance d'un échantillon et renvoie une valeur entre -1 et 1
    float Next(float frequency, float sampleRate) {
        float value = std::sin(phase);
        phase += kTwoPi * frequency / sampleRate;
        if (phase >= kTwoPi) phase -= kTwoPi;
        return value;
    }
};
