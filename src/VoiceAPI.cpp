// "VoiceAPI.cpp"
#include <windows.h>
#include <string>
#include <filesystem>
#include "PluginManager.h"
#include "AudioEngine.h" // Pour accéder à g_AudioEngine

// Définition de la signature du callback attendu par le C++
// id : identifiant unique généré par le moteur
// name : nom du plugin encodé en ANSI (const char*)
typedef void(__stdcall *PluginDiscoveredCallback)(int id, const char* name);

extern "C" {
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