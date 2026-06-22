#include "backends/CubismNativeBackend.hpp"
#include "util/CrashDiagnostics.hpp"

#if defined(L2D_ENABLE_CUBISM_NATIVE)

#include <GL/glew.h>

extern "C" bool g_hideRedOption = false;

#include "LAppAllocator_Common.hpp"
#include "LAppDefine.hpp"
#include "LAppDelegate.hpp"
#define private protected
#include "LAppModel.hpp"
#undef private
#include "LAppPal.hpp"
#include "Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp"

#include "CubismFramework.hpp"
#include "Id/CubismIdManager.hpp"
#include "Effect/CubismBreath.hpp"
#include "Math/CubismMatrix44.hpp"
#include "Math/CubismModelMatrix.hpp"
#include "Motion/CubismMotion.hpp"
#include "Motion/CubismMotionQueueEntry.hpp"
#include "Motion/CubismMotionQueueManager.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <iostream>
#include <algorithm>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

extern "C" {
void LAppPal_SetOverrideTime(double currentTime, double deltaTime);
void LAppPal_ClearOverrideTime();
}

namespace l2dgui
{
namespace
{

class DummyMotion : public Live2D::Cubism::Framework::ACubismMotion
{
public:
    DummyMotion()
    {
        _isLoop = true;
    }
    
    Live2D::Cubism::Framework::csmFloat32 GetDuration() override
    {
        return -1.0f;
    }
    
    Live2D::Cubism::Framework::csmFloat32 GetLoopDuration() override
    {
        return -1.0f;
    }

protected:
    void DoUpdateParameters(
        Live2D::Cubism::Framework::CubismModel* model,
        Live2D::Cubism::Framework::csmFloat32 userTimeSeconds,
        Live2D::Cubism::Framework::csmFloat32 weight,
        Live2D::Cubism::Framework::CubismMotionQueueEntry* motionQueueEntry) override
    {
    }
};

class LAppModelOverride : public LAppModel
{
public:
    LAppModelOverride()
    {
        _dummyMotion = new DummyMotion();
        _autoIdleEnabled = false;
    }

    ~LAppModelOverride() override
    {
        Live2D::Cubism::Framework::ACubismMotion::Delete(_dummyMotion);
    }

    Live2D::Cubism::Framework::ACubismMotion* GetDummyMotion() const
    {
        return _dummyMotion;
    }

    Live2D::Cubism::Framework::ACubismMotion* GetMotion(const std::string& group, int index)
    {
        char keyBuf[256];
        snprintf(keyBuf, sizeof(keyBuf), "%s_%d", group.c_str(), index);
        Live2D::Cubism::Framework::csmString csmKey(keyBuf);
        if (_motions.IsExist(csmKey))
        {
            return _motions[csmKey];
        }
        return nullptr;
    }

    void SetAutoIdleEnabled(bool enable)
    {
        _autoIdleEnabled = enable;
    }

    void Update()
    {
        if (!_autoIdleEnabled && _motionManager->IsFinished())
        {
            _motionManager->SetReservePriority(LAppDefine::PriorityNone);
            _motionManager->StartMotionPriority(_dummyMotion, false, LAppDefine::PriorityNone);
        }
        LAppModel::Update();
    }

    Live2D::Cubism::Framework::CubismMotionManager* GetMotionManager() const
    {
        return _motionManager;
    }

    void DisableHeadBreathing()
    {
        if (_breath)
        {
            using namespace Live2D::Cubism::Framework;
            csmVector<CubismBreath::BreathParameterData> breathParameters;
            breathParameters.PushBack(CubismBreath::BreathParameterData(
                CubismFramework::GetIdManager()->GetId("ParamBreath"),
                0.5f, 0.5f, 3.2345f, 0.5f
            ));
            _breath->SetParameters(breathParameters);
        }
    }

    Live2D::Cubism::Framework::ACubismMotion* LoadMotion(
        const Live2D::Cubism::Framework::csmByte* buffer,
        Live2D::Cubism::Framework::csmSizeInt size,
        const Live2D::Cubism::Framework::csmChar* name,
        Live2D::Cubism::Framework::ACubismMotion::FinishedMotionCallback onFinishedMotionHandler = NULL,
        Live2D::Cubism::Framework::ACubismMotion::BeganMotionCallback onBeganMotionHandler = NULL,
        Live2D::Cubism::Framework::ICubismModelSetting* modelSetting = NULL,
        const Live2D::Cubism::Framework::csmChar* group = NULL,
        const Live2D::Cubism::Framework::csmInt32 index = -1,
        Live2D::Cubism::Framework::csmBool shouldCheckMotionConsistency = false) override
    {
        auto* motion = LAppModel::LoadMotion(buffer, size, name, onFinishedMotionHandler, onBeganMotionHandler, modelSetting, group, index, shouldCheckMotionConsistency);
        if (motion && group)
        {
            std::string groupStr(group);
            if (groupStr == "Idle" || groupStr == "idle")
            {
                motion->SetLoop(true);
            }
        }
        return motion;
    }

private:
    Live2D::Cubism::Framework::ACubismMotion* _dummyMotion;
    bool _autoIdleEnabled;
};

struct NativeModelRuntime
{
    std::unique_ptr<LAppModelOverride> model;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    float rotationDeg = 0.0f;
    float alpha = 1.0f;
    bool flipX = false;
    bool flipY = false;
    bool nativeCrashed = false;
    std::string lastNativeError;
    double cumulativeTime = 0.0;
    double lastExportTime = 0.0;
    bool isFirstExportFrame = true;
    std::vector<float> defaultPartOpacities;
    bool motionPaused = false;
    bool loopMotion = true;

