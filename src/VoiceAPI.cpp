// "VoiceAPI.cpp"
#include <windows.h>
#include <string>
#include <filesystem>
#include "PluginManager.h"
#include "AudioEngine.h" // Pour accéder à g_AudioEngine
#include "EffectChain.h"
#include "PitchShiftEffect.h"
#include "RobotEffect.h"

// Définition de la signature du callback attendu par le C++
// id : identifiant unique généré par le moteur
// name : nom du plugin encodé en ANSI (const char*)
typedef void(__stdcall *PluginDiscoveredCallback)(int id, const char* name);

// isCapture : 1 = entrée (micro), 0 = sortie
// name : nom du périphérique encodé en UTF-8, à repasser tel quel à VC_Initialize
typedef void(__stdcall *DeviceDiscoveredCallback)(int isCapture, const char* name);

extern "C" {
    // Liste les périphériques audio disponibles
    __declspec(dllexport) void VC_EnumerateDevices(DeviceDiscoveredCallback callback) {
        if (!callback) return;

        ma_context context;
        if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) return;

        ma_device_info* playbackInfos = NULL;
        ma_device_info* captureInfos = NULL;
        ma_uint32 playbackCount = 0;
        ma_uint32 captureCount = 0;
        if (ma_context_get_devices(&context, &playbackInfos, &playbackCount, &captureInfos, &captureCount) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < captureCount; ++i) callback(1, captureInfos[i].name);
            for (ma_uint32 i = 0; i < playbackCount; ++i) callback(0, playbackInfos[i].name);
        }

        ma_context_uninit(&context);
    }

    // Crée le moteur et la chaîne d'effets standard (0 = Pitch, 1 = Robot)
    // Un identifiant NULL sélectionne le périphérique par défaut
    __declspec(dllexport) bool VC_Initialize(const char* inputDeviceId, const char* outputDeviceId) {
        if (g_AudioEngine) return true;

        auto engine = std::make_unique<AudioEngine>();
        engine->GetEffectChain()->AddEffect(std::make_unique<PitchShiftEffect>());
        engine->GetEffectChain()->AddEffect(std::make_unique<RobotEffect>());

        if (!engine->Initialize(inputDeviceId ? inputDeviceId : "", outputDeviceId ? outputDeviceId : "", AudioEngineConfig{})) {
            return false;
        }

        g_AudioEngine = std::move(engine);
        return true;
    }

    __declspec(dllexport) bool VC_Start() {
        return g_AudioEngine && g_AudioEngine->Start();
    }

    __declspec(dllexport) void VC_Stop() {
        g_AudioEngine.reset();
    }

    __declspec(dllexport) void VC_SetEffectParameter(int effectIndex, int paramId, float value) {
        if (!g_AudioEngine || effectIndex < 0) return;

        if (IEffect* effect = g_AudioEngine->GetEffectChain()->GetEffect(static_cast<size_t>(effectIndex))) {
            effect->SetParameter(paramId, value);
        }
    }

    __declspec(dllexport) void VC_SetEffectEnabled(int effectIndex, bool enabled) {
        if (!g_AudioEngine || effectIndex < 0) return;

        g_AudioEngine->GetEffectChain()->SetEffectEnabled(static_cast<size_t>(effectIndex), enabled);
    }

    // --- Conversion de voix externe (clonage IA réalisé côté C#) ---

    __declspec(dllexport) void VC_SetVoiceConversionEnabled(bool enabled) {
        if (g_AudioEngine) g_AudioEngine->SetVoiceConversionEnabled(enabled);
    }

    // Lit le micro (mono, à la fréquence du moteur) ; renvoie le nombre d'échantillons lus
    __declspec(dllexport) int VC_ReadVoiceConversionInput(float* samples, int count) {
        if (!g_AudioEngine || !samples || count <= 0) return 0;
        return static_cast<int>(g_AudioEngine->ReadConversionInput(samples, static_cast<size_t>(count)));
    }

    // Renvoie la voix convertie (mono, à la fréquence du moteur) ; renvoie le nombre d'échantillons acceptés
    __declspec(dllexport) int VC_WriteVoiceConversionOutput(const float* samples, int count) {
        if (!g_AudioEngine || !samples || count <= 0) return 0;
        return static_cast<int>(g_AudioEngine->WriteConversionOutput(samples, static_cast<size_t>(count)));
    }

    __declspec(dllexport) int VC_GetSampleRate() {
        return g_AudioEngine ? static_cast<int>(g_AudioEngine->GetSampleRate()) : 0;
    }

    // Fonction exportée appelée par le C#
    __declspec(dllexport) void VC_ScanPlugins(const char* directoryPath, PluginDiscoveredCallback callback) {
        if (!directoryPath || !callback || !g_AudioEngine) return;

        std::filesystem::path path(directoryPath);
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) return;

        int pluginIdCounter = 100; // Les ID standards (Pitch, Robot) pourraient être 0-99

        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.path().extension() == ".dll") {
                std::string pluginName;
                
                // On tente de charger la DLL pour voir si c'est un plugin valide
                auto plugin = PluginManager::LoadPlugin(entry.path().wstring(), pluginName);
                
                if (plugin) {
                    // C'est un plugin valide ! On l'ajoute au moteur audio
                    g_AudioEngine->GetEffectChain()->AddEffect(std::move(plugin));

                    // On déclenche le callback vers le C# avec l'ID et le nom
                    callback(pluginIdCounter, pluginName.c_str());
                    
                    pluginIdCounter++;
                }
            }
        }
    }
}