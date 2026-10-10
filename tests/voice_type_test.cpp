// "voice_type_test.cpp" : vérifie les plugins de type de voix (homme, femme, enfant).
// Une voix synthétique grave, puis une aiguë, passe dans chaque plugin ; la hauteur de sortie doit
// atteindre la hauteur visée par le plugin, quel que soit le point de départ, sans coupure ni saut de volume.
//
// Usage : voice_type_test <plugin.dll> <hauteur visee en Hz> [<plugin.dll> <hauteur> ...]
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

typedef void* (*CreateFn)();
typedef void (*DestroyFn)(void*);
typedef const char* (*NameFn)();
typedef void (*ProcessFn)(void*, float*, uint32_t, uint32_t, uint32_t);

static const uint32_t kSampleRate = 48000;
static const double kPi = 3.14159265358979323846;

// Voix synthétique : harmoniques de f0 (avec intonation) pondérées par trois formants
static std::vector<float> MakeVoice(double f0, double formantScale, double seconds) {
    std::vector<float> voice((size_t)(kSampleRate * seconds));
    const double formants[3] = {700 * formantScale, 1200 * formantScale, 2600 * formantScale};
    const double widths[3] = {130, 170, 250};
    double phase = 0.0;

    for (size_t n = 0; n < voice.size(); ++n) {
        double t = (double)n / kSampleRate;
        double f = f0 * (1.0 + 0.08 * std::sin(2 * kPi * 0.7 * t) + 0.01 * std::sin(2 * kPi * 5.5 * t));
        phase += 2 * kPi * f / kSampleRate;

        double sample = 0.0;
        for (int k = 1; k * f < 5000; ++k) {
            double amplitude = 0.0;
            for (int j = 0; j < 3; ++j) {
                double d = (k * f - formants[j]) / widths[j];
                amplitude += 1.0 / (1.0 + d * d);
            }
            sample += (amplitude + 0.02) * std::sin(k * phase) / std::sqrt((double)k);
        }
        voice[n] = (float)(0.08 * sample);
    }
    return voice;
}

// Hauteur moyenne par autocorrélation normalisée sur des trames de 40 ms
static double MeasurePitch(const std::vector<float>& x, size_t from) {
    const int window = 1920, minLag = kSampleRate / 600, maxLag = kSampleRate / 60;
    std::vector<double> nsdf(maxLag + 2);
    double sumLog = 0.0;
    int count = 0;

    for (size_t pos = from; pos + window + maxLag < x.size(); pos += 2400) {
        double best = -1e9;
        for (int lag = minLag; lag <= maxLag; ++lag) {
            double acf = 0.0, norm = 0.0;
            for (int i = 0; i < window; ++i) {
                acf += x[pos + i] * x[pos + i + lag];
                norm += x[pos + i] * x[pos + i] + x[pos + i + lag] * x[pos + i + lag];
            }
            nsdf[lag] = 2 * acf / (norm + 1e-12);
            best = std::max(best, nsdf[lag]);
        }
        if (best <= 0.5) continue;

        for (int lag = minLag + 1; lag < maxLag; ++lag) {
            if (nsdf[lag] >= 0.9 * best && nsdf[lag] >= nsdf[lag - 1] && nsdf[lag] >= nsdf[lag + 1]) {
                sumLog += std::log((double)kSampleRate / lag);
                ++count;
                break;
            }
        }
    }
    return count ? std::exp(sumLog / count) : 0.0;
}

int main(int argc, char** argv) {
    if (argc < 3 || argc % 2 == 0) {
        printf("Usage : voice_type_test <plugin.dll> <hauteur visee en Hz> [...]\n");
        return 1;
    }

    struct Source { const char* label; double f0; double formant; };
    const Source sources[2] = {{"voix grave 120 Hz", 120, 1.0}, {"voix aigue 220 Hz", 220, 1.17}};
    int failures = 0;

    for (const Source& source : sources) {
        std::vector<float> voice = MakeVoice(source.f0, source.formant, 4.0);
        size_t from = kSampleRate * 2; // Les 2 premières secondes laissent au plugin le temps de s'adapter
        printf("\nSource : %s (hauteur mesuree %.0f Hz)\n", source.label, MeasurePitch(voice, from));

        for (int a = 1; a + 1 < argc; a += 2) {
            double target = std::atof(argv[a + 1]);
            HMODULE module = LoadLibraryA(argv[a]);
            if (!module) {
                printf("  FAIL chargement %s\n", argv[a]);
                ++failures;
                continue;
            }
            auto create = (CreateFn)GetProcAddress(module, "Plugin_Create");
            auto destroy = (DestroyFn)GetProcAddress(module, "Plugin_Destroy");
            auto name = (NameFn)GetProcAddress(module, "Plugin_GetName");
            auto process = (ProcessFn)GetProcAddress(module, "Plugin_Process");

            void* plugin = create();
            std::vector<float> out(voice.size()), buffer(256 * 2);
            for (size_t pos = 0; pos + 256 <= voice.size(); pos += 256) {
                for (int i = 0; i < 256; ++i) buffer[i * 2] = buffer[i * 2 + 1] = voice[pos + i];
                process(plugin, buffer.data(), 256, 2, kSampleRate);
                for (int i = 0; i < 256; ++i) out[pos + i] = buffer[i * 2];
            }
            destroy(plugin);

            double sumIn = 0.0, sumOut = 0.0;
            bool finite = true;
            for (size_t n = from; n < out.size(); ++n) {
                sumIn += voice[n] * voice[n];
                sumOut += out[n] * out[n];
                if (!std::isfinite(out[n])) finite = false;
            }
            double level = std::sqrt(sumOut / sumIn);
            double rmsOut = std::sqrt(sumOut / (out.size() - from));

            // Coupures : niveau du bloc de 20 ms le plus faible, rapporté au niveau moyen
            double weakest = 1e9;
            for (size_t pos = from; pos + 960 <= out.size(); pos += 960) {
                double sum = 0.0;
                for (int i = 0; i < 960; ++i) sum += out[pos + i] * out[pos + i];
                weakest = std::min(weakest, std::sqrt(sum / 960) / rmsOut);
            }

            double pitch = MeasurePitch(out, from);
            bool ok = finite && std::fabs(pitch - target) <= 0.08 * target && level > 0.6 && level < 1.4 && weakest > 0.4;
            if (!ok) ++failures;
            printf("  %s %-12s hauteur=%4.0f Hz (visee %.0f)  niveau=%.2fx  bloc le plus faible=%.2fx\n",
                   ok ? "OK  " : "FAIL", name(), pitch, target, level, weakest);
        }
    }

    printf("\n%d cas en echec\n", failures);
    return failures;
}
