// "VoiceType.h" : base commune des plugins de type de voix (homme, femme, enfant)
#pragma once
#include "PluginHelper.h"
#include <algorithm>
#include <atomic>

// Transforme la voix vers une voix cible définie par sa hauteur moyenne (Hz) et son timbre.
//
// Principe (PSOLA) : la voix est découpée en grains de deux périodes, calés sur la hauteur détectée.
//  - l'espacement des grains en sortie fixe la nouvelle hauteur ;
//  - la vitesse de lecture de chaque grain déplace les formants (le timbre), indépendamment de la hauteur.
// La hauteur moyenne de la personne qui parle est mesurée en continu : le plugin s'adapte donc à la voix
// d'origine au lieu d'appliquer un décalage fixe.
class VoiceTypeDSP {
public:
    // targetPitchHz : hauteur moyenne visée ; targetFormant : taille du conduit vocal visé (1.0 = homme adulte)
    VoiceTypeDSP(float targetPitchHz, float targetFormant)
        : m_input(kInputSize, 0.0f), m_low(kLowSize, 0.0f),
          m_targetPitch(targetPitchHz), m_targetFormant(targetFormant) {}

    void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
        if (sampleRate != m_sampleRate) Configure(sampleRate);

        for (uint32_t i = 0; i < frameCount; ++i) {
            float mono = 0.0f;
            for (uint32_t c = 0; c < channels; ++c) mono += buffer[i * channels + c];
            mono /= static_cast<float>(channels);

            m_input[m_writePos & (kInputSize - 1)] = mono;

            // Copie sous-échantillonnée pour la détection de hauteur
            m_decimSum += mono;
            if (++m_decimCount >= m_decim) {
                m_low[m_lowPos++ & (kLowSize - 1)] = m_decimSum / static_cast<float>(m_decim);
                m_decimSum = 0.0f;
                m_decimCount = 0;
            }
            if (++m_hopCount >= kHop) {
                m_hopCount = 0;
                AnalysePitch();
            }

            m_nextGrain -= 1.0f;
            if (m_nextGrain <= 0.0f) {
                StartGrain();
                m_nextGrain += m_period / m_pitchRatio;
            }

            float out = 0.0f;
            for (Grain& grain : m_grains) {
                if (!grain.active) continue;
                float window = 0.5f - 0.5f * std::cos(kTwoPi * grain.progress);
                out += window * ReadInput(grain.position);
                grain.position += grain.speed;
                grain.progress += grain.progressStep;
                if (grain.progress >= 1.0f) grain.active = false;
            }
            out *= m_gain;

            for (uint32_t c = 0; c < channels; ++c) buffer[i * channels + c] = out;
            ++m_writePos;
        }
    }

    void SetParam(int paramId, float value) {
        if (paramId == 0) m_targetPitch.store(value, std::memory_order_relaxed);        // Hauteur visée (Hz)
        else if (paramId == 1) m_targetFormant.store(value, std::memory_order_relaxed); // Timbre visé
    }

