#pragma once

#if defined(_WIN32)
  #if defined(L2D_BACKEND_EXPORTS)
    #define L2D_API __declspec(dllexport)
  #else
    #define L2D_API __declspec(dllimport)
  #endif
#else
  #define L2D_API
#endif

#include "app/AppState.hpp"
#include "render/ViewportCamera.hpp"
#include <functional>
#include <memory>
#include <string>

namespace l2dgui
{

struct LoadResult
{
    bool ok = false;
    std::string error;
};

class ILive2DBackend
{
public:
    virtual ~ILive2DBackend() = default;

    virtual const char* name() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual LoadResult loadModel(ModelEntry& entry) = 0;
    virtual void unloadModel(uint64_t modelId) = 0;

    virtual void playMotion(uint64_t modelId, const MotionItem& motion, bool loop) = 0;
    virtual void stopMotion(uint64_t modelId) = 0;
    virtual void startPlaylist(uint64_t modelId, const std::vector<PlaylistItem>& playlist, bool loop) = 0;
    virtual void stopPlaylist(uint64_t modelId) = 0;
    virtual void setExpression(uint64_t modelId, const ExpressionItem& expression) = 0;
    virtual void setDragPoint(uint64_t modelId, float x, float y) = 0;

    virtual void update(AppState& state, float dtSeconds) = 0;
    virtual void draw(const AppState& state, int width, int height, const ViewportCamera& camera) = 0;

    // Used by FrameExporter. The OpenGL context is current on the same thread.
    // Passing the camera lets the exporter replicate the exact same viewport transform.
    virtual void prepareForExport(uint64_t modelId) = 0;
    virtual void setExporting(bool exporting) = 0;
    virtual void drawForExport(const AppState& state, int width, int height, const ViewportCamera& camera, float absoluteTimeSeconds) = 0;

    virtual void setHideRedOption(bool enable) = 0;
    virtual bool hitTest(uint64_t modelId, float x, float y) = 0;
    virtual float getMotionTime(uint64_t modelId) = 0;
    virtual float getMotionDuration(uint64_t modelId) = 0;
    virtual void setMotionTime(uint64_t modelId, float timeSeconds) = 0;
    virtual void setMotionPaused(uint64_t modelId, bool paused) = 0;
    virtual bool isMotionPaused(uint64_t modelId) = 0;
    virtual float getMotionFadeInTime(uint64_t modelId, const std::string& group, int index) = 0;
    virtual void  setMotionBlendThreshold(uint64_t modelId, float threshold) = 0;

    virtual int getPartCount(uint64_t modelId) = 0;
    virtual const char* getPartId(uint64_t modelId, int index) = 0;
    virtual float getPartOpacity(uint64_t modelId, int index) = 0;
    virtual void setPartOpacity(uint64_t modelId, int index, float opacity) = 0;

    virtual int getParameterCount(uint64_t modelId) = 0;
    virtual const char* getParameterId(uint64_t modelId, int index) = 0;
    virtual float getParameterValue(uint64_t modelId, int index) = 0;
    virtual void setParameterValue(uint64_t modelId, int index, float value) = 0;
    virtual float getParameterMin(uint64_t modelId, int index) = 0;
    virtual float getParameterMax(uint64_t modelId, int index) = 0;
    virtual float getParameterDefault(uint64_t modelId, int index) = 0;
};

L2D_API std::unique_ptr<ILive2DBackend> createBackend();

} // namespace l2dgui
