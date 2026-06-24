#pragma once

#include "backends/ILive2DBackend.hpp"
#include <memory>
#include <unordered_map>

namespace l2dgui
{

struct BridgePlaylistItem
{
    std::string group;
    int index;
    float fadeTime;
};

// Thin bridge boundary. The GUI/exporter never includes Live2D headers directly.
// Implement INativeCubismBridge by wrapping CubismNativeSamples' LAppModel/LAppPal/etc.
class INativeCubismBridge
{
public:
    virtual ~INativeCubismBridge() = default;
    virtual bool initialize(std::string* error) = 0;
    virtual void shutdown() = 0;
    virtual bool load(uint64_t id, const std::string& modelDirUtf8, const std::string& model3FileUtf8, std::string* error) = 0;
    virtual void unload(uint64_t id) = 0;
    virtual void startMotion(uint64_t id, const std::string& group, int index, bool loop) = 0;
    virtual void stopMotion(uint64_t id) = 0;
    virtual void startPlaylist(uint64_t id, const std::vector<BridgePlaylistItem>& items, bool loop) = 0;
    virtual void stopPlaylist(uint64_t id) = 0;
    virtual void setExpression(uint64_t id, const std::string& name) = 0;
    virtual void setDragPoint(uint64_t id, float x, float y) = 0;
    virtual void setTransform(uint64_t id, float x, float y, float scale, float rotationDeg, float alpha, bool flipX, bool flipY, bool loopMotion) = 0;
    virtual void update(uint64_t id, float dtSeconds, bool autoIdle) = 0;
    virtual void prepareForExport(uint64_t id) = 0;
    virtual void draw(uint64_t id, int width, int height, float viewPanX, float viewPanY, float viewZoom, float viewRotDeg, bool viewFlipX, bool viewFlipY, bool showWireframe, bool showHitAreas) = 0;
    virtual void drawAtTime(uint64_t id, int width, int height, float absoluteTimeSeconds, float viewPanX, float viewPanY, float viewZoom, float viewRotDeg, bool viewFlipX, bool viewFlipY) = 0;
    virtual bool hitTest(uint64_t id, float x, float y) = 0;
    virtual float getMotionTime(uint64_t id) = 0;
    virtual float getMotionDuration(uint64_t id) = 0;
    virtual void setMotionTime(uint64_t id, float timeSeconds) = 0;
    virtual void setMotionPaused(uint64_t id, bool paused) = 0;
    virtual bool isMotionPaused(uint64_t id) = 0;
    virtual float getMotionFadeInTime(uint64_t id, const std::string& group, int index) = 0;
    virtual void  setMotionBlendThreshold(uint64_t id, float threshold) = 0;

    virtual int getPartCount(uint64_t id) = 0;
    virtual const char* getPartId(uint64_t id, int index) = 0;
    virtual float getPartOpacity(uint64_t id, int index) = 0;
    virtual void setPartOpacity(uint64_t id, int index, float opacity) = 0;

    virtual int getParameterCount(uint64_t id) = 0;
    virtual const char* getParameterId(uint64_t id, int index) = 0;
    virtual float getParameterValue(uint64_t id, int index) = 0;
    virtual void setParameterValue(uint64_t id, int index, float value) = 0;
    virtual float getParameterMin(uint64_t id, int index) = 0;
    virtual float getParameterMax(uint64_t id, int index) = 0;
    virtual float getParameterDefault(uint64_t id, int index) = 0;
};

std::unique_ptr<INativeCubismBridge> createNativeCubismBridge();

class CubismNativeBackend final : public ILive2DBackend
{
public:
    CubismNativeBackend();
    ~CubismNativeBackend() override;

    const char* name() const override { return "Live2D Cubism Native Backend"; }
    bool initialize() override;
    void shutdown() override;
    LoadResult loadModel(ModelEntry& entry) override;
    void unloadModel(uint64_t modelId) override;
    void playMotion(uint64_t modelId, const MotionItem& motion, bool loop) override;
    void stopMotion(uint64_t modelId) override;
    void startPlaylist(uint64_t modelId, const std::vector<PlaylistItem>& playlist, bool loop) override;
    void stopPlaylist(uint64_t modelId) override;
    void setExpression(uint64_t modelId, const ExpressionItem& expression) override;
    void setDragPoint(uint64_t modelId, float x, float y) override;
    void update(AppState& state, float dtSeconds) override;
    void draw(const AppState& state, int width, int height, const ViewportCamera& camera) override;
    void prepareForExport(uint64_t modelId) override;
    void drawForExport(const AppState& state, int width, int height, const ViewportCamera& camera, float absoluteTimeSeconds) override;
    void setHideRedOption(bool enable) override;
    bool hitTest(uint64_t modelId, float x, float y) override;
    float getMotionTime(uint64_t modelId) override;
    float getMotionDuration(uint64_t modelId) override;
    void setMotionTime(uint64_t modelId, float timeSeconds) override;
    void setMotionPaused(uint64_t modelId, bool paused) override;
    bool isMotionPaused(uint64_t modelId) override;
    float getMotionFadeInTime(uint64_t modelId, const std::string& group, int index) override;
    void  setMotionBlendThreshold(uint64_t modelId, float threshold) override;

    int getPartCount(uint64_t modelId) override;
    const char* getPartId(uint64_t modelId, int index) override;
    float getPartOpacity(uint64_t modelId, int index) override;
    void setPartOpacity(uint64_t modelId, int index, float opacity) override;

    int getParameterCount(uint64_t modelId) override;
    const char* getParameterId(uint64_t modelId, int index) override;
    float getParameterValue(uint64_t modelId, int index) override;
    void setParameterValue(uint64_t modelId, int index, float value) override;
    float getParameterMin(uint64_t modelId, int index) override;
    float getParameterMax(uint64_t modelId, int index) override;
    float getParameterDefault(uint64_t modelId, int index) override;

private:
    std::unique_ptr<INativeCubismBridge> _bridge;
};

} // namespace l2dgui
