#include "app/GuiApplication.hpp"
#include "render/ModelScanner.hpp"
#include "util/CrashDiagnostics.hpp"
#include "util/PathUtil.hpp"
#include "util/NativeDialogs.hpp"
#include "util/ProjectSerializer.hpp"
#include "gui/ConsoleLog.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace l2dgui
{

static std::string getCurrentTimestampString()
{
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t t = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d%H%M%S");
    return oss.str();
}

GuiApplication::GuiApplication() = default;

GuiApplication::~GuiApplication()
{
    if (_backend) _backend->shutdown();
    if (_initialized)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (_window) glfwDestroyWindow(_window);
    glfwTerminate();
}

bool GuiApplication::initialize(int argc, char** argv)
{
    setCrashStage("app initialize: glfwInit");
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    _window = glfwCreateWindow(1440, 900, "L2D Viewer GUI Native MVP", nullptr, nullptr);
    if (!_window) return false;
    glfwMakeContextCurrent(_window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        throw std::runtime_error("Failed to initialize GLEW.");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsLight();

    // Load Segoe UI (Windows system font) with Latin + Misc symbols range so
    // Unicode label strings like ▶ ⏸ ⬜ ⟲ ☰ ⚙ render correctly.
    {
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        static const ImWchar ranges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x2000, 0x2BFF, // General Punctuation, Arrows, Math, Misc symbols
            0xAC00, 0xD7A3, // Hangul Syllables (가-힣)
            0,
        };
        ImFont* font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/malgun.ttf", 15.0f, &cfg, ranges);
        if (!font)
        {
            font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 15.0f, &cfg, ranges);
        }
        if (!font)
        {
            // Fallback: default font (ASCII only)
            io.Fonts->AddFontDefault();
        }
    }
    // Premium light style overrides (applied once, after font configuration)
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    
    // Soft clean light palette colors
    style.Colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.96f, 0.96f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.84f, 0.87f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.78f, 0.88f, 1.00f, 1.00f);       // 확실한 파스텔 블루 선택색
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.92f, 1.00f, 1.00f); // 마우스 호버 시 살짝 연한 파란색
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.68f, 0.82f, 0.98f, 1.00f);  // 클릭 시 더 짙은 파란색
    style.Colors[ImGuiCol_Button] = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.88f, 0.93f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.78f, 0.81f, 0.88f, 1.00f);
    
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(24.0f/255.0f, 119.0f/255.0f, 242.0f/255.0f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(12.0f/255.0f, 95.0f/255.0f, 200.0f/255.0f, 1.00f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.96f, 0.96f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.86f, 0.86f, 0.88f, 1.00f);
    
    ImGui_ImplGlfw_InitForOpenGL(_window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    _initialized = true;

    glfwSetWindowUserPointer(_window, this);
    glfwSetDropCallback(_window, &GuiApplication::glfwDropCallback);
    glfwSetScrollCallback(_window, &GuiApplication::glfwScrollCallback);

    setCrashStage("app initialize: backend initialize");
    _backend = createBackend();
    _backend->initialize();
    logDiagnostic("[APP] backend initialized: " + std::string(_backend->name()));

    _state.browserDir = std::filesystem::current_path();
    loadConfig();
    _state.browserModels = ModelScanner::findModels(_state.browserDir, 5);

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--test-export")
        {
            _testExportMode = true;
        }
        else
        {
            requestImportPath(pathFromUtf8(argv[i]));
        }
    }

    _lastTime = glfwGetTime();
    pushUndoState("Initial State");
    return true;
}

float GuiApplication::deltaTime()
{
    const double now = glfwGetTime();
    const float dt = static_cast<float>(now - _lastTime);
    _lastTime = now;
    return std::clamp(dt, 0.0f, 0.1f);
}

