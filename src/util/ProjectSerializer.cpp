#include "util/ProjectSerializer.hpp"
#include "backends/ILive2DBackend.hpp"
#include "util/PathUtil.hpp"
#include "render/ModelScanner.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace l2dgui
{

static json serializeViewSettings(const ViewSettings& v)
{
    json j;
    j["width"] = v.width;
    j["height"] = v.height;
    j["zoom"] = v.zoom;
    j["panX"] = v.panX;
    j["panY"] = v.panY;
    j["centerX"] = v.centerX;
    j["centerY"] = v.centerY;
    j["rotationDeg"] = v.rotationDeg;
    j["flipX"] = v.flipX;
    j["flipY"] = v.flipY;
    j["playbackSpeed"] = v.playbackSpeed;
    j["showAxis"] = v.showAxis;
    j["bgImagePath"] = v.bgImagePath;
    j["bgImageMode"] = v.bgImageMode;
    j["transparentBackground"] = v.transparentBackground;
    j["bg"] = { v.bg[0], v.bg[1], v.bg[2], v.bg[3] };
    j["onlySelected"] = v.onlySelected;
    j["showGrid"] = v.showGrid;
    j["debugBounds"] = v.debugBounds;
    j["hideRed"] = v.hideRed;
    j["showWireframe"] = v.showWireframe;
    j["showHitAreas"] = v.showHitAreas;
    j["targetFps"] = v.targetFps;
    j["leftPanelWidth"] = v.leftPanelWidth;
    j["consoleHeight"] = v.consoleHeight;
    return j;
}

static void deserializeViewSettings(const json& j, ViewSettings& v)
{
    auto get = [&](const char* key, auto& val) {
        if (j.contains(key)) j[key].get_to(val);
    };
    get("width", v.width);
    get("height", v.height);
    get("zoom", v.zoom);
    get("panX", v.panX);
    get("panY", v.panY);
    get("centerX", v.centerX);
    get("centerY", v.centerY);
    get("rotationDeg", v.rotationDeg);
    get("flipX", v.flipX);
    get("flipY", v.flipY);
    get("playbackSpeed", v.playbackSpeed);
    get("showAxis", v.showAxis);
    get("bgImagePath", v.bgImagePath);
    get("bgImageMode", v.bgImageMode);
    get("transparentBackground", v.transparentBackground);
    if (j.contains("bg") && j["bg"].is_array() && j["bg"].size() == 4)
    {
        for (int i = 0; i < 4; ++i) v.bg[i] = j["bg"][i].get<float>();
    }
    get("onlySelected", v.onlySelected);
    get("showGrid", v.showGrid);
    get("debugBounds", v.debugBounds);
    get("hideRed", v.hideRed);
    get("showWireframe", v.showWireframe);
    get("showHitAreas", v.showHitAreas);
    get("targetFps", v.targetFps);
    get("leftPanelWidth", v.leftPanelWidth);
    get("consoleHeight", v.consoleHeight);
}

static json serializeExportSettings(const ExportSettings& e)
{
    json j;
    j["exportType"] = static_cast<int>(e.exportType);
    j["width"] = e.width;
    j["height"] = e.height;
    j["exportSingle"] = e.exportSingle;
    j["outputFolder"] = utf8Path(e.outputFolder);
    j["bgColor"] = { e.bgColor[0], e.bgColor[1], e.bgColor[2], e.bgColor[3] };
    j["margin"] = e.margin;
    j["autoResolution"] = e.autoResolution;
    j["maxResolution"] = e.maxResolution;
    j["imageFormat"] = e.imageFormat;
    j["imageQuality"] = e.imageQuality;
    j["duration"] = e.duration;
    j["fps"] = e.fps;
    j["exportSpeed"] = e.exportSpeed;
    j["keepLastFrame"] = e.keepLastFrame;
    j["videoFormat"] = e.videoFormat;
    j["crfParameter"] = e.crfParameter;
    j["loopPlay"] = e.loopPlay;
    j["qualityParameter"] = e.qualityParameter;
    j["losslessCompression"] = e.losslessCompression;
    j["gifDitherMode"] = e.gifDitherMode;
    j["gifMaxColors"] = e.gifMaxColors;
    j["renderScale"] = e.renderScale;
    j["ffmpegPath"] = utf8Path(e.ffmpegPath);
    j["extraFfmpegArgs"] = e.extraFfmpegArgs;
    return j;
}

static void deserializeExportSettings(const json& j, ExportSettings& e)
{
    auto get = [&](const char* key, auto& val) {
        if (j.contains(key)) j[key].get_to(val);
    };
    if (j.contains("exportType")) e.exportType = static_cast<ExportType>(j["exportType"].get<int>());
    get("width", e.width);
    get("height", e.height);
    get("exportSingle", e.exportSingle);
    if (j.contains("outputFolder")) e.outputFolder = pathFromUtf8(j["outputFolder"].get<std::string>());
    if (j.contains("bgColor") && j["bgColor"].is_array() && j["bgColor"].size() == 4)
    {
        for (int i = 0; i < 4; ++i) e.bgColor[i] = j["bgColor"][i].get<float>();
    }
    get("margin", e.margin);
    get("autoResolution", e.autoResolution);
    get("maxResolution", e.maxResolution);
    get("imageFormat", e.imageFormat);
    get("imageQuality", e.imageQuality);
    get("duration", e.duration);
    get("fps", e.fps);
    get("exportSpeed", e.exportSpeed);
    get("keepLastFrame", e.keepLastFrame);
    get("videoFormat", e.videoFormat);
    get("crfParameter", e.crfParameter);
    get("loopPlay", e.loopPlay);
    get("qualityParameter", e.qualityParameter);
    get("losslessCompression", e.losslessCompression);
    get("gifDitherMode", e.gifDitherMode);
    get("gifMaxColors", e.gifMaxColors);
    get("renderScale", e.renderScale);
    if (j.contains("ffmpegPath")) e.ffmpegPath = pathFromUtf8(j["ffmpegPath"].get<std::string>());
    if (j.contains("extraFfmpegArgs")) j["extraFfmpegArgs"].get_to(e.extraFfmpegArgs);
}

static json serializePlaylistItem(const PlaylistItem& p)
{
    return json{{"group", p.group}, {"index", p.index}, {"fadeTime", p.fadeTime}};
}

static json serializeModel(const ModelEntry& m)
{
    json j;
    j["model3Json"] = utf8Path(m.model3Json);
    j["name"] = m.name;
    j["visible"] = m.visible;
    j["alpha"] = m.alpha;
    j["x"] = m.x;
    j["y"] = m.y;
    j["scale"] = m.scale;
    j["rotationDeg"] = m.rotationDeg;
    j["timeScale"] = m.timeScale;
    j["loopMotion"] = m.loopMotion;
    j["autoIdle"] = m.autoIdle;
    j["flipX"] = m.flipX;
    j["flipY"] = m.flipY;
    j["currentMotion"] = m.currentMotion;
    j["currentExpression"] = m.currentExpression;
    j["playlistActive"] = m.playlistActive;
    j["steppedInterpolationMode"] = m.steppedInterpolationMode;

    json playlist = json::array();
    for (const auto& p : m.playlist)
        playlist.push_back(serializePlaylistItem(p));
    j["playlist"] = playlist;
    return j;
}

bool ProjectSerializer::save(const AppState& state, const std::filesystem::path& filePath)
{
    json root;
    root["version"] = "1.0";
    root["viewport"] = serializeViewSettings(state.view);
    root["export"] = serializeExportSettings(state.exportSettings);
    root["browserDir"] = utf8Path(state.browserDir);

    json models = json::array();
    for (const auto& m : state.models)
        models.push_back(serializeModel(m));
    root["models"] = models;

    try
    {
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);
        std::ofstream ofs(filePath);
        if (!ofs.is_open()) return false;
        ofs << root.dump(2);
        return ofs.good();
    }
    catch (...)
    {
        return false;
    }
}

