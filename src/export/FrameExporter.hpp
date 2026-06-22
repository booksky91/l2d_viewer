#pragma once

#include "app/AppState.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace l2dgui
{

struct AlphaBounds
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool valid = false;
};

struct FrameExporterCallbacks
{
    // Called after the exporter binds and clears its offscreen FBO.
    std::function<void(int width, int height, float absoluteTimeSeconds)> render;
    std::function<void(float progress, const std::string& message)> progress;
    // Returns the duration of the selected model's current animation in seconds.
    // Used to resolve duration == -1 ("export 1 full cycle").  May return 0
    // if no motion is loaded; the exporter will fall back to a 1-second clip.
    std::function<float()> getMotionDuration;
};

class FrameExporter
{
public:
    bool exportWebmAlpha(const ExportSettings& settings, const FrameExporterCallbacks& callbacks, ExportStatus* status);
    bool exportSingleFrame(const ExportSettings& settings, const FrameExporterCallbacks& callbacks, ExportStatus* status);
    bool exportFrameSequence(const ExportSettings& settings, const FrameExporterCallbacks& callbacks, ExportStatus* status);
    bool exportVideo(const ExportSettings& settings, const FrameExporterCallbacks& callbacks, ExportStatus* status);

private:
    bool ensureFbo(int width, int height, std::string* error);
    void destroyFbo();
    void clear(bool transparent, const float bg[4]);
    std::vector<uint8_t> readRgba(int width, int height);
    AlphaBounds computeFrameBounds(const uint8_t* rgba, int width, int height, int alphaThreshold) const;
    AlphaBounds mergeBounds(const AlphaBounds& a, const AlphaBounds& b) const;
    AlphaBounds padAndEven(const AlphaBounds& b, int padding, int maxW, int maxH) const;
    bool resolveAutoResolution(ExportSettings& settings, const FrameExporterCallbacks& callbacks);
    std::string buildFfmpegCommand(const ExportSettings& settings, const AlphaBounds& crop, bool useCrop) const;

    unsigned int _fbo = 0;
    unsigned int _tex = 0;
    unsigned int _rbo = 0;
    int _w = 0;
    int _h = 0;
};

} // namespace l2dgui