int GuiApplication::run()
{
    if (_testExportMode)
    {
        std::cout << "[TEST] Starting automated export test..." << std::endl;
        _state.exportSettings.width = 512;
        _state.exportSettings.height = 512;
        _state.exportSettings.seconds = 1.0f;
        _state.exportSettings.exportType = EXPORT_FRAME_SEQUENCE;
        _state.exportSettings.outputFolder = pathFromUtf8("exports");

        // Draw a dummy frame to initialize OpenGL/Live2D renderer state before context switching
        _backend->update(_state, 0.0f);
        _backend->draw(_state, 512, 512, _camera);

        requestExport();
        std::cout << "[TEST] Export result message: " << _state.exportStatus.message << std::endl;
        std::cout << "[TEST] Export progress: " << _state.exportStatus.progress << std::endl;
        std::cout << "[TEST] Export last command: " << _state.exportStatus.lastCommand << std::endl;

        saveConfig();

        if (_state.exportStatus.message.find("Export complete") != std::string::npos)
        {
            std::cout << "[TEST] SUCCESS: Export finished successfully." << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "[TEST] FAILURE: Export failed. Message: " << _state.exportStatus.message << std::endl;
            return 1;
        }
    }

    while (!glfwWindowShouldClose(_window))
    {
        double frameStartTime = glfwGetTime();
        setCrashStage("app run: poll events");
        glfwPollEvents();
        const float dt = deltaTime();

        // Calculate viewport-normalized mouse coordinates for Look-Target tracking on click-and-drag
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(_window, &mx, &my);
        int winW = 0, winH = 0;
        glfwGetWindowSize(_window, &winW, &winH);

        // 1. Compute gaze tracking Look-Target
        float dragX = 0.0f;
        float dragY = 0.0f;
        if (_isDraggingLookTarget)
        {
            float rx = static_cast<float>(mx) - _state.view.leftPanelWidth;
            float ry = static_cast<float>(my);
            float viewW = static_cast<float>(winW) - _state.view.leftPanelWidth;
            float viewH = static_cast<float>(winH) - _state.view.consoleHeight;
            if (viewW > 0.0f && viewH > 0.0f)
            {
                dragX = (rx / viewW) * 2.0f - 1.0f;
                dragY = -((ry / viewH) * 2.0f - 1.0f);
                dragX = std::clamp(dragX, -1.0f, 1.0f);
                dragY = std::clamp(dragY, -1.0f, 1.0f);
            }
        }

        for (const auto& m : _state.models)
        {
            _backend->setDragPoint(m.id, dragX, dragY);
        }

        _backend->setHideRedOption(_state.view.hideRed);

        setCrashStage("app run: backend update");
        _backend->update(_state, dt);

        int w = 0, h = 0;
        glfwGetFramebufferSize(_window, &w, &h);
        glViewport(0, 0, w, h);
        if (_state.view.transparentBackground)
        {
            glClearColor(0, 0, 0, 0);
        }
        else
        {
            glClearColor(_state.view.bg[0], _state.view.bg[1], _state.view.bg[2], _state.view.bg[3]);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Global keyboard shortcuts (Ctrl+S: save, Ctrl+O: open project, Ctrl+Z: undo, Ctrl+Y: redo)
        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
            {
                saveProject();
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
            {
                loadProject();
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
            {
                _undoManager.undo(_state, *_backend);
            }
            if (ImGui::GetIO().KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))))
            {
                _undoManager.redo(_state, *_backend);
            }
        }

        _gui.draw(_state, *_backend, *this);

        // Reserve the area not occupied by the left control panel.
        ImGuiViewport* vp = ImGui::GetMainViewport();
        const float leftPanelW = _state.view.leftPanelWidth;
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + leftPanelW, vp->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - leftPanelW, vp->WorkSize.y - _state.view.consoleHeight));
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);
        ImGui::Text("Drag .model3.json/folder here. Left-drag: pan, scroll: zoom, Right-drag: look target. Select model in left panel.");
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("viewport_canvas", canvasSize, ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonLeft);
        handleViewportInput();
        ImGui::End();

        // Compute centered aspect-ratio scissor region inside the right panel.
        const float scaleY = (float)h / vp->WorkSize.y;
        const int leftPx  = static_cast<int>(leftPanelW);
        const int bottomPx = static_cast<int>(_state.view.consoleHeight * scaleY);
        const int availW = std::max(1, w - leftPx);
        const int availH = std::max(1, h - bottomPx);

        // Letterbox: fit view.width x view.height into availW x availH
        const float targetAspect = (_state.view.height > 0)
            ? static_cast<float>(_state.view.width) / static_cast<float>(_state.view.height)
            : static_cast<float>(availW) / static_cast<float>(availH);
        int vpW, vpH, vpX, vpY;
        if (static_cast<float>(availW) / static_cast<float>(availH) > targetAspect)
        {
            vpH = availH;
            vpW = static_cast<int>(availH * targetAspect);
        }
        else
        {
            vpW = availW;
            vpH = static_cast<int>(availW / targetAspect);
        }
        vpX = leftPx + (availW - vpW) / 2;
        vpY = bottomPx + (availH - vpH) / 2;

        // Draw letterbox background (dark workspace) for the whole right area.
        glEnable(GL_SCISSOR_TEST);
        glScissor(leftPx, bottomPx, availW, availH);
        glViewport(leftPx, bottomPx, availW, availH);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw the actual scene scissored to the centered viewport rect.
        glScissor(vpX, vpY, vpW, vpH);
        glViewport(vpX, vpY, vpW, vpH);
        if (!_state.view.transparentBackground)
        {
            glClearColor(_state.view.bg[0], _state.view.bg[1], _state.view.bg[2], _state.view.bg[3]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else
        {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            drawCheckerboard(vpW, vpH);
        }
        loadBackgroundImage();
        drawBackgroundQuad(vpW, vpH);
        if (_state.view.showGrid) drawGrid(vpW, vpH);
        setCrashStage("app run: backend draw");
        _backend->draw(_state, vpW, vpH, _camera);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, w, h);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(_window);

        if (_exportDeferred)
        {
            _exportDeferred = false;
            executeExport();
        }

        double elapsed = glfwGetTime() - frameStartTime;
        double targetFrameTime = 1.0 / _state.view.targetFps;
        if (elapsed < targetFrameTime)
        {
            double sleepTime = targetFrameTime - elapsed;
            if (sleepTime > 0.002)
            {
                std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime - 0.001));
            }
        }
    }
    saveConfig();
    return 0;
}