    bool hasLastCustomMotion = false;
    std::string lastCustomMotionGroup;
    int lastCustomMotionIndex = -1;
    float lastCustomMotionDuration = 0.0f;
    float lastCustomMotionTime = 0.0f;
    bool lastCustomMotionLoop = false;

    struct RuntimePlaylistItem
    {
        std::string group;
        int index = 0;
        float fadeTime = 1.0f;
        float duration = 0.0f;
    };
    std::vector<RuntimePlaylistItem> playlistItems;
    bool isPlaylistActive = false;
    int playlistCurrentIndex = -1;
    bool playlistLoop = false;
    double playlistTotalDuration = 0.0;
    std::vector<double> playlistSlotStartTimes;
};

static Live2D::Cubism::Framework::CubismFramework::Option makeCubismOption()
{
    Live2D::Cubism::Framework::CubismFramework::Option option;
    option.LogFunction = LAppPal::PrintMessage;
    option.LoggingLevel = LAppDefine::CubismLoggingLevel;
    option.LoadFileFunction = LAppPal::LoadFileAsBytes;
    option.ReleaseBytesFunction = LAppPal::ReleaseBytes;
    return option;
}

#if defined(_WIN32)
static unsigned long g_lastSehCode = 0;
static const void* g_lastSehAddress = nullptr;

static int recordSehException(EXCEPTION_POINTERS* info)
{
    g_lastSehCode = info && info->ExceptionRecord ? static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode) : 0;
    g_lastSehAddress = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
    return EXCEPTION_EXECUTE_HANDLER;
}

static std::string lastSehMessage(const char* operation)
{
    return formatNativeException(g_lastSehCode, g_lastSehAddress, operation);
}

static bool safeLoadAssets(LAppModel* model, const char* dir, const char* file)
{
    __try
    {
        model->LoadAssets(dir, file);
        return true;
    }
    __except(recordSehException(GetExceptionInformation()))
    {
        return false;
    }
}

static bool safeUpdateModel(LAppModel* model)
{
    __try
    {
        model->Update();
        return true;
    }
    __except(recordSehException(GetExceptionInformation()))
    {
        return false;
    }
}

static bool safeDrawModel(LAppModel* model, Live2D::Cubism::Framework::CubismMatrix44* matrix)
{
    __try
    {
        model->Draw(*matrix);
        return true;
    }
    __except(recordSehException(GetExceptionInformation()))
    {
        return false;
    }
}

static bool safeStartMotion(LAppModel* model, const char* group, int index, int priority)
{
    __try
    {
        model->StartMotion(group, index, priority);
        return true;
    }
    __except(recordSehException(GetExceptionInformation()))
    {
        return false;
    }
}
#endif
}

class CubismNativeSamplesBridge final : public INativeCubismBridge
{
public:
    bool initialize(std::string* error) override
    {
        using namespace Live2D::Cubism::Framework;
        setCrashStage("native initialize: CubismFramework startup");
        logDiagnostic("[NATIVE] initialize begin");

        if (!_cubismStarted)
        {
            _option = makeCubismOption();
            CubismFramework::StartUp(&_allocator, &_option);
            CubismFramework::Initialize();
            _cubismStarted = true;
        }

        if (!LAppDelegate::GetInstance()->Initialize())
        {
            if (error) *error = "Failed to initialize LAppDelegate shim texture manager.";
            return false;
        }
        LAppPal::UpdateTime();
        logDiagnostic("[NATIVE] initialize complete");
        return true;
    }

    void shutdown() override
    {
        _models.clear();
        LAppDelegate::ReleaseInstance();

        if (_cubismStarted)
        {
            Live2D::Cubism::Framework::CubismFramework::Dispose();
            Live2D::Cubism::Framework::CubismFramework::CleanUp();
            _cubismStarted = false;
        }
    }