bool ProjectSerializer::load(AppState& state, ILive2DBackend& backend, const std::filesystem::path& filePath, std::string* error)
{
    try
    {
        std::ifstream ifs(filePath);
        if (!ifs.is_open())
        {
            if (error) *error = "Cannot open file: " + utf8Path(filePath);
            return false;
        }
        json root = json::parse(ifs);

        // Unload all existing models
        for (const auto& m : state.models)
            backend.unloadModel(m.id);
        state.models.clear();

        // Viewport settings
        if (root.contains("viewport"))
            deserializeViewSettings(root["viewport"], state.view);

        // Export settings
        if (root.contains("export"))
            deserializeExportSettings(root["export"], state.exportSettings);

        // Browser dir
        if (root.contains("browserDir"))
        {
            state.browserDir = pathFromUtf8(root["browserDir"].get<std::string>());
        }

        // Load models
        if (root.contains("models") && root["models"].is_array())
        {
            for (const auto& mj : root["models"])
            {
                std::string model3JsonStr = mj.value("model3Json", "");
                if (model3JsonStr.empty()) continue;

                std::filesystem::path model3Json = pathFromUtf8(model3JsonStr);
                if (!std::filesystem::exists(model3Json))
                {
                    std::cerr << "[ProjectSerializer] Model file not found: " << model3JsonStr << std::endl;
                    continue;
                }

                ModelEntry entry;
                entry.id = state.nextId();
                entry.model3Json = model3Json;
                entry.baseDir = model3Json.parent_path();
                entry.name = mj.value("name", model3Json.stem().u8string());

                auto get = [&](const char* key, auto& val) {
                    if (mj.contains(key)) mj[key].get_to(val);
                };
                get("visible", entry.visible);
                get("alpha", entry.alpha);
                get("x", entry.x);
                get("y", entry.y);
                get("scale", entry.scale);
                get("rotationDeg", entry.rotationDeg);
                get("timeScale", entry.timeScale);
                get("loopMotion", entry.loopMotion);
                get("autoIdle", entry.autoIdle);
                get("flipX", entry.flipX);
                get("flipY", entry.flipY);
                get("currentMotion", entry.currentMotion);
                get("currentExpression", entry.currentExpression);
                get("steppedInterpolationMode", entry.steppedInterpolationMode);
                get("playlistActive", entry.playlistActive);

                if (mj.contains("playlist") && mj["playlist"].is_array())
                {
                    for (const auto& pj : mj["playlist"])
                    {
                        PlaylistItem item;
                        item.group = pj.value("group", "");
                        item.index = pj.value("index", 0);
                        item.fadeTime = pj.value("fadeTime", 0.0f);
                        entry.playlist.push_back(item);
                    }
                }

                // Load model metadata first to populate motions/expressions etc.
                ModelScanner::fillMetadataFromModel3Json(entry);

                // Load model in backend
                auto loadRes = backend.loadModel(entry);
                if (loadRes.ok)
                {
                    if (entry.playlistActive)
                    {
                        backend.startPlaylist(entry.id, entry.playlist, entry.loopMotion);
                    }
                    else if (entry.currentMotion >= 0 && entry.currentMotion < (int)entry.motions.size())
                    {
                        backend.playMotion(entry.id, entry.motions[entry.currentMotion], entry.loopMotion);
                    }
                    if (entry.currentExpression >= 0 && entry.currentExpression < (int)entry.expressions.size())
                    {
                        backend.setExpression(entry.id, entry.expressions[entry.currentExpression]);
                    }
                    state.models.push_back(std::move(entry));
                }
                else
                {
                    std::cerr << "[ProjectSerializer] Failed to load backend model: " << loadRes.error << std::endl;
                }
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        if (error) *error = e.what();
        return false;
    }
}

bool ProjectSerializer::savePreset(const ExportSettings& settings, const std::string& presetName)
{
    try
    {
        std::filesystem::path dir = "export_presets";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        std::filesystem::path filePath = dir / (presetName + ".json");
        std::ofstream ofs(filePath);
        if (!ofs.is_open()) return false;

        json j = serializeExportSettings(settings);
        ofs << j.dump(2);
        return ofs.good();
    }
    catch (...)
    {
        return false;
    }
}

bool ProjectSerializer::loadPreset(ExportSettings& settings, const std::string& presetName)
{
    try
    {
        std::filesystem::path filePath = std::filesystem::path("export_presets") / (presetName + ".json");
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) return false;

        json j = json::parse(ifs);
        deserializeExportSettings(j, settings);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::vector<std::string> ProjectSerializer::listPresets()
{
    std::vector<std::string> presets;
    try
    {
        std::filesystem::path dir = "export_presets";
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) return presets;

        for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                presets.push_back(entry.path().stem().string());
            }
        }
    }
    catch (...)
    {
    }
    return presets;
}

} // namespace l2dgui