void GuiApplication::drawGrid(int width, int height)
{
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    glOrtho(-aspect / _camera.zoom + _camera.panX, aspect / _camera.zoom + _camera.panX,
            -1.0f / _camera.zoom + _camera.panY, 1.0f / _camera.zoom + _camera.panY,
            -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (_state.view.showGrid)
    {
        glLineWidth(1.0f);
        // Soft dark gray grid for light background theme
        glColor4f(0.0f, 0.0f, 0.0f, 0.08f);
        glBegin(GL_LINES);
        for (float x = -10.0f; x <= 10.01f; x += 0.25f)
        {
            if (std::abs(x) < 0.01f && _state.view.showAxis) continue;
            glVertex2f(x, -10.0f); glVertex2f(x, 10.0f);
        }
        for (float y = -10.0f; y <= 10.01f; y += 0.25f)
        {
            if (std::abs(y) < 0.01f && _state.view.showAxis) continue;
            glVertex2f(-10.0f, y); glVertex2f(10.0f, y);
        }
        glEnd();
    }

    if (_state.view.showAxis)
    {
        glLineWidth(2.0f);
        glColor4f(0.4f, 0.4f, 0.4f, 0.40f);
        glBegin(GL_LINES);
        glVertex2f(-10.0f, 0.0f); glVertex2f(10.0f, 0.0f);
        glVertex2f(0.0f, -10.0f); glVertex2f(0.0f, 10.0f);
        glEnd();
    }
}

void GuiApplication::handleViewportInput()
{
    ImGuiIO& io = ImGui::GetIO();

    // 1. Mouse wheel zoom inside viewport
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f)
    {
        const float factor = io.MouseWheel > 0 ? 1.1f : 0.9f;
        int w = 0, h = 0;
        glfwGetFramebufferSize(_window, &w, &h);
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float scaleY = (float)h / vp->WorkSize.y;
        int bottomPx = static_cast<int>(120.0f * scaleY);
        float relativeMouseX = io.MousePos.x - _state.view.leftPanelWidth;
        _camera.zoomAt(factor, relativeMouseX, io.MousePos.y, 
                        std::max(1, w - static_cast<int>(_state.view.leftPanelWidth)), 
                        std::max(1, h - bottomPx));
    }

    // 2. Click start (Left click: Model Hit Test)
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(_window, &mx, &my);
        float px = 0.0f, py = 0.0f;
        screenToProjection(mx, my, px, py);

        uint64_t clickedModelId = 0;
        for (int i = static_cast<int>(_state.models.size()) - 1; i >= 0; --i)
        {
            ModelEntry& m = _state.models[i];
            if (m.visible && _backend->hitTest(m.id, px, py))
            {
                clickedModelId = m.id;
                break;
            }
        }

        if (clickedModelId != 0)
        {
            ModelEntry* clickedModel = _state.find(clickedModelId);
            if (clickedModel)
            {
                if (io.KeyCtrl)
                {
                    clickedModel->selected = !clickedModel->selected;
                }
                else if (!clickedModel->selected)
                {
                    _state.clearSelection();
                    clickedModel->selected = true;
                }
            }
            _isDraggingModel = true;
            _lastMouseX = mx;
            _lastMouseY = my;
        }
        else
        {
            if (!io.KeyCtrl)
            {
                _state.clearSelection();
            }
            _isDraggingModel = false;
        }
    }

    // 3. Left mouse drag
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        if (_isDraggingModel)
        {
            // Drag selected models
            double mx = 0.0, my = 0.0;
            glfwGetCursorPos(_window, &mx, &my);
            float currPx = 0.0f, currPy = 0.0f;
            float prevPx = 0.0f, prevPy = 0.0f;
            screenToProjection(mx, my, currPx, currPy);
            screenToProjection(_lastMouseX, _lastMouseY, prevPx, prevPy);

            float dProjX = currPx - prevPx;
            float dProjY = currPy - prevPy;

            for (auto* m : _state.selectedModels())
            {
                m->x += dProjX;
                m->y += dProjY;
            }
            _lastMouseX = mx;
            _lastMouseY = my;
        }
        else
        {
            // Empty space left drag: Pan viewport
            int w = 0, h = 0;
            glfwGetFramebufferSize(_window, &w, &h);
            ImGuiViewport* vp = ImGui::GetMainViewport();
            float scaleY = (float)h / vp->WorkSize.y;
            int bottomPx = static_cast<int>(_state.view.consoleHeight * scaleY);
            _camera.pan(io.MouseDelta.x, io.MouseDelta.y, 
                        std::max(1, w - static_cast<int>(_state.view.leftPanelWidth)), 
                        std::max(1, h - bottomPx));
        }
    }

    // Release left mouse button: reset dragging flag
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        _isDraggingModel = false;
    }

    // 4. Right mouse drag: Look-Target tracking
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        _isDraggingLookTarget = true;
    }
    else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        _isDraggingLookTarget = false;
    }
}

void GuiApplication::screenToProjection(double mx, double my, float& px, float& py)
{
    int winW = 0, winH = 0;
    glfwGetWindowSize(_window, &winW, &winH);
    const float leftPanelW = _state.view.leftPanelWidth;
    float viewW = static_cast<float>(winW) - leftPanelW;
    float viewH = static_cast<float>(winH) - _state.view.consoleHeight;
    if (viewW <= 0.0f || viewH <= 0.0f)
    {
        px = 0.0f; py = 0.0f;
        return;
    }

    float rx = static_cast<float>(mx) - leftPanelW;
    float ry = static_cast<float>(my);

    float aspect = viewW / viewH;
    float normX = (rx / viewW) * 2.0f - 1.0f;
    float normY = -((ry / viewH) * 2.0f - 1.0f);

    px = normX * aspect / _camera.zoom + _camera.panX;
    py = normY / _camera.zoom + _camera.panY;
}