    bool load(uint64_t id, const std::string& modelDirUtf8, const std::string& model3FileUtf8, std::string* error) override
    {
        try
        {
            LAppDelegate::GetInstance()->SetExternalContextSize(2048, 2048);
            auto rt = std::make_unique<NativeModelRuntime>();
            rt->model = std::make_unique<LAppModelOverride>();

            logDiagnostic("[NATIVE] load begin id=" + std::to_string(id) + " dir=" + modelDirUtf8 + " file=" + model3FileUtf8);
            setCrashStage("native load: LAppModel::LoadAssets");
#if defined(_WIN32)
            if (!safeLoadAssets(rt->model.get(), modelDirUtf8.c_str(), model3FileUtf8.c_str()))
            {
                const std::string msg = lastSehMessage("LAppModel::LoadAssets");
                logDiagnostic(msg);
                if (error) *error = msg;
                return false;
            }
#else
            rt->model->LoadAssets(modelDirUtf8.c_str(), model3FileUtf8.c_str());
#endif
            rt->model->DisableHeadBreathing();

            auto* csmModel = rt->model->GetModel();
            if (csmModel)
            {
                int partCount = csmModel->GetPartCount();
                rt->defaultPartOpacities.resize(partCount);
                for (int p = 0; p < partCount; ++p)
                {
                    rt->defaultPartOpacities[p] = csmModel->GetPartOpacity(p);
                }
            }

            logDiagnostic("[NATIVE] load complete id=" + std::to_string(id));
            _models[id] = std::move(rt);
            return true;
        }
        catch (const std::exception& e)
        {
            if (error) *error = e.what();
            return false;
        }
        catch (...)
        {
            if (error) *error = "Unknown exception while loading Live2D model.";
            return false;
        }
    }

    void unload(uint64_t id) override
    {
        _models.erase(id);
    }

    void startMotion(uint64_t id, const std::string& group, int index, bool loop) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed) return;

        resetModelToDefaults(rt);

        // Skip logging for Idle motions to reduce console log noise
        if (group != "Idle" && group != "idle")
        {
            logDiagnostic("[NATIVE] start motion id=" + std::to_string(id) + " group=" + group + " index=" + std::to_string(index));
        }

        setCrashStage("native motion: LAppModel::StartMotion");
#if defined(_WIN32)
        if (!safeStartMotion(rt->model.get(), group.c_str(), index, LAppDefine::PriorityForce))
        {
            rt->nativeCrashed = true;
            rt->lastNativeError = lastSehMessage("LAppModel::StartMotion");
            logDiagnostic(rt->lastNativeError);
            return;
        }
#else
        rt->model->StartMotion(group.c_str(), index, LAppDefine::PriorityForce);
