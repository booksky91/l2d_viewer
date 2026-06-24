#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace l2dgui
{

struct MotionItem
{
    std::string group;
    int index = 0;
    std::string file;
    std::string label() const;
};

struct PlaylistItem
{
    std::string group;
    int index = 0;
    float fadeTime = 0.0f;
    bool selected = false;
};

struct ExpressionItem
{
    std::string name;
    std::string file;
};

struct ModelEntry
{
    uint64_t id = 0;
    std::filesystem::path model3Json;
    std::filesystem::path baseDir;
    std::string name;

    bool visible = true;
    bool selected = false;
    float alpha = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    float rotationDeg = 0.0f;
    float timeScale = 1.0f;
    bool loopMotion = true;
    bool autoIdle = false;
    bool flipX = false;
    bool flipY = false;

    std::vector<MotionItem> motions;
    std::vector<ExpressionItem> expressions;
    int currentMotion = -1;
    int currentExpression = -1;

    std::vector<PlaylistItem> playlist;
    bool playlistActive = false;
    float motionBlendDeltaThreshold = 0.15f;
};

struct ViewSettings
{
    int width = 1280;
    int height = 720;
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float rotationDeg = 0.0f;
    bool flipX = false;
    bool flipY = false;
    float playbackSpeed = 1.0f;
    bool showAxis = true;
    std::string bgImagePath;
    int bgImageMode = 0; // 0=None, 1=Fill, 2=Uniform, 3=UniformToFill
    bool transparentBackground = true;
    float bg[4] = {0.96f, 0.96f, 0.97f, 1.0f}; // Soft light gray bg default
    bool onlySelected = false;
    bool showGrid = true;
    bool debugBounds = false;
    bool hideRed = true;
    bool showWireframe = false;
    bool showHitAreas = false;
    int targetFps = 60;

    float leftPanelWidth = 460.0f; // Resizable left panel (icon sidebar 40px + active panel)
    float consoleHeight = 120.0f; // Height of the console log panel at the bottom
};

enum ExportType
{
    EXPORT_SINGLE_FRAME = 0,
    EXPORT_FRAME_SEQUENCE,
    EXPORT_VIDEO
};

struct ExportSettings
{
    ExportType exportType = EXPORT_VIDEO;

    // Common settings
    // Width/height default to 0 so ExportDialogs::syncViewportDimensions()
    // fills them from the active viewport on first popup open.
    int width = 0;
    int height = 0;
    bool exportSingle = false; // true: combine selected models to single file, false: export individually
    std::filesystem::path outputFolder = "exports";
    float bgColor[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // Transparent default
    int margin = 0;
    bool autoResolution = false;
    int maxResolution = 2048;

    // 1. Single Frame options
    int imageFormat = 1; // 0=Jpeg, 1=Png, 2=Webp
    int imageQuality = 100;

    // 2. Frame Sequence options
    float duration = -1.0f; // -1 = 1 motion loop
    int fps = 30;
    float exportSpeed = 1.0f;
    bool keepLastFrame = true;

    // 3. GIF / Video options
    int videoFormat = 3; // 0=Gif, 1=Webp, 2=Apng, 3=Mp4, 4=Webm, 5=Mkv, 6=Mov
    int crfParameter = 23;
    bool loopPlay = true;
    int qualityParameter = 75;
    bool losslessCompression = false;

    // GIF-specific quality options
    int gifDitherMode = 0;    // 0=None, 1=Bayer, 2=Floyd-Steinberg, 3=Sierra
    int gifMaxColors = 256;   // 64-256

    // Render scale (supersampling)
    int renderScale = 1;      // 1, 2, or 4

    // Legacy/existing settings to prevent compilation breakages
    std::filesystem::path outputPath = "output_alpha.webm";
    std::filesystem::path ffmpegPath = "ffmpeg";
    float seconds = 3.0f;
    int padding = 32;
    int crf = 30;
    bool autoCrop = true;
    bool transparent = true;
    bool selectedOnly = true;
    bool useCurrentViewport = false;
    std::string extraFfmpegArgs;
};

struct ExportStatus
{
    bool running = false;
    float progress = 0.0f;
    std::string message;
    std::string lastCommand;

    int totalModels = 0;
    int currentModelIdx = 0;
    int currentFrame = 0;
    int totalFrames = 0;
    bool cancelRequested = false;
    bool skipRequested = false;
};

class AppState
{
public:
    uint64_t nextId();
    ModelEntry* find(uint64_t id);
    const ModelEntry* find(uint64_t id) const;
    std::vector<ModelEntry*> selectedModels();
    void clearSelection();
    void selectOnly(uint64_t id);

    std::vector<ModelEntry> models;
    ViewSettings view;
    ExportSettings exportSettings;
    ExportStatus exportStatus;

    std::filesystem::path browserDir;
    std::vector<std::filesystem::path> browserModels;

private:
    uint64_t _nextId = 1;
};

} // namespace l2dgui
