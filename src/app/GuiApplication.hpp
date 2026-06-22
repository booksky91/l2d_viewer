#pragma once

#include "app/AppState.hpp"
#include "app/UndoManager.hpp"
#include "backends/ILive2DBackend.hpp"
#include "export/FrameExporter.hpp"
#include "gui/MainGui.hpp"
#include "render/ViewportCamera.hpp"
#include <memory>
#include <string>

struct GLFWwindow;

namespace l2dgui
{

class GuiApplication
{
public:
    GuiApplication();
    ~GuiApplication();

    bool initialize(int argc, char** argv);
    int run();
    void requestImportPath(const std::filesystem::path& path);
    void requestExport();
    void saveProject();
    void loadProject();
    void pushUndoState(const std::string& description);
    UndoManager& getUndoManager() { return _undoManager; }
    ViewportCamera& getCamera() { return _camera; }

private:
    static void glfwDropCallback(GLFWwindow* window, int count, const char** paths);
    static void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    void importPath(const std::filesystem::path& path);
    void renderViewport(int width, int height);
    void drawGrid(int width, int height);
    void handleViewportInput();
    float deltaTime();
    void saveConfig();
    void loadConfig();

    GLFWwindow* _window = nullptr;
    std::unique_ptr<ILive2DBackend> _backend;
    AppState _state;
    ViewportCamera _camera;
    MainGui _gui;
    FrameExporter _exporter;
    double _lastTime = 0.0;
    bool _initialized = false;
    bool _testExportMode = false;
    bool _isDraggingLookTarget = false;
    bool _isDraggingModel = false;
    void executeExport();
    bool _exportDeferred = false;
    double _lastMouseX = 0.0;
    double _lastMouseY = 0.0;
    void screenToProjection(double mx, double my, float& px, float& py);

    // Project file
    std::filesystem::path _currentProjectPath;

    // Undo/Redo
    UndoManager _undoManager;

    // Background rendering
    unsigned int _bgTexture = 0;
    int _bgTexW = 0, _bgTexH = 0;
    std::string _loadedBgPath;
    unsigned int _checkerTex = 0;
    void loadBackgroundImage();
    void drawBackgroundQuad(int vpW, int vpH);
    void drawCheckerboard(int vpW, int vpH);
};

} // namespace l2dgui
