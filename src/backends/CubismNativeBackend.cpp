#include "backends/CubismNativeBackend.hpp"
#include "util/PathUtil.hpp"
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace Live2D { namespace Cubism { namespace Framework {
extern int g_steppedInterpolationMode;
}}}

namespace l2dgui
{

static void injectIdleMotionIfNeeded(std::string& jsonContent)
{
    if (jsonContent.find("\"Idle\"") != std::string::npos || jsonContent.find("\"idle\"") != std::string::npos)
    {
        return;
    }

    size_t motionsPos = jsonContent.find("\"Motions\"");
    if (motionsPos == std::string::npos) return;

    size_t openBrace = jsonContent.find("{", motionsPos);
    if (openBrace == std::string::npos) return;

    std::string candidateKey = "";
    size_t candidateStartBracket = std::string::npos;
    size_t candidateEndBracket = std::string::npos;

    int braceCount = 1;
    size_t i = openBrace + 1;
    while (i < jsonContent.size() && braceCount > 0)
    {
        char c = jsonContent[i];
        if (c == '{')
        {
            braceCount++;
            i++;
            continue;
        }
        else if (c == '}')
        {
            braceCount--;
            i++;
            continue;
        }

        // Parse key only if we are at the direct child depth of the Motions object
        if (braceCount == 1 && c == '"')
        {
            size_t keyStart = i + 1;
            size_t keyEnd = jsonContent.find("\"", keyStart);
            if (keyEnd == std::string::npos) break;

            std::string key = jsonContent.substr(keyStart, keyEnd - keyStart);
            
            // Look for the colon ':'
            size_t colonPos = keyEnd + 1;
            while (colonPos < jsonContent.size() && std::isspace(static_cast<unsigned char>(jsonContent[colonPos])))
            {
                colonPos++;
            }

            if (colonPos < jsonContent.size() && jsonContent[colonPos] == ':')
            {
                // Look for the bracket '['
                size_t bracketPos = colonPos + 1;
                while (bracketPos < jsonContent.size() && std::isspace(static_cast<unsigned char>(jsonContent[bracketPos])))
                {
                    bracketPos++;
                }

                if (bracketPos < jsonContent.size() && jsonContent[bracketPos] == '[')
                {
                    // Find the matching ']'
                    size_t endBracketPos = std::string::npos;
                    int bracketDepth = 0;
                    for (size_t k = bracketPos; k < jsonContent.size(); ++k)
                    {
                        if (jsonContent[k] == '[') bracketDepth++;
                        else if (jsonContent[k] == ']')
                        {
                            bracketDepth--;
                            if (bracketDepth == 0)
                            {
                                endBracketPos = k;
                                break;
                            }
                        }
                    }

                    if (endBracketPos != std::string::npos)
                    {
                        std::string lowerKey = key;
                        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

                        if (lowerKey.find("loop") != std::string::npos)
                        {
                            candidateKey = key;
                            candidateStartBracket = bracketPos;
                            candidateEndBracket = endBracketPos;
                            break; // Prioritize loop motion and exit search
                        }
                        else if (candidateKey.empty())
                        {
                            candidateKey = key;
                            candidateStartBracket = bracketPos;
                            candidateEndBracket = endBracketPos;
                        }
                    }
                }
            }
            i = keyEnd + 1;
        }
        else
        {
            i++;
        }
    }

    if (!candidateKey.empty() && candidateStartBracket != std::string::npos && candidateEndBracket != std::string::npos)
    {
        std::string arrayContent = jsonContent.substr(candidateStartBracket, candidateEndBracket - candidateStartBracket + 1);
        std::string injectStr = "\n    \"Idle\": " + arrayContent + ",";
        jsonContent.insert(openBrace + 1, injectStr);
        std::cout << "[BACKEND] Injected virtual 'Idle' motion group copied from '" << candidateKey << "'" << std::endl;
    }
}

#if !defined(L2D_ENABLE_CUBISM_NATIVE)
class MissingNativeCubismBridge final : public INativeCubismBridge
{
public:
    bool initialize(std::string* error) override
    {
        if (error) *error = "L2D_ENABLE_CUBISM_NATIVE=OFF. Reconfigure CMake with -DL2D_ENABLE_CUBISM_NATIVE=ON and provide a bridge implementation.";
        return false;
    }
    void shutdown() override {}
    bool load(uint64_t, const std::string&, const std::string&, std::string*) override { return false; }
    void unload(uint64_t) override {}
    void startMotion(uint64_t, const std::string&, int, bool) override {}
    void stopMotion(uint64_t) override {}
    void startPlaylist(uint64_t, const std::vector<BridgePlaylistItem>&, bool) override {}
    void stopPlaylist(uint64_t) override {}
    void setExpression(uint64_t, const std::string&) override {}
    void setDragPoint(uint64_t, float, float) override {}
    void setTransform(uint64_t, float, float, float, float, float, bool, bool, bool) override {}
    void update(uint64_t, float, bool) override {}
    void draw(uint64_t, int, int, float, float, float, bool, bool) override {}
    void drawAtTime(uint64_t, int, int, float) override {}
    float getMotionFadeInTime(uint64_t, const std::string&, int) override { return -1.0f; }
    void  setMotionBlendThreshold(uint64_t, float) override {}
};

std::unique_ptr<INativeCubismBridge> createNativeCubismBridge()
{
    return std::make_unique<MissingNativeCubismBridge>();
}
#endif

CubismNativeBackend::CubismNativeBackend() = default;
CubismNativeBackend::~CubismNativeBackend() = default;

bool CubismNativeBackend::initialize()
{
    _bridge = createNativeCubismBridge();
    std::string error;
    if (!_bridge->initialize(&error))
    {
        throw std::runtime_error(error);
    }
    return true;
}

void CubismNativeBackend::shutdown()
{
    if (_bridge) _bridge->shutdown();
    _bridge.reset();
}

LoadResult CubismNativeBackend::loadModel(ModelEntry& entry)
{
    if (!_bridge) return {false, "Native bridge is not initialized."};

    std::error_code ec;
    // Always stage the model folder to a staging directory to inject virtual Idle motion if missing
    const auto stageRoot = std::filesystem::temp_directory_path(ec) / "l2d_viewer_gui_stage";
    const auto stageDir = stageRoot / ("model_" + std::to_string(entry.id));
    std::filesystem::remove_all(stageDir, ec);
    ec.clear();
    std::filesystem::create_directories(stageDir, ec);
    if (ec)
    {
        return {false, "Failed to create staging directory: " + ec.message()};
    }

    std::filesystem::copy(entry.baseDir, stageDir,
        std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        return {false, "Failed to copy model folder to staging directory: " + ec.message()};
    }

    const auto runtimeBaseDir = stageDir;
    const auto runtimeModel3 = stageDir / entry.model3Json.filename();

    // Read, modify, and overwrite model3.json to inject Idle motion if missing
    std::ifstream inFile(runtimeModel3, std::ios::in | std::ios::binary);
    if (inFile.is_open())
    {
        std::stringstream ss;
        ss << inFile.rdbuf();
        inFile.close();

        std::string content = ss.str();
        injectIdleMotionIfNeeded(content);

        std::ofstream outFile(runtimeModel3, std::ios::out | std::ios::binary | std::ios::trunc);
        if (outFile.is_open())
        {
            outFile << content;
            outFile.close();
        }
    }

    std::string error;
    const auto dir = utf8Path(runtimeBaseDir) + "/";
    const auto file = utf8Path(runtimeModel3.filename());
    if (!_bridge->load(entry.id, dir, file, &error))
    {
        return {false, error.empty() ? "Failed to load model." : error};
    }
    return {true, {}};
}

void CubismNativeBackend::unloadModel(uint64_t modelId)
{
    if (_bridge) _bridge->unload(modelId);
}

void CubismNativeBackend::playMotion(uint64_t modelId, const MotionItem& motion, bool loop)
{
    if (_bridge)
    {
        _bridge->stopPlaylist(modelId);
        _bridge->startMotion(modelId, motion.group, motion.index, loop);
    }
}

void CubismNativeBackend::stopMotion(uint64_t modelId)
{
    if (_bridge)
    {
        _bridge->stopPlaylist(modelId);
        _bridge->stopMotion(modelId);
    }
}

void CubismNativeBackend::startPlaylist(uint64_t modelId, const std::vector<PlaylistItem>& playlist, bool loop)
{
    if (!_bridge) return;
    std::vector<BridgePlaylistItem> bridgeItems;
    for (const auto& item : playlist)
    {
        BridgePlaylistItem bItem;
        bItem.group = item.group;
        bItem.index = item.index;
        bItem.fadeTime = item.fadeTime;
        bridgeItems.push_back(bItem);
    }
    _bridge->startPlaylist(modelId, bridgeItems, loop);
}

void CubismNativeBackend::stopPlaylist(uint64_t modelId)
{
    if (_bridge) _bridge->stopPlaylist(modelId);
}

void CubismNativeBackend::setExpression(uint64_t modelId, const ExpressionItem& expression)
{
    if (_bridge) _bridge->setExpression(modelId, expression.name);
}

void CubismNativeBackend::setDragPoint(uint64_t modelId, float x, float y)
{
    if (_bridge) _bridge->setDragPoint(modelId, x, y);
}

void CubismNativeBackend::update(AppState& state, float dtSeconds)
{
    if (!_bridge) return;
    for (const auto& m : state.models)
    {
#if defined(L2D_ENABLE_CUBISM_NATIVE)
        Live2D::Cubism::Framework::g_steppedInterpolationMode = m.steppedInterpolationMode;
#endif
        _bridge->setTransform(m.id, m.x, m.y, m.scale, m.rotationDeg, m.alpha, m.flipX, m.flipY, m.loopMotion);
        _bridge->setMotionBlendThreshold(m.id, m.motionBlendDeltaThreshold);
        _bridge->update(m.id, dtSeconds * m.timeScale, m.autoIdle);
    }
}

void CubismNativeBackend::draw(const AppState& state, int width, int height, const ViewportCamera& camera)
{
    if (!_bridge) return;
    for (const auto& m : state.models)
    {
        if (!m.visible) continue;
        if (state.view.onlySelected && !m.selected) continue;
        _bridge->draw(m.id, width, height, camera.panX, camera.panY, camera.zoom,
                      state.view.rotationDeg, state.view.flipX, state.view.flipY,
                      state.view.showWireframe, state.view.showHitAreas);
    }
}

void CubismNativeBackend::prepareForExport(uint64_t modelId)
{
    if (!_bridge) return;
    _bridge->prepareForExport(modelId);
}

void CubismNativeBackend::drawForExport(const AppState& state, int width, int height, const ViewportCamera& camera, float absoluteTimeSeconds)
{
    if (!_bridge) return;
    setHideRedOption(state.view.hideRed);
    for (const auto& m : state.models)
    {
        if (!m.visible) continue;
        if (state.exportSettings.selectedOnly && !m.selected) continue;
#if defined(L2D_ENABLE_CUBISM_NATIVE)
        Live2D::Cubism::Framework::g_steppedInterpolationMode = m.steppedInterpolationMode;
#endif
        _bridge->setTransform(m.id, m.x, m.y, m.scale, m.rotationDeg, m.alpha, m.flipX, m.flipY, m.loopMotion);
        _bridge->drawAtTime(m.id, width, height, absoluteTimeSeconds,
                            camera.panX, camera.panY, camera.zoom,
                            state.view.rotationDeg, state.view.flipX, state.view.flipY);
    }
}

extern "C" void LAppPal_SetExporting(bool exporting);

void CubismNativeBackend::setExporting(bool exporting)
{
    LAppPal_SetExporting(exporting);
}

extern "C" bool g_hideRedOption;

void CubismNativeBackend::setHideRedOption(bool enable)
{
    g_hideRedOption = enable;
}

bool CubismNativeBackend::hitTest(uint64_t modelId, float x, float y)
{
    if (!_bridge) return false;
    return _bridge->hitTest(modelId, x, y);
}

float CubismNativeBackend::getMotionTime(uint64_t modelId)
{
    if (!_bridge) return 0.0f;
    return _bridge->getMotionTime(modelId);
}

float CubismNativeBackend::getMotionDuration(uint64_t modelId)
{
    if (!_bridge) return 0.0f;
    return _bridge->getMotionDuration(modelId);
}

void CubismNativeBackend::setMotionTime(uint64_t modelId, float timeSeconds)
{
    if (!_bridge) return;
    _bridge->setMotionTime(modelId, timeSeconds);
}

void CubismNativeBackend::setMotionPaused(uint64_t modelId, bool paused)
{
    if (!_bridge) return;
    _bridge->setMotionPaused(modelId, paused);
}

bool CubismNativeBackend::isMotionPaused(uint64_t modelId)
{
    if (!_bridge) return false;
    return _bridge->isMotionPaused(modelId);
}

float CubismNativeBackend::getMotionFadeInTime(uint64_t modelId, const std::string& group, int index)
{
    if (!_bridge) return -1.0f;
    return _bridge->getMotionFadeInTime(modelId, group, index);
}

void CubismNativeBackend::setMotionBlendThreshold(uint64_t modelId, float threshold)
{
    if (_bridge) _bridge->setMotionBlendThreshold(modelId, threshold);
}

int CubismNativeBackend::getPartCount(uint64_t modelId)
{
    if (!_bridge) return 0;
    return _bridge->getPartCount(modelId);
}

const char* CubismNativeBackend::getPartId(uint64_t modelId, int index)
{
    if (!_bridge) return "";
    return _bridge->getPartId(modelId, index);
}

float CubismNativeBackend::getPartOpacity(uint64_t modelId, int index)
{
    if (!_bridge) return 1.0f;
    return _bridge->getPartOpacity(modelId, index);
}

void CubismNativeBackend::setPartOpacity(uint64_t modelId, int index, float opacity)
{
    if (!_bridge) return;
    _bridge->setPartOpacity(modelId, index, opacity);
}

int CubismNativeBackend::getParameterCount(uint64_t modelId)
{
    if (!_bridge) return 0;
    return _bridge->getParameterCount(modelId);
}

const char* CubismNativeBackend::getParameterId(uint64_t modelId, int index)
{
    if (!_bridge) return "";
    return _bridge->getParameterId(modelId, index);
}

float CubismNativeBackend::getParameterValue(uint64_t modelId, int index)
{
    if (!_bridge) return 0.0f;
    return _bridge->getParameterValue(modelId, index);
}

void CubismNativeBackend::setParameterValue(uint64_t modelId, int index, float value)
{
    if (!_bridge) return;
    _bridge->setParameterValue(modelId, index, value);
}

float CubismNativeBackend::getParameterMin(uint64_t modelId, int index)
{
    if (!_bridge) return -1.0f;
    return _bridge->getParameterMin(modelId, index);
}

float CubismNativeBackend::getParameterMax(uint64_t modelId, int index)
{
    if (!_bridge) return 1.0f;
    return _bridge->getParameterMax(modelId, index);
}

float CubismNativeBackend::getParameterDefault(uint64_t modelId, int index)
{
    if (!_bridge) return 0.0f;
    return _bridge->getParameterDefault(modelId, index);
}

} // namespace l2dgui
