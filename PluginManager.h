// "PluginManager.h"
#pragma once
#include <string>
#include <memory>
#include <windows.h>
#include "PluginEffect.h"

class PluginManager {
public:
    // Tente de charger une DLL et renvoie un IEffect si valide
    static std::unique_ptr<IEffect> LoadPlugin(const std::wstring& dllPath, std::string& outName);
};