void GuiApplication::requestImportPath(const std::filesystem::path& path)
{
    importPath(path);
}

void GuiApplication::importPath(const std::filesystem::path& path)
{
    try
    {
        auto models = ModelScanner::findModels(path, 5);
        if (models.empty())
        {
            _state.exportStatus.message = "No .model3.json found. Drop a model folder or .model3.json, not .moc3.";
            std::cerr << _state.exportStatus.message << "\n";
            return;
        }

        for (const auto& model : models)
        {
            logDiagnostic("[APP] import candidate: " + utf8Path(model));
            ModelEntry entry;
            entry.id = _state.nextId();
            entry.model3Json = std::filesystem::absolute(model).lexically_normal();
            ModelScanner::fillMetadataFromModel3Json(entry);
            entry.selected = _state.models.empty();
            const auto result = _backend->loadModel(entry);
            if (!result.ok)
            {
                _state.exportStatus.message = "Failed to load " + utf8Path(entry.model3Json) + ": " + result.error;
                std::cerr << _state.exportStatus.message << "\n";
                continue;
            }
            _state.exportStatus.message = "Loaded " + utf8Path(entry.model3Json);
            logDiagnostic("[APP] import complete: " + utf8Path(entry.model3Json));

            if (entry.playlist.empty() && !entry.motions.empty())
            {
                PlaylistItem newItem;
                int defaultIdx = 0;
                std::vector<std::string> keywords = {"idle", "wait", "loop"};
                bool found = false;
                for (const auto& kw : keywords)
                {
                    for (int m_i = 0; m_i < (int)entry.motions.size(); ++m_i)
                    {
                        std::string gLower = entry.motions[m_i].group;
                        std::transform(gLower.begin(), gLower.end(), gLower.begin(), ::tolower);
                        if (gLower.find(kw) != std::string::npos)
                        {
                            defaultIdx = m_i;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                newItem.group = entry.motions[defaultIdx].group;
                newItem.index = entry.motions[defaultIdx].index;
                newItem.fadeTime = 0.0f;
                newItem.selected = false;
                entry.playlist.push_back(newItem);
            }

            _state.models.push_back(std::move(entry));
        }
    }
    catch (const std::exception& e)
    {
        _state.exportStatus.message = std::string("Import failed: ") + e.what();
        std::cerr << _state.exportStatus.message << "\n";
    }
    catch (...)
    {
        _state.exportStatus.message = "Import failed: unknown error.";
        std::cerr << _state.exportStatus.message << "\n";
    }
}

void GuiApplication::requestExport()
{
    if (_testExportMode)
    {
        executeExport();
    }
    else
    {
        _exportDeferred = true;
    }
}

void GuiApplication::executeExport()
{
    auto selectedModels = _state.selectedModels();
    if (selectedModels.empty()) return;

    std::string timestamp = getCurrentTimestampString();
    ExportSettings settings = _state.exportSettings;
    if (settings.useCurrentViewport)
    {
        settings.width = _state.view.width;
        settings.height = _state.view.height;
    }

    // Keep main context fully alive, but temporarily disable inputs to it
    ImGuiContext* mainCtx = ImGui::GetCurrentContext();
    ImGuiStyle mainStyle = ImGui::GetStyle();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoKeyboard;

    // Get main window geometry
    int mainX = 0, mainY = 0, mainW = 0, mainH = 0;
    glfwGetWindowPos(_window, &mainX, &mainY);
    glfwGetWindowSize(_window, &mainW, &mainH);
    
    int totalModels = (settings.exportSingle || selectedModels.size() == 1) ? 1 : (int)selectedModels.size();
    int progW = 460;
    int progH = (totalModels > 1) ? 170 : 120; // Dynamically sized to avoid empty space
    
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Spawn hidden to avoid flash before positioning
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    
    GLFWwindow* progressWindow = glfwCreateWindow(progW, progH, "Export Progress", nullptr, nullptr);
    glfwDefaultWindowHints(); // Reset hints to defaults
    
    ImGuiContext* progressCtx = nullptr;
    if (progressWindow)
    {
        // Center the progress window over the main window
        int progX = mainX + (mainW - progW) / 2;
        int progY = mainY + (mainH - progH) / 2;
        glfwSetWindowPos(progressWindow, progX, progY);
        glfwShowWindow(progressWindow);

        progressCtx = ImGui::CreateContext();
        ImGui::SetCurrentContext(progressCtx);
        
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        // Load fonts
        {
            ImFontConfig cfg;
            cfg.OversampleH = 2;
            cfg.OversampleV = 2;
            static const ImWchar ranges[] = {
                0x0020, 0x00FF,
                0x2000, 0x2BFF,
                0xAC00, 0xD7A3,
                0,
            };
            ImFont* font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/malgun.ttf", 15.0f, &cfg, ranges);
            if (!font) font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 15.0f, &cfg, ranges);
            if (!font) io.Fonts->AddFontDefault();
        }
        
        ImGui::StyleColorsLight();
        ImGui::GetStyle() = mainStyle; // Copy style settings directly
        
        glfwMakeContextCurrent(progressWindow);
        ImGui_ImplGlfw_InitForOpenGL(progressWindow, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        // Restore main context immediately so FBO setup and model drawing context is correct
        glfwMakeContextCurrent(_window);
        ImGui::SetCurrentContext(mainCtx);
    }
    else
    {
        progressWindow = _window;
    }


    FrameExporterCallbacks cb;
    cb.render = [this](int width, int height, float t) {
        _backend->drawForExport(_state, width, height, _camera, t);
    };
    
    cb.progress = [this, progressWindow, progressCtx, mainCtx](float progressVal, const std::string& message) {
        glfwPollEvents();

        if (progressWindow != _window)
        {
            if (glfwWindowShouldClose(progressWindow))
            {
                _state.exportStatus.cancelRequested = true;
            }
            ImGui::SetCurrentContext(progressCtx);
            glfwMakeContextCurrent(progressWindow);
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int w, h;
        glfwGetWindowSize(progressWindow, &w, &h);
        
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        if (ImGui::Begin("ProgressContent", nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoCollapse | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (_state.exportStatus.totalModels > 1)
            {
                float overallProgress = static_cast<float>(_state.exportStatus.currentModelIdx) / _state.exportStatus.totalModels;
                overallProgress += progressVal / _state.exportStatus.totalModels;
                
                ImGui::Text("Overall Export Progress: Model %d of %d", _state.exportStatus.currentModelIdx + 1, _state.exportStatus.totalModels);
                ImGui::ProgressBar(overallProgress, ImVec2(-1.0f, 18.0f));
            }

            ImGui::Text("Current Export: %s", message.c_str());
            int curFrame = _state.exportStatus.currentFrame;
            int totFrames = _state.exportStatus.totalFrames;
            float frameProgress = (totFrames > 0) ? (static_cast<float>(curFrame) / totFrames) : 0.0f;
            
            char frameBuf[64];
            std::snprintf(frameBuf, sizeof(frameBuf), "Frame %d / %d", curFrame, totFrames);
            ImGui::ProgressBar(frameProgress, ImVec2(-1.0f, 18.0f), frameBuf);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float availWidth = ImGui::GetContentRegionAvail().x;
            if (_state.exportStatus.totalModels > 1)
            {
                float btnW = (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                if (ImGui::Button("Skip Current Model", ImVec2(btnW, 30.0f)))
                {
                    _state.exportStatus.skipRequested = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel All", ImVec2(btnW, 30.0f)))
                {
                    _state.exportStatus.cancelRequested = true;
                }
            }
            else
            {
                if (ImGui::Button("Cancel", ImVec2(-1.0f, 30.0f)))
                {
                    _state.exportStatus.cancelRequested = true;
                }
            }

            ImGui::End();
        }
        ImGui::PopStyleVar(2);

        ImGui::Render();

        int fw, fh;
        glfwGetFramebufferSize(progressWindow, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(0.96f, 0.96f, 0.97f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(progressWindow);

        if (progressWindow != _window)
        {
            glfwMakeContextCurrent(_window);
            ImGui::SetCurrentContext(mainCtx);
        }
    };
    
    cb.getMotionDuration = [this]() -> float {
        for (const auto& m : _state.models)
        {
            if (m.visible) return _backend->getMotionDuration(m.id);
        }
        return 0.0f;
    };

    _state.exportStatus.cancelRequested = false;
    _state.exportStatus.skipRequested = false;

    try
    {
        if (settings.exportSingle || selectedModels.size() == 1)
        {
            _state.exportStatus.totalModels = 1;
            _state.exportStatus.currentModelIdx = 0;
            _state.exportStatus.cancelRequested = false;
            _state.exportStatus.skipRequested = false;

            std::string baseName = (selectedModels.size() == 1) ? (selectedModels[0]->name + "_" + timestamp) : ("composite_" + timestamp);
            std::string ext = "";
            if (settings.exportType == EXPORT_SINGLE_FRAME)
            {
                ext = (settings.imageFormat == 0) ? ".jpg" : ((settings.imageFormat == 2) ? ".webp" : ".png");
                settings.outputPath = settings.outputFolder / (baseName + ext);
            }
            else if (settings.exportType == EXPORT_FRAME_SEQUENCE)
            {
                ext = (settings.imageFormat == 0) ? ".jpg" : ((settings.imageFormat == 2) ? ".webp" : ".png");
                std::filesystem::path seqFolder = settings.outputFolder / baseName;
                std::error_code ec;
                std::filesystem::create_directories(seqFolder, ec);
                settings.outputPath = seqFolder / ("frame_%04d" + ext);
            }
            else
            {
                std::string vext = ".mp4";
                if (settings.videoFormat == 0) vext = ".gif";
                else if (settings.videoFormat == 1) vext = ".webp";
                else if (settings.videoFormat == 2) vext = ".png";
                else if (settings.videoFormat == 4) vext = ".webm";
                else if (settings.videoFormat == 5) vext = ".mkv";
                else if (settings.videoFormat == 6) vext = ".mov";
                settings.outputPath = settings.outputFolder / (baseName + vext);
            }

            for (auto& m : _state.models)
            {
                if (m.visible)
                {
                    _backend->prepareForExport(m.id);
                }
            }

            if (settings.exportType == EXPORT_SINGLE_FRAME)
            {
                _exporter.exportSingleFrame(settings, cb, &_state.exportStatus);
            }
            else if (settings.exportType == EXPORT_FRAME_SEQUENCE)
            {
                _exporter.exportFrameSequence(settings, cb, &_state.exportStatus);
            }
            else
            {
                _exporter.exportVideo(settings, cb, &_state.exportStatus);
            }
        }
        else
        {
            struct ModelBackup {
                uint64_t id;
                bool visible;
                bool selected;
            };
            std::vector<ModelBackup> backups;
            for (const auto& m : _state.models)
            {
                backups.push_back({m.id, m.visible, m.selected});
            }

            _state.exportStatus.totalModels = selectedModels.size();

            for (size_t idx = 0; idx < selectedModels.size(); ++idx)
            {
                if (_state.exportStatus.cancelRequested)
                {
                    break;
                }

                _state.exportStatus.currentModelIdx = idx;
                _state.exportStatus.skipRequested = false;

                ModelEntry* activeModel = selectedModels[idx];
                for (auto& m : _state.models)
                {
                    m.selected = (m.id == activeModel->id);
                    m.visible = (m.id == activeModel->id);
                }

                std::string baseName = activeModel->name + "_" + timestamp;
                std::string ext = "";
                if (settings.exportType == EXPORT_SINGLE_FRAME)
                {
                    ext = (settings.imageFormat == 0) ? ".jpg" : ((settings.imageFormat == 2) ? ".webp" : ".png");
                    settings.outputPath = settings.outputFolder / (baseName + ext);
                }
                else if (settings.exportType == EXPORT_FRAME_SEQUENCE)
                {
                    ext = (settings.imageFormat == 0) ? ".jpg" : ((settings.imageFormat == 2) ? ".webp" : ".png");
                    std::filesystem::path seqFolder = settings.outputFolder / baseName;
                    std::error_code ec;
                    std::filesystem::create_directories(seqFolder, ec);
                    settings.outputPath = seqFolder / ("frame_%04d" + ext);
                }
                else
                {
                    std::string vext = ".mp4";
                    if (settings.videoFormat == 0) vext = ".gif";
                    else if (settings.videoFormat == 1) vext = ".webp";
                    else if (settings.videoFormat == 2) vext = ".png";
                    else if (settings.videoFormat == 4) vext = ".webm";
                    else if (settings.videoFormat == 5) vext = ".mkv";
                    else if (settings.videoFormat == 6) vext = ".mov";
                    settings.outputPath = settings.outputFolder / (baseName + vext);
                }

                std::string msg = "Exporting model " + std::to_string(idx + 1) + " of " + std::to_string(selectedModels.size()) + ": " + activeModel->name;
                _state.exportStatus.message = msg;

                _backend->prepareForExport(activeModel->id);

                bool success = false;
                if (settings.exportType == EXPORT_SINGLE_FRAME)
                {
                    success = _exporter.exportSingleFrame(settings, cb, &_state.exportStatus);
                }
                else if (settings.exportType == EXPORT_FRAME_SEQUENCE)
                {
                    success = _exporter.exportFrameSequence(settings, cb, &_state.exportStatus);
                }
                else
                {
                    success = _exporter.exportVideo(settings, cb, &_state.exportStatus);
                }

                if (!success && !_state.exportStatus.skipRequested)
                {
                    break;
                }
            }

            for (const auto& b : backups)
            {
                ModelEntry* m = _state.find(b.id);
                if (m)
                {
                    m->visible = b.visible;
                    m->selected = b.selected;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        _state.exportStatus.running = false;
        _state.exportStatus.message = e.what();
    }

    // --- CLEANUP PROGRESS WINDOW & RE-INITIALIZE MAIN IMGUI BINDINGS ---
    if (progressWindow != _window)
    {
        glfwMakeContextCurrent(progressWindow);
        ImGui::SetCurrentContext(progressCtx); // Set progress context current first so we shut down its bindings, not main window's!
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(progressCtx);
        glfwDestroyWindow(progressWindow);
    }

    // Restore main context and input flags
    glfwMakeContextCurrent(_window);
    ImGui::SetCurrentContext(mainCtx);
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard;
}

void GuiApplication::glfwDropCallback(GLFWwindow* window, int count, const char** paths)
{
    auto* self = static_cast<GuiApplication*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    for (int i = 0; i < count; ++i)
    {
        self->requestImportPath(pathFromUtf8(paths[i]));
    }
}

void GuiApplication::glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

    auto* self = static_cast<GuiApplication*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    const float factor = yoffset > 0 ? 1.1f : 0.9f;

    if (io.KeyCtrl)
    {
        for (auto* m : self->_state.selectedModels())
        {
            m->scale = std::clamp(m->scale * factor, 0.01f, 20.0f);
        }
    }
}

void GuiApplication::saveConfig()
{
    std::ofstream f("config.ini");
    if (!f.is_open()) return;

    f << "[Viewport]\n";
    f << "width = " << _state.view.width << "\n";
    f << "height = " << _state.view.height << "\n";
    f << "zoom = " << _camera.zoom << "\n";
    f << "panX = " << _camera.panX << "\n";
    f << "panY = " << _camera.panY << "\n";
    f << "rotationDeg = " << _state.view.rotationDeg << "\n";
    f << "flipX = " << (_state.view.flipX ? "true" : "false") << "\n";
    f << "flipY = " << (_state.view.flipY ? "true" : "false") << "\n";
    f << "playbackSpeed = " << _state.view.playbackSpeed << "\n";
    f << "showAxis = " << (_state.view.showAxis ? "true" : "false") << "\n";
    f << "transparentBackground = " << (_state.view.transparentBackground ? "true" : "false") << "\n";
    f << "bgColor = " << _state.view.bg[0] << "," << _state.view.bg[1] << "," << _state.view.bg[2] << "," << _state.view.bg[3] << "\n";
    f << "bgImagePath = " << utf8Path(_state.view.bgImagePath) << "\n";
    f << "bgImageMode = " << _state.view.bgImageMode << "\n";
    f << "onlySelected = " << (_state.view.onlySelected ? "true" : "false") << "\n";
    f << "showGrid = " << (_state.view.showGrid ? "true" : "false") << "\n";
    f << "debugBounds = " << (_state.view.debugBounds ? "true" : "false") << "\n";
    f << "hideRed = " << (_state.view.hideRed ? "true" : "false") << "\n";
    f << "showWireframe = " << (_state.view.showWireframe ? "true" : "false") << "\n";
    f << "showHitAreas = " << (_state.view.showHitAreas ? "true" : "false") << "\n";
    f << "leftPanelWidth = " << _state.view.leftPanelWidth << "\n";
    f << "consoleHeight = " << _state.view.consoleHeight << "\n";
    f << "targetFps = " << _state.view.targetFps << "\n";

    f << "\n[Browser]\n";
    f << "browserDir = " << utf8Path(_state.browserDir) << "\n";

    f << "\n[Export]\n";
    f << "fps = " << _state.exportSettings.fps << "\n";
}

void GuiApplication::loadConfig()
{
    std::ifstream f("config.ini");
    if (!f.is_open()) return;

    std::string line;
    std::string section;
    while (std::getline(f, line))
    {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        auto last_not_space = line.find_last_not_of(" \t\r\n");
        if (last_not_space != std::string::npos)
        {
            line.erase(last_not_space + 1);
        }
        else
        {
            line.clear();
        }

        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[' && line.back() == ']')
        {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        key.erase(0, key.find_first_not_of(" \t"));
        auto last_k = key.find_last_not_of(" \t");
        if (last_k != std::string::npos) key.erase(last_k + 1); else key.clear();

        std::string val = line.substr(eq + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        auto last_v = val.find_last_not_of(" \t");
        if (last_v != std::string::npos) val.erase(last_v + 1); else val.clear();

        if (section == "Viewport")
        {
            if (key == "width") _state.view.width = std::stoi(val);
            else if (key == "height") _state.view.height = std::stoi(val);
            else if (key == "zoom") _camera.zoom = std::stof(val);
            else if (key == "panX") _camera.panX = std::stof(val);
            else if (key == "panY") _camera.panY = std::stof(val);
            else if (key == "rotationDeg") _state.view.rotationDeg = std::stof(val);
            else if (key == "flipX") _state.view.flipX = (val == "true" || val == "1");
            else if (key == "flipY") _state.view.flipY = (val == "true" || val == "1");
            else if (key == "playbackSpeed") _state.view.playbackSpeed = std::stof(val);
            else if (key == "showAxis") _state.view.showAxis = (val == "true" || val == "1");
            else if (key == "transparentBackground") _state.view.transparentBackground = (val == "true" || val == "1");
            else if (key == "bgColor")
            {
                std::stringstream ss(val);
                std::string item;
                int i = 0;
                while (std::getline(ss, item, ',') && i < 4)
                {
                    _state.view.bg[i++] = std::stof(item);
                }
            }
            else if (key == "bgImagePath") _state.view.bgImagePath = val;
            else if (key == "bgImageMode") _state.view.bgImageMode = std::stoi(val);
            else if (key == "onlySelected") _state.view.onlySelected = (val == "true" || val == "1");
            else if (key == "showGrid") _state.view.showGrid = (val == "true" || val == "1");
            else if (key == "debugBounds") _state.view.debugBounds = (val == "true" || val == "1");
            else if (key == "hideRed") _state.view.hideRed = (val == "true" || val == "1");
            else if (key == "showWireframe") _state.view.showWireframe = (val == "true" || val == "1");
            else if (key == "showHitAreas") _state.view.showHitAreas = (val == "true" || val == "1");
            else if (key == "leftPanelWidth") _state.view.leftPanelWidth = std::stof(val);
            else if (key == "consoleHeight") _state.view.consoleHeight = std::stof(val);
            else if (key == "targetFps") _state.view.targetFps = std::stoi(val);
        }
        else if (section == "Browser")
        {
            if (key == "browserDir")
            {
                std::filesystem::path p = pathFromUtf8(val);
                if (std::filesystem::exists(p) && std::filesystem::is_directory(p))
                {
                    _state.browserDir = p;
                }
            }
        }
        else if (section == "Export")
        {
            if (key == "fps")
            {
                _state.exportSettings.fps = std::stoi(val);
            }
        }
    }
}

void GuiApplication::saveProject()
{
    auto path = NativeDialogs::saveFile(L"project", L"json");
    if (!path) return;
    _currentProjectPath = *path;

    if (ProjectSerializer::save(_state, _currentProjectPath))
    {
        ConsoleLog::getInstance().addLog(LOG_INFO, "[Project] Saved: " + utf8Path(_currentProjectPath));
    }
    else
    {
        ConsoleLog::getInstance().addLog(LOG_ERROR, "[Project] Failed to save: " + utf8Path(_currentProjectPath));
    }
}

void GuiApplication::loadProject()
{
    auto path = NativeDialogs::pickFile(L"Live2D Project (*.json)\0*.json\0");
    if (!path) return;

    std::string error;
    if (ProjectSerializer::load(_state, *_backend, *path, &error))
    {
        _currentProjectPath = *path;
        _loadedBgPath.clear(); // force reload background texture
        ConsoleLog::getInstance().addLog(LOG_INFO, "[Project] Loaded: " + utf8Path(*path));
        _undoManager.clear();
        pushUndoState("Load Project");
    }
    else
    {
        ConsoleLog::getInstance().addLog(LOG_ERROR, "[Project] Failed to load: " + utf8Path(*path) + " (" + error + ")");
    }
}

void GuiApplication::pushUndoState(const std::string& description)
{
    _undoManager.pushState(_state, description);
}

void GuiApplication::loadBackgroundImage()
{
    const std::string& path = _state.view.bgImagePath;
    if (path == _loadedBgPath) return;
    _loadedBgPath = path;

    // Delete old texture
    if (_bgTexture)
    {
        glDeleteTextures(1, &_bgTexture);
        _bgTexture = 0;
        _bgTexW = _bgTexH = 0;
    }

    if (path.empty()) return;

    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) return;

    glGenTextures(1, &_bgTexture);
    glBindTexture(GL_TEXTURE_2D, _bgTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    _bgTexW = w;
    _bgTexH = h;
}

void GuiApplication::drawCheckerboard(int vpW, int vpH)
{
    // Lazy-init 2x2 checker texture
    if (!_checkerTex)
    {
        unsigned char pixels[4 * 4] = {
            200, 200, 200, 255,   255, 255, 255, 255,
            255, 255, 255, 255,   200, 200, 200, 255
        };
        glGenTextures(1, &_checkerTex);
        glBindTexture(GL_TEXTURE_2D, _checkerTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Save GL state
    glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT | GL_CURRENT_BIT);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, vpW, 0, vpH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBindTexture(GL_TEXTURE_2D, _checkerTex);
    float cellSize = 8.0f;
    float tcX = vpW / (2.0f * cellSize);
    float tcY = vpH / (2.0f * cellSize);

    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);    glVertex2f(0, 0);
    glTexCoord2f(tcX, 0);  glVertex2f((float)vpW, 0);
    glTexCoord2f(tcX, tcY); glVertex2f((float)vpW, (float)vpH);
    glTexCoord2f(0, tcY);  glVertex2f(0, (float)vpH);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

void GuiApplication::drawBackgroundQuad(int vpW, int vpH)
{
    if (!_bgTexture || _state.view.bgImageMode == 0) return;

    float drawX = 0, drawY = 0, drawW = (float)vpW, drawH = (float)vpH;
    float imgAspect = (float)_bgTexW / (float)_bgTexH;
    float vpAspect = (float)vpW / (float)vpH;

    // Compute draw rect based on mode
    switch (_state.view.bgImageMode)
    {
    case 1: // Fill: cover viewport, center and crop
        if (vpAspect > imgAspect)
        {
            drawW = (float)vpW;
            drawH = drawW / imgAspect;
        }
        else
        {
            drawH = (float)vpH;
            drawW = drawH * imgAspect;
        }
        drawX = ((float)vpW - drawW) * 0.5f;
        drawY = ((float)vpH - drawH) * 0.5f;
        break;
    case 2: // Uniform/Fit: fit inside viewport
        if (vpAspect > imgAspect)
        {
            drawH = (float)vpH;
            drawW = drawH * imgAspect;
        }
        else
        {
            drawW = (float)vpW;
            drawH = drawW / imgAspect;
        }
        drawX = ((float)vpW - drawW) * 0.5f;
        drawY = ((float)vpH - drawH) * 0.5f;
        break;
    case 3: // UniformToFill/Stretch
        drawX = 0; drawY = 0;
        drawW = (float)vpW; drawH = (float)vpH;
        break;
    }

    glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT | GL_CURRENT_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, vpW, 0, vpH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBindTexture(GL_TEXTURE_2D, _bgTexture);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(drawX, drawY);
    glTexCoord2f(1, 1); glVertex2f(drawX + drawW, drawY);
    glTexCoord2f(1, 0); glVertex2f(drawX + drawW, drawY + drawH);
    glTexCoord2f(0, 0); glVertex2f(drawX, drawY + drawH);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

} // namespace l2dgui
