// "PluginManager.cpp"
#include "PluginManager.h"
#include <iostream>

std::unique_ptr<IEffect> PluginManager::LoadPlugin(const std::wstring& dllPath, std::string& outName) {
    HMODULE hModule = LoadLibraryW(dllPath.c_str());
    if (!hModule) {
        return nullptr;
    }

    // Récupération des pointeurs de fonctions
    auto createFn   = reinterpret_cast<PFN_Plugin_Create>(GetProcAddress(hModule, "Plugin_Create"));
    auto destroyFn  = reinterpret_cast<PFN_Plugin_Destroy>(GetProcAddress(hModule, "Plugin_Destroy"));
    auto getNameFn  = reinterpret_cast<PFN_Plugin_GetName>(GetProcAddress(hModule, "Plugin_GetName"));
    auto processFn  = reinterpret_cast<PFN_Plugin_Process>(GetProcAddress(hModule, "Plugin_Process"));
    auto setParamFn = reinterpret_cast<PFN_Plugin_SetParameter>(GetProcAddress(hModule, "Plugin_SetParameter"));

    // Validation : la DLL doit exporter toutes les fonctions requises
    if (!createFn || !destroyFn || !getNameFn || !processFn || !setParamFn) {
        FreeLibrary(hModule);
        return nullptr;
    }

    outName = getNameFn();

    // Création du Wrapper qui gérera le cycle de vie
    return std::make_unique<PluginEffect>(hModule, createFn, destroyFn, processFn, setParamFn);
}