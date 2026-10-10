// "plugin_smoke_test.cpp" : charge chaque DLL de plugin passée en argument et fait passer 2 s de signal dedans.
// Vérifie que le plugin exporte toute l'API, produit un son fini, non muet et sans pic aberrant.
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

typedef void* (*CreateFn)();
typedef void (*DestroyFn)(void*);
typedef const char* (*NameFn)();
typedef void (*ProcessFn)(void*, float*, uint32_t, uint32_t, uint32_t);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage : plugin_smoke_test <plugin.dll> [...]\n");
        return 1;
    }

    int failures = 0;
    for (int a = 1; a < argc; ++a) {
        HMODULE module = LoadLibraryA(argv[a]);
        if (!module) {
            printf("FAIL chargement %s\n", argv[a]);
            ++failures;
            continue;
        }

        auto create = (CreateFn)GetProcAddress(module, "Plugin_Create");
        auto destroy = (DestroyFn)GetProcAddress(module, "Plugin_Destroy");
        auto name = (NameFn)GetProcAddress(module, "Plugin_GetName");
        auto process = (ProcessFn)GetProcAddress(module, "Plugin_Process");
        auto setParam = GetProcAddress(module, "Plugin_SetParameter");
        if (!create || !destroy || !name || !process || !setParam) {
            printf("FAIL exports %s\n", argv[a]);
            ++failures;
            continue;
        }

        void* plugin = create();
        const uint32_t sampleRate = 48000, channels = 2, block = 256;
        std::vector<float> buffer(block * channels);
        double phase = 0.0, sumIn = 0.0, sumOut = 0.0;
        float peak = 0.0f;
        bool finite = true;

        for (uint32_t n = 0; n < sampleRate * 2 / block; ++n) {
            for (uint32_t i = 0; i < block; ++i) {
                // Voix synthétique : 180 Hz et deux harmoniques, amplitude 0.3
                float x = 0.3f * (float)(std::sin(phase) * 0.6 + std::sin(phase * 3) * 0.3 + std::sin(phase * 9) * 0.1);
                phase += 6.283185307 * 180.0 / sampleRate;
                buffer[i * channels] = buffer[i * channels + 1] = x;
                sumIn += x * x;
            }
            process(plugin, buffer.data(), block, channels, sampleRate);
            for (uint32_t i = 0; i < block; ++i) {
                float left = buffer[i * channels], right = buffer[i * channels + 1];
                if (!std::isfinite(left) || !std::isfinite(right)) finite = false;
                sumOut += left * left;
                if (std::fabs(left) > peak) peak = std::fabs(left);
            }
        }
        destroy(plugin);

        double rmsIn = std::sqrt(sumIn / (sampleRate * 2)), rmsOut = std::sqrt(sumOut / (sampleRate * 2));
        bool ok = finite && rmsOut > 0.005 && peak <= 1.0f;
        if (!ok) ++failures;
        printf("%s %-26s niveau entree=%.3f sortie=%.3f pic=%.2f\n", ok ? "OK  " : "FAIL", name(), rmsIn, rmsOut, peak);
    }

    printf("%d plugin(s) en echec\n", failures);
    return failures;
}
