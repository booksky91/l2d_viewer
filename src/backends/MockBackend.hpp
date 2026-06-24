#pragma once

#include "backends/ILive2DBackend.hpp"
#include <unordered_map>

namespace l2dgui
{

class MockBackend final : public ILive2DBackend
{
public:
    const char* name() const override { return "Mock OpenGL Backend"; }
    bool initialize() override;
    void shutdown() override;
    LoadResult loadModel(ModelEntry& entry) override;
    void unloadModel(uint64_t modelId) override;
    void playMotion(uint64_t modelId, const MotionItem& motion, bool loop) override;
    void stopMotion(uint64_t modelId) override;
    void startPlaylist(uint64_t modelId, const std::vector<PlaylistItem>& playlist, bool loop) override {}
    void stopPlaylist(uint64_t modelId) override {}
    void setExpression(uint64_t modelId, const ExpressionItem& expression) override;
    void setDragPoint(uint64_t modelId, float x, float y) override {}
    void update(AppState& state, float dtSeconds) override;
    void draw(const AppState& state, int width, int height, const ViewportCamera& camera) override;
    void prepareForExport(uint64_t modelId) override {}
    void drawForExport(const AppState& state, int width, int height, const ViewportCamera& camera, float absoluteTimeSeconds) override;
    void setExporting(bool exporting) override {}
    void setHideRedOption(bool enable) override {}
    bool hitTest(uint64_t modelId, float x, float y) override { return false; }
    float getMotionTime(uint64_t modelId) override { return 0.0f; }
    float getMotionDuration(uint64_t modelId) override { return 0.0f; }
    void setMotionTime(uint64_t modelId, float timeSeconds) override {}
    void setMotionPaused(uint64_t modelId, bool paused) override {}
    bool isMotionPaused(uint64_t modelId) override { return false; }
    float getMotionFadeInTime(uint64_t modelId, const std::string& group, int index) override { return -1.0f; }
    void  setMotionBlendThreshold(uint64_t modelId, float threshold) override {}

    int getPartCount(uint64_t) override { return 0; }
    const char* getPartId(uint64_t, int) override { return ""; }
    float getPartOpacity(uint64_t, int) override { return 1.0f; }
    void setPartOpacity(uint64_t, int, float) override {}

    int getParameterCount(uint64_t) override { return 0; }
    const char* getParameterId(uint64_t, int) override { return ""; }
    float getParameterValue(uint64_t, int) override { return 0.0f; }
    void setParameterValue(uint64_t, int, float) override {}
    float getParameterMin(uint64_t, int) override { return -1.0f; }
    float getParameterMax(uint64_t, int) override { return 1.0f; }
    float getParameterDefault(uint64_t, int) override { return 0.0f; }

private:
    struct Runtime
    {
        float time = 0.0f;
        int motionIndex = -1;
        bool loop = true;
    };
    std::unordered_map<uint64_t, Runtime> _runtimes;

    void drawScene(const AppState& state, int width, int height, const ViewportCamera& camera, bool exportMode, float t);
    void drawOne(const ModelEntry& m, const Runtime& rt, float t, bool showHitAreas);
};

} // namespace l2dgui