#endif

        auto* motionManager = rt->model->GetMotionManager();
        if (motionManager)
        {
            auto* entries = motionManager->GetCubismMotionQueueEntries();
            if (entries && entries->GetSize() > 0)
            {
                auto* entry = entries->At(entries->GetSize() - 1);
                if (entry)
                {
                    auto* activeMotion = entry->GetCubismMotion();
                    if (activeMotion)
                    {
                        activeMotion->SetLoop(loop);
                    }
                }
            }
        }

        // Track custom motion (ignore Idle or dummy motions)
        if (group != "Idle" && group != "idle")
        {
            rt->hasLastCustomMotion = true;
            rt->lastCustomMotionGroup = group;
            rt->lastCustomMotionIndex = index;
            rt->lastCustomMotionLoop = loop;
            rt->lastCustomMotionTime = 0.0f;
            rt->lastCustomMotionDuration = 0.0f; // Will be set in update
        }
    }

    void resetModelToDefaults(NativeModelRuntime* rt)
    {
        if (!rt || !rt->model || !rt->model->GetModel()) return;

        auto* mqm = rt->model->GetMotionManager();
        if (mqm)
        {
            mqm->StopAllMotions();
        }

        auto* csmModel = rt->model->GetModel();
        
        int paramCount = csmModel->GetParameterCount();
        for (int i = 0; i < paramCount; ++i)
        {
            float defaultValue = csmModel->GetParameterDefaultValue(static_cast<Live2D::Cubism::Framework::csmUint32>(i));
            csmModel->SetParameterValue(i, defaultValue);
        }
        csmModel->SaveParameters();

        int partCount = csmModel->GetPartCount();
        int backupCount = static_cast<int>(rt->defaultPartOpacities.size());
        for (int p = 0; p < partCount && p < backupCount; ++p)
        {
            csmModel->SetPartOpacity(p, rt->defaultPartOpacities[p]);
        }
    }

    void playPlaylistSlot(NativeModelRuntime* rt, int index)
    {
        if (index < 0 || index >= (int)rt->playlistItems.size())
        {
            rt->playlistCurrentIndex = -1;
            auto* mqm = rt->model->GetMotionManager();
            if (mqm)
            {
                mqm->StopAllMotions();
                mqm->SetReservePriority(LAppDefine::PriorityNone);
                mqm->StartMotionPriority(rt->model->GetDummyMotion(), false, LAppDefine::PriorityNone);
            }
            return;
        }

        rt->playlistCurrentIndex = index;
        const auto& item = rt->playlistItems[index];

        // Set properties on the motion instance BEFORE starting it so that
        // the newly created motion queue entry inherits these properties.
        auto* motion = rt->model->GetMotion(item.group, item.index);
        if (motion)
        {
            motion->SetLoop(false);
            motion->SetFadeInTime(item.fadeTime);
        }

        setCrashStage("native playlist motion: LAppModel::StartMotion");
#if defined(_WIN32)
        if (!safeStartMotion(rt->model.get(), item.group.c_str(), item.index, LAppDefine::PriorityForce))
        {
            rt->nativeCrashed = true;
            rt->lastNativeError = lastSehMessage("LAppModel::StartMotion (Playlist)");
            logDiagnostic(rt->lastNativeError);
            return;
        }
#else
        rt->model->StartMotion(item.group.c_str(), item.index, LAppDefine::PriorityForce);
#endif
    }

    void startPlaylist(uint64_t id, const std::vector<BridgePlaylistItem>& items, bool loop) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model) return;

        resetModelToDefaults(rt);

        rt->isPlaylistActive = true;
        rt->playlistLoop = loop;
        rt->playlistItems.clear();
        rt->playlistSlotStartTimes.clear();
        rt->playlistTotalDuration = 0.0;
        rt->playlistCurrentIndex = -1;

        double currentOffset = 0.0;
        for (const auto& item : items)
        {
            NativeModelRuntime::RuntimePlaylistItem rItem;
            rItem.group = item.group;
            rItem.index = item.index;
            rItem.fadeTime = item.fadeTime;

            auto* motion = rt->model->GetMotion(item.group, item.index);
            float duration = 0.0f;
            if (motion)
            {
                duration = motion->GetDuration();
                if (duration < 0.0f) duration = motion->GetLoopDuration();
            }
            rItem.duration = duration;

            rt->playlistSlotStartTimes.push_back(currentOffset);
            currentOffset += duration;
            rt->playlistItems.push_back(rItem);
        }
        rt->playlistTotalDuration = currentOffset;

        rt->cumulativeTime = 0.0;
        rt->lastCustomMotionTime = 0.0f;
        rt->lastCustomMotionDuration = static_cast<float>(rt->playlistTotalDuration);
        rt->hasLastCustomMotion = true;
        rt->lastCustomMotionGroup = "Playlist";
        rt->lastCustomMotionIndex = 0;

        playPlaylistSlot(rt, 0);
    }

    void stopPlaylist(uint64_t id) override
    {
        auto* rt = find(id);
        if (rt)
        {
            rt->isPlaylistActive = false;
            rt->playlistCurrentIndex = -1;
            rt->playlistItems.clear();
            rt->playlistSlotStartTimes.clear();
            rt->playlistTotalDuration = 0.0;
        }
    }

    void stopMotion(uint64_t id) override
    {
        auto* rt = find(id);
        if (rt && !rt->nativeCrashed && rt->model)
        {
            auto* motionManager = rt->model->GetMotionManager();
            if (motionManager)
            {
                motionManager->StopAllMotions();
                motionManager->SetReservePriority(LAppDefine::PriorityNone);
                motionManager->UpdateMotion(rt->model->GetModel(), 0.0f);
            }
            rt->lastCustomMotionTime = 0.0f;
        }
    }

    void setExpression(uint64_t id, const std::string& name) override
    {
        auto* rt = find(id);
        if (rt && !rt->nativeCrashed) rt->model->SetExpression(name.c_str());
    }

    void setDragPoint(uint64_t id, float x, float y) override
    {
        auto* rt = find(id);
        if (rt && !rt->nativeCrashed) rt->model->SetDragging(x, y);
    }

    void setTransform(uint64_t id, float x, float y, float scale, float rotationDeg, float alpha, bool flipX, bool flipY, bool loopMotion) override
    {
        using namespace Live2D::Cubism::Framework;
        auto* rt = find(id);
        if (!rt) return;
        rt->x = x;
        rt->y = y;
        rt->scale = scale;
        rt->rotationDeg = rotationDeg;
        rt->alpha = alpha;
        rt->flipX = flipX;
        rt->flipY = flipY;
        rt->loopMotion = loopMotion;

        // Build the model-local matrix: scale (with flip) then translate.
        // Model rotation is applied here as a 2D rotation matrix.
        if (rt->model && rt->model->GetModelMatrix())
        {
            auto* mm = rt->model->GetModelMatrix();
            mm->LoadIdentity();

            const float sx = flipX ? -scale : scale;
            const float sy = flipY ? -scale : scale;

            if (rotationDeg != 0.0f)
            {
                const float rad = rotationDeg * (3.14159265f / 180.0f);
                const float cosA = std::cos(rad);
                const float sinA = std::sin(rad);
                // Manual 2D rotation + scale matrix (column-major, 4x4):
                // [ sx*cos  -sy*sin  0  0 ]
                // [ sx*sin   sy*cos  0  0 ]
                // [  0        0      1  0 ]
                // [  tx       ty     0  1 ]
                CubismMatrix44 rot;
                rot.LoadIdentity();
                float* arr = rot.GetArray();
                arr[0]  =  sx * cosA;   // col0 row0
                arr[1]  =  sx * sinA;   // col0 row1
                arr[4]  = -sy * sinA;   // col1 row0
                arr[5]  =  sy * cosA;   // col1 row1
                arr[12] = x;            // translation X
                arr[13] = y;            // translation Y
                mm->MultiplyByMatrix(&rot);
            }
            else
            {
                mm->Scale(sx, sy);
                mm->Translate(x, y);
            }
        }
    }

    bool hitTest(uint64_t id, float x, float y) override
    {
        using namespace Live2D::Cubism::Framework;
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model) return false;

        // Check all visible drawables as a general hit-test (so clicking anywhere on the model can select it)
        if (rt->model->GetModel())
        {
            const csmInt32 count = rt->model->GetModel()->GetDrawableCount();
            for (csmInt32 i = 0; i < count; ++i)
            {
                if (rt->model->GetModel()->GetDrawableOpacity(i) > 0.0f)
                {
                    const CubismIdHandle drawID = rt->model->GetModel()->GetDrawableId(i);
                    if (rt->model->IsHit(drawID, x, y))
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void update(uint64_t id, float dtSeconds, bool autoIdle) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed) return;

        bool effectiveAutoIdle = autoIdle;
        if (rt->isPlaylistActive)
        {
            effectiveAutoIdle = false;
        }
        else if (!rt->loopMotion && rt->hasLastCustomMotion)
        {
            effectiveAutoIdle = false;
        }
        rt->model->SetAutoIdleEnabled(effectiveAutoIdle);

        float effectiveDt = rt->motionPaused ? 0.0f : dtSeconds;
        rt->cumulativeTime += effectiveDt;

        if (rt->isPlaylistActive)
        {
            if (rt->playlistItems.empty())
            {
                rt->model->SetAutoIdleEnabled(true);
                rt->isPlaylistActive = false;
            }
            else
            {
                double curTime = rt->cumulativeTime;
                if (rt->playlistTotalDuration > 0.0)
                {
                    if (curTime >= rt->playlistTotalDuration)
                    {
                        if (rt->playlistLoop)
                        {
                            curTime = std::fmod(curTime, rt->playlistTotalDuration);
                            rt->cumulativeTime = curTime;
                            resetModelToDefaults(rt);
                            playPlaylistSlot(rt, 0);
                        }
                        else
                        {
                            curTime = rt->playlistTotalDuration;
                            rt->cumulativeTime = curTime;
                            if (rt->playlistCurrentIndex != -1)
                            {
                                rt->playlistCurrentIndex = -1;
                                auto* mqm = rt->model->GetMotionManager();
                                if (mqm)
                                {
                                    mqm->StopAllMotions();
                                    mqm->SetReservePriority(LAppDefine::PriorityNone);
                                    mqm->StartMotionPriority(rt->model->GetDummyMotion(), false, LAppDefine::PriorityNone);
                                }
                            }
                        }
                    }
                }

                if (rt->playlistCurrentIndex != -1)
                {
                    int targetSlot = -1;
                    for (int i = 0; i < (int)rt->playlistItems.size(); ++i)
                    {
                        double startTime = rt->playlistSlotStartTimes[i];
                        double endTime = startTime + rt->playlistItems[i].duration;
                        if (curTime >= startTime && curTime < endTime)
                        {
                            targetSlot = i;
                            break;
                        }
                    }
                    if (targetSlot == -1 && !rt->playlistItems.empty() && curTime < rt->playlistTotalDuration)
                    {
                        targetSlot = 0;
                    }

                    if (targetSlot != -1 && targetSlot != rt->playlistCurrentIndex)
                    {
                        playPlaylistSlot(rt, targetSlot);
                        auto* mqm = rt->model->GetMotionManager();
                        if (mqm)
                        {
                            auto* entries = mqm->GetCubismMotionQueueEntries();
                            if (entries && entries->GetSize() > 0)
                            {
                                auto* entry = entries->At(entries->GetSize() - 1);
                                if (entry && entry->GetCubismMotion() != rt->model->GetDummyMotion())
                                {
                                    double slotExpectStart = rt->playlistSlotStartTimes[targetSlot];
                                    double actualStartOffset = curTime - slotExpectStart;
                                    entry->SetStartTime(rt->cumulativeTime - actualStartOffset);
                                    entry->SetFadeInStartTime(rt->cumulativeTime - actualStartOffset);
                                }
                            }
                        }
                    }
                }

                rt->lastCustomMotionDuration = static_cast<float>(rt->playlistTotalDuration);
                rt->lastCustomMotionTime = static_cast<float>(curTime);
            }
        }
        else
        {
            using namespace Live2D::Cubism::Framework;
            auto* mqm = rt->model->GetMotionManager();
            bool stillActive = false;
            if (mqm)
            {
                auto* entries = mqm->GetCubismMotionQueueEntries();
                if (entries)
                {
                    for (csmUint32 i = 0; i < entries->GetSize(); ++i)
                    {
                        auto* entry = (*entries)[i];
                        if (entry && !entry->IsFinished() && entry->GetCubismMotion())
                        {
                            if (entry->GetCubismMotion() != rt->model->GetDummyMotion())
                            {
                                entry->GetCubismMotion()->SetLoop(rt->loopMotion);
                                float dur = entry->GetCubismMotion()->GetDuration();
                                if (dur < 0.0f) dur = entry->GetCubismMotion()->GetLoopDuration();
                                rt->lastCustomMotionDuration = dur;

                                float localTime = static_cast<float>(rt->cumulativeTime) - entry->GetStartTime();
                                if (localTime < 0.0f) localTime = 0.0f;
                                rt->lastCustomMotionTime = localTime;
                                stillActive = true;

                                if (!rt->hasLastCustomMotion)
                                {
                                    rt->hasLastCustomMotion = true;
                                    rt->lastCustomMotionGroup = "Idle";
                                    rt->lastCustomMotionIndex = 0;
                                    rt->lastCustomMotionLoop = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }
            if (rt->hasLastCustomMotion && !stillActive)
            {
                rt->lastCustomMotionTime = rt->lastCustomMotionDuration;
            }
        }

        LAppPal_SetOverrideTime(rt->cumulativeTime, effectiveDt);

        LAppPal::UpdateTime();
        setCrashStage("native update: LAppModel::Update");
#if defined(_WIN32)
        if (!safeUpdateModel(rt->model.get()))
        {
            rt->nativeCrashed = true;
            rt->lastNativeError = lastSehMessage("LAppModel::Update");
            logDiagnostic(rt->lastNativeError);
            LAppPal_ClearOverrideTime();
            return;
        }
#else
        rt->model->Update();
#endif

        LAppPal_ClearOverrideTime();
    }

    void draw(uint64_t id, int width, int height, float panX, float panY, float zoom, float viewRotDeg, bool viewFlipX, bool viewFlipY, bool showWireframe, bool showHitAreas) override
    {
        using namespace Live2D::Cubism::Framework;
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed) return;
        LAppDelegate::GetInstance()->SetExternalContextSize(width, height);

        if (showWireframe)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        // Build a pure camera/projection matrix (no model transforms here):
        // aspect correction, camera zoom, camera pan, viewport flip, viewport rotation.
        const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        const float sx = viewFlipX ? -1.0f : 1.0f;
        const float sy = viewFlipY ? -1.0f : 1.0f;

        CubismMatrix44 matrix;
        matrix.LoadIdentity();

        if (viewRotDeg != 0.0f)
        {
            const float rad = viewRotDeg * (3.14159265f / 180.0f);
            const float cosA = std::cos(rad);
            const float sinA = std::sin(rad);
            // Build rotation-only 4x4 and multiply into matrix
            CubismMatrix44 rot;
            rot.LoadIdentity();
            float* arr = rot.GetArray();
            arr[0] = cosA;  arr[1] = sinA;
            arr[4] = -sinA; arr[5] = cosA;
            matrix.MultiplyByMatrix(&rot);
        }

        // Apply aspect + zoom + pan + flip as a scale/translate on top
        CubismMatrix44 cam;
        cam.LoadIdentity();
        cam.Scale(sx * zoom / aspect, sy * zoom);
        cam.Translate(panX, panY);
        matrix.MultiplyByMatrix(&cam);

        if (rt->model && rt->model->GetRenderer<Rendering::CubismRenderer_OpenGLES2>())
        {
            rt->model->GetRenderer<Rendering::CubismRenderer_OpenGLES2>()->SetModelColor(1.0f, 1.0f, 1.0f, rt->alpha);
        }

        setCrashStage("native draw: LAppModel::Draw");
#if defined(_WIN32)
        if (!safeDrawModel(rt->model.get(), &matrix))
        {
            rt->nativeCrashed = true;
            rt->lastNativeError = lastSehMessage("LAppModel::Draw");
            logDiagnostic(rt->lastNativeError);
            if (showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            return;
        }
#else
        rt->model->Draw(matrix);
#endif

        if (showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if (showHitAreas && rt->model && rt->model->GetModel())
        {
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            glUseProgram(0);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            
            Live2D::Cubism::Framework::CubismMatrix44 finalMatrix = matrix;
            if (rt->model->GetModelMatrix())
            {
                finalMatrix.MultiplyByMatrix(rt->model->GetModelMatrix());
            }
            glLoadMatrixf(finalMatrix.GetArray());

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            glDisable(GL_TEXTURE_2D);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            const csmInt32 count = rt->model->GetModel()->GetDrawableCount();
            for (csmInt32 i = 0; i < count; ++i)
            {
                const auto* drawID = rt->model->GetModel()->GetDrawableId(i);
                std::string idStr = drawID ? drawID->GetString().GetRawString() : "";
                
                std::string lowerId = idStr;
                std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
                
                if (lowerId.find("hit") != std::string::npos)
                {
                    const csmInt32 vertexCount = rt->model->GetModel()->GetDrawableVertexCount(i);
                    const csmFloat32* vertices = rt->model->GetModel()->GetDrawableVertices(i);
                    if (vertexCount > 0 && vertices)
                    {
                        csmFloat32 left = vertices[0];
                        csmFloat32 right = vertices[0];
                        csmFloat32 top = vertices[1];
                        csmFloat32 bottom = vertices[1];
                        for (csmInt32 j = 1; j < vertexCount; ++j)
                        {
                            csmFloat32 vx = vertices[2 * j];
                            csmFloat32 vy = vertices[2 * j + 1];
                            if (vx < left) left = vx;
                            if (vx > right) right = vx;
                            if (vy < top) top = vy;
                            if (vy > bottom) bottom = vy;
                        }

                        // Fill
                        glColor4f(1.0f, 0.0f, 0.0f, 0.2f);
                        glBegin(GL_QUADS);
                        glVertex2f(left, top);
                        glVertex2f(right, top);
                        glVertex2f(right, bottom);
                        glVertex2f(left, bottom);
                        glEnd();

                        // Outline
                        glColor4f(1.0f, 0.0f, 0.0f, 0.8f);
                        glLineWidth(2.0f);
                        glBegin(GL_LINE_LOOP);
                        glVertex2f(left, top);
                        glVertex2f(right, top);
                        glVertex2f(right, bottom);
                        glVertex2f(left, bottom);
                        glEnd();
                    }
                }
            }

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glPopAttrib();
        }
    }

    void prepareForExport(uint64_t id) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed) return;

        resetModelToDefaults(rt);

        rt->isFirstExportFrame = true;
        rt->lastExportTime = 0.0;
        if (rt->isPlaylistActive)
        {
            rt->cumulativeTime = 0.0;
            playPlaylistSlot(rt, 0);
        }
    }

    void drawAtTime(uint64_t id, int width, int height, float absoluteTimeSeconds, float panX, float panY, float zoom, float viewRotDeg, bool viewFlipX, bool viewFlipY) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed) return;

        double dt = 0.0;
        if (rt->isFirstExportFrame)
        {
            rt->isFirstExportFrame = false;
            rt->lastExportTime = absoluteTimeSeconds;
            dt = 0.0;
        }
        else
        {
            dt = absoluteTimeSeconds - rt->lastExportTime;
            rt->lastExportTime = absoluteTimeSeconds;
        }

        update(id, static_cast<float>(dt), false);

        draw(id, width, height, panX, panY, zoom, viewRotDeg, viewFlipX, viewFlipY, false, false);
    }

    float getMotionTime(uint64_t id) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed) return 0.0f;
        if (rt->hasLastCustomMotion)
        {
            return rt->lastCustomMotionTime;
        }
        return 0.0f;
    }

    float getMotionDuration(uint64_t id) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed) return 0.0f;
        if (rt->hasLastCustomMotion)
        {
            return rt->lastCustomMotionDuration;
        }
        return 0.0f;
    }

    void setMotionTime(uint64_t id, float timeSeconds) override
    {
        using namespace Live2D::Cubism::Framework;
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model) return;

        if (rt->isPlaylistActive)
        {
            if (rt->playlistItems.empty()) return;

            if (timeSeconds < 0.0f) timeSeconds = 0.0f;
            if (timeSeconds < 0.001f)
            {
                resetModelToDefaults(rt);
            }
            if (timeSeconds > rt->playlistTotalDuration)
            {
                timeSeconds = (float)rt->playlistTotalDuration;
            }

            int targetSlot = -1;
            for (int i = 0; i < (int)rt->playlistItems.size(); ++i)
            {
                double startTime = rt->playlistSlotStartTimes[i];
                double endTime = startTime + rt->playlistItems[i].duration;
                if (timeSeconds >= startTime && timeSeconds <= endTime)
                {
                    targetSlot = i;
                    break;
                }
            }
            if (targetSlot == -1 && !rt->playlistItems.empty())
            {
                targetSlot = (int)rt->playlistItems.size() - 1;
            }

            rt->cumulativeTime = timeSeconds;
            rt->lastCustomMotionTime = timeSeconds;

            playPlaylistSlot(rt, targetSlot);

            auto* mqm = rt->model->GetMotionManager();
            if (mqm)
            {
                auto* entries = mqm->GetCubismMotionQueueEntries();
                if (entries && entries->GetSize() > 0)
                {
                    auto* entry = entries->At(entries->GetSize() - 1);
                    if (entry && entry->GetCubismMotion() != rt->model->GetDummyMotion())
                    {
                        double slotExpectStart = rt->playlistSlotStartTimes[targetSlot];
                        double actualStartOffset = timeSeconds - slotExpectStart;
                        entry->SetStartTime(rt->cumulativeTime - actualStartOffset);
                        entry->SetFadeInStartTime(rt->cumulativeTime - actualStartOffset);
                    }
                }
            }
        }
        else
        {
            if (rt->hasLastCustomMotion)
            {
                auto* mqm = rt->model->GetMotionManager();
                if (!mqm) return;

                auto* entries = mqm->GetCubismMotionQueueEntries();
                bool found = false;
                if (entries)
                {
                    for (csmUint32 i = 0; i < entries->GetSize(); ++i)
                    {
                        auto* entry = (*entries)[i];
                        if (entry && !entry->IsFinished() && entry->GetCubismMotion())
                        {
                            if (entry->GetCubismMotion() != rt->model->GetDummyMotion())
                            {
                                float localTime = static_cast<float>(rt->cumulativeTime) - entry->GetStartTime();
                                float delta = timeSeconds - localTime;
                                entry->SetStartTime(entry->GetStartTime() - delta);
                                entry->SetFadeInStartTime(entry->GetFadeInStartTime() - delta);
                                entry->SetEndTime(entry->GetEndTime() - delta);
                                rt->lastCustomMotionTime = timeSeconds;
                                found = true;
                                break;
                            }
                        }
                    }
                }
                if (!found)
                {
                    startMotion(id, rt->lastCustomMotionGroup, rt->lastCustomMotionIndex, rt->loopMotion);
                    entries = mqm->GetCubismMotionQueueEntries();
                    if (entries)
                    {
                        for (csmUint32 i = 0; i < entries->GetSize(); ++i)
                        {
                            auto* entry = (*entries)[i];
                            if (entry && !entry->IsFinished() && entry->GetCubismMotion())
                            {
                                if (entry->GetCubismMotion() != rt->model->GetDummyMotion())
                                {
                                    float localTime = static_cast<float>(rt->cumulativeTime) - entry->GetStartTime();
                                    float delta = timeSeconds - localTime;
                                    entry->SetStartTime(entry->GetStartTime() - delta);
                                    entry->SetFadeInStartTime(entry->GetFadeInStartTime() - delta);
                                    entry->SetEndTime(entry->GetEndTime() - delta);
                                    rt->lastCustomMotionTime = timeSeconds;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void setMotionPaused(uint64_t id, bool paused) override
    {
        auto* rt = find(id);
        if (rt)
        {
            rt->motionPaused = paused;
            if (!paused && rt->hasLastCustomMotion && rt->model)
            {
                using namespace Live2D::Cubism::Framework;
                auto* mqm = rt->model->GetMotionManager();
                bool found = false;
                if (mqm)
                {
                    auto* entries = mqm->GetCubismMotionQueueEntries();
                    if (entries)
                    {
                        for (csmUint32 i = 0; i < entries->GetSize(); ++i)
                        {
                            auto* entry = (*entries)[i];
                            if (entry && !entry->IsFinished() && entry->GetCubismMotion())
                            {
                                if (entry->GetCubismMotion() != rt->model->GetDummyMotion())
                                {
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!found)
                {
                    startMotion(id, rt->lastCustomMotionGroup, rt->lastCustomMotionIndex, rt->loopMotion);
                }
            }
        }
    }

    bool isMotionPaused(uint64_t id) override
    {
        auto* rt = find(id);
        return rt ? rt->motionPaused : false;
    }

    int getPartCount(uint64_t id) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return 0;
        return rt->model->GetModel()->GetPartCount();
    }

    const char* getPartId(uint64_t id, int index) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return "";
        auto partId = rt->model->GetModel()->GetPartId(static_cast<Live2D::Cubism::Framework::csmUint32>(index));
        return partId ? partId->GetString().GetRawString() : "";
    }

    float getPartOpacity(uint64_t id, int index) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return 1.0f;
        return rt->model->GetModel()->GetPartOpacity(index);
    }

    void setPartOpacity(uint64_t id, int index, float opacity) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return;
        rt->model->GetModel()->SetPartOpacity(index, opacity);
    }

    int getParameterCount(uint64_t id) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return 0;
        return rt->model->GetModel()->GetParameterCount();
    }

    const char* getParameterId(uint64_t id, int index) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return "";
        auto paramId = rt->model->GetModel()->GetParameterId(static_cast<Live2D::Cubism::Framework::csmUint32>(index));
        return paramId ? paramId->GetString().GetRawString() : "";
    }

    float getParameterValue(uint64_t id, int index) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return 0.0f;
        return rt->model->GetModel()->GetParameterValue(index);
    }

    void setParameterValue(uint64_t id, int index, float value) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return;
        rt->model->GetModel()->SetParameterValue(index, value);
    }

    float getParameterMin(uint64_t id, int index) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return -1.0f;
        return rt->model->GetModel()->GetParameterMinimumValue(static_cast<Live2D::Cubism::Framework::csmUint32>(index));
    }

    float getParameterMax(uint64_t id, int index) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return 1.0f;
        return rt->model->GetModel()->GetParameterMaximumValue(static_cast<Live2D::Cubism::Framework::csmUint32>(index));
    }

    float getParameterDefault(uint64_t id, int index) override
    {
        auto* rt = find(id);
        if (!rt || rt->nativeCrashed || !rt->model || !rt->model->GetModel()) return 0.0f;
        return rt->model->GetModel()->GetParameterDefaultValue(static_cast<Live2D::Cubism::Framework::csmUint32>(index));
    }

private:
    NativeModelRuntime* find(uint64_t id)
    {
        auto it = _models.find(id);
        return it == _models.end() ? nullptr : it->second.get();
    }

    LAppAllocator_Common _allocator;
    Live2D::Cubism::Framework::CubismFramework::Option _option{};
    bool _cubismStarted = false;
    std::unordered_map<uint64_t, std::unique_ptr<NativeModelRuntime>> _models;
};

std::unique_ptr<INativeCubismBridge> createNativeCubismBridge()
{
    return std::make_unique<CubismNativeSamplesBridge>();
}

} // namespace l2dgui
#endif