private:
    static constexpr size_t kInputSize = 16384; // Puissances de deux : index circulaire par masque
    static constexpr size_t kLowSize = 2048;
    static constexpr uint32_t kHop = 256;       // Une analyse de hauteur toutes les 256 frames
    static constexpr int kMaxGrains = 8;
    static constexpr int kMaxAnalysis = 1024;
    static constexpr float kMinPitch = 70.0f;
    static constexpr float kMaxPitch = 500.0f;

    struct Grain {
        bool active = false;
        double position = 0.0;   // Position de lecture dans l'entrée (en frames absolues)
        double speed = 1.0;      // Vitesse de lecture = décalage des formants
        float progress = 0.0f;   // Avancement dans la fenêtre (0 à 1)
        float progressStep = 0.0f;
    };

    void Configure(uint32_t sampleRate) {
        m_sampleRate = sampleRate;
        m_decim = std::max<uint32_t>(1, (sampleRate + 6000) / 12000); // Analyse autour de 12 kHz
        m_lowRate = static_cast<float>(sampleRate) / static_cast<float>(m_decim);
        m_period = static_cast<float>(sampleRate) / 150.0f;
    }

    float ReadInput(double position) const {
        int64_t index = static_cast<int64_t>(position);
        float frac = static_cast<float>(position - static_cast<double>(index));
        float a = m_input[static_cast<uint64_t>(index) & (kInputSize - 1)];
        float b = m_input[static_cast<uint64_t>(index + 1) & (kInputSize - 1)];
        return a + (b - a) * frac;
    }

    // Taille relative du conduit vocal estimée à partir de la hauteur moyenne de la voix d'origine
    static float SourceFormant(float pitchHz) {
        if (pitchHz <= 110.0f) return 1.0f;
        if (pitchHz <= 210.0f) return 1.0f + (pitchHz - 110.0f) / 100.0f * 0.17f;
        if (pitchHz <= 290.0f) return 1.17f + (pitchHz - 210.0f) / 80.0f * 0.18f;
        return 1.35f;
    }

    // Détection de hauteur par autocorrélation normalisée sur le signal sous-échantillonné
    void AnalysePitch() {
        int minLag = static_cast<int>(m_lowRate / kMaxPitch);
        int maxLag = static_cast<int>(m_lowRate / kMinPitch);
        int window = static_cast<int>(m_lowRate * 0.032f);
        if (minLag < 2 || window + maxLag + 1 > kMaxAnalysis || m_lowPos < static_cast<uint64_t>(window + maxLag + 1)) return;

        float x[kMaxAnalysis];
        int total = window + maxLag;
        uint64_t start = m_lowPos - static_cast<uint64_t>(total);
        float energy = 0.0f;
        for (int i = 0; i < total; ++i) {
            x[i] = m_low[(start + static_cast<uint64_t>(i)) & (kLowSize - 1)];
            energy += x[i] * x[i];
        }

        m_voiced = false;
        if (energy / static_cast<float>(total) < 1e-6f) return; // Silence

        float nsdf[kMaxAnalysis];
        float best = 0.0f;
        for (int lag = minLag - 1; lag <= maxLag; ++lag) {
            float acf = 0.0f;
            float norm = 0.0f;
            for (int i = 0; i < window; ++i) {
                acf += x[i] * x[i + lag];
                norm += x[i] * x[i] + x[i + lag] * x[i + lag];
            }
            nsdf[lag] = 2.0f * acf / (norm + 1e-12f);
        }
        for (int lag = minLag; lag < maxLag; ++lag) best = std::max(best, nsdf[lag]);
        if (best < 0.55f) return; // Son non voisé (consonne, souffle, bruit)

        // Premier pic assez proche du maximum : évite de se tromper d'une octave
        for (int lag = minLag; lag < maxLag; ++lag) {
            if (nsdf[lag] >= 0.85f * best && nsdf[lag] >= nsdf[lag - 1] && nsdf[lag] >= nsdf[lag + 1]) {
                float a = nsdf[lag - 1], b = nsdf[lag], c = nsdf[lag + 1];
                float denom = a - 2.0f * b + c;
                float offset = std::fabs(denom) > 1e-9f ? 0.5f * (a - c) / denom : 0.0f;
                float period = (static_cast<float>(lag) + offset) * static_cast<float>(m_decim);

                float ratio = period / m_period;
                m_period = (ratio > 0.8f && ratio < 1.25f) ? m_period + 0.5f * (period - m_period) : period;
                m_voiced = true;
                UpdateRatios(static_cast<float>(m_sampleRate) / period);
                return;
            }
        }
    }

    // Met à jour la hauteur moyenne de la voix d'origine et les rapports de transformation
    void UpdateRatios(float pitchHz) {
        ++m_pitchCount;
        float alpha = std::max(1.0f / static_cast<float>(m_pitchCount), 0.004f);
        m_averageLogPitch += alpha * (std::log(pitchHz) - m_averageLogPitch);
        float averagePitch = std::exp(m_averageLogPitch);

        float pitchRatio = m_targetPitch.load(std::memory_order_relaxed) / averagePitch;
        float formantRatio = m_targetFormant.load(std::memory_order_relaxed) / SourceFormant(averagePitch);
        m_pitchRatio = std::clamp(pitchRatio, 0.45f, 2.6f);
        m_formantRatio = std::clamp(formantRatio, 0.7f, 1.5f);

        // Le niveau suit la densité des grains (plus fort s'ils se recouvrent, plus faible s'ils s'espacent) : on compense
        m_gain = 1.0f / std::sqrt(m_pitchRatio / m_formantRatio);
    }

    void StartGrain() {
        double now = static_cast<double>(m_writePos);
        double period = static_cast<double>(m_period);
        double desired = now - 0.5 * period - 4.0;
        double center;

        if (m_voiced) {
            // Centre calé à un nombre entier de périodes du grain précédent : les grains restent en phase
            center = m_lastCenter + std::round((desired - m_lastCenter) / period) * period;
        } else {
            // Son non voisé : position aléatoire pour ne pas créer de hauteur artificielle
            m_random = m_random * 1664525u + 1013904223u;
            center = desired - 0.5 * period * (static_cast<double>(m_random >> 8) / 16777216.0);
        }
        m_lastCenter = center;

        for (Grain& grain : m_grains) {
            if (grain.active) continue;
            grain.active = true;
            grain.position = center - period;
            grain.speed = m_formantRatio;
            grain.progress = 0.0f;
            grain.progressStep = m_formantRatio / (2.0f * m_period);
            return;
        }
    }

    std::vector<float> m_input;
    std::vector<float> m_low;
    uint64_t m_writePos = 0;
    uint64_t m_lowPos = 0;
    uint32_t m_sampleRate = 0;
    uint32_t m_decim = 4;
    uint32_t m_decimCount = 0;
    uint32_t m_hopCount = 0;
    float m_decimSum = 0.0f;
    float m_lowRate = 12000.0f;

    Grain m_grains[kMaxGrains];
    float m_nextGrain = 0.0f;
    double m_lastCenter = 0.0;
    uint32_t m_random = 12345u;

    float m_period = 320.0f;  // Période détectée (en frames)
    bool m_voiced = false;
    uint32_t m_pitchCount = 0;
    float m_averageLogPitch = 0.0f;
    float m_pitchRatio = 1.0f;
    float m_formantRatio = 1.0f;
    float m_gain = 1.0f;

    std::atomic<float> m_targetPitch;
    std::atomic<float> m_targetFormant;
};
