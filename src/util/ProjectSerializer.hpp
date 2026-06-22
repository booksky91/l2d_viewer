#pragma once

#include "app/AppState.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace l2dgui
{
class ILive2DBackend;

class ProjectSerializer
{
public:
    static bool save(const AppState& state, const std::filesystem::path& filePath);
    static bool load(AppState& state, ILive2DBackend& backend, const std::filesystem::path& filePath, std::string* error = nullptr);

    static bool savePreset(const ExportSettings& settings, const std::string& presetName);
    static bool loadPreset(ExportSettings& settings, const std::string& presetName);
    static std::vector<std::string> listPresets();
};

} // namespace l2dgui
