#pragma once

#include "app/AppState.hpp"
#include <filesystem>

namespace l2dgui
{
class GuiApplication;
class ILive2DBackend;

class MainGui
{
public:
    enum ActivePanel
    {
        PANEL_MODELS = 0,
        PANEL_VIEWPORT_SETTINGS,
        PANEL_FILE_BROWSER
    };

    enum FocusedTable
    {
        FOCUS_NONE = 0,
        FOCUS_MODELS,
        FOCUS_PLAYLIST
    };

    enum SubTab
    {
        TAB_BASIC = 0,
        TAB_RENDER,
        TAB_TRANSFORM,
        TAB_EXPRESSION,
        TAB_PARTS,
        TAB_MOTION,
        TAB_DEBUG
    };

    void draw(AppState& state, ILive2DBackend& backend, GuiApplication& app);

private:
    void drawIconSidebar(GuiApplication& app);
    void drawModelListPanel(AppState& state, ILive2DBackend& backend, GuiApplication& app);
    void drawViewportSettingsPanel(AppState& state, GuiApplication& app);
    void drawFileBrowserPanel(AppState& state, GuiApplication& app);

    void drawTabBasic(ModelEntry& model, GuiApplication& app);
    void drawTabRender(AppState& state, ModelEntry& model, ILive2DBackend& backend, GuiApplication& app);
    void drawTabTransform(AppState& state, ModelEntry& model, GuiApplication& app);
    void drawTabExpression(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app);
    void drawTabParts(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app);
    void drawTabMotion(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app);
    void drawTabDebug(AppState& state, ModelEntry& model, ILive2DBackend& backend, GuiApplication& app);

    void drawModelPanel(AppState& state, ILive2DBackend& backend, GuiApplication& app);
    void drawBrowserPanel(AppState& state, GuiApplication& app);
    void drawStagePanel(AppState& state);
    void drawExportPanel(AppState& state, GuiApplication& app, ILive2DBackend& backend);
    void drawMotionControls(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app);
    void drawModelTransform(ModelEntry& model, GuiApplication& app);
    void drawPartsAndParameters(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app);
    void drawPlaybackBar(AppState& state, ILive2DBackend& backend);

    ActivePanel _activePanel = PANEL_MODELS;
    SubTab _currentTab = TAB_BASIC;
    FocusedTable _lastFocusedTable = FOCUS_NONE;

    // Deferred export popup flags: set inside nested menus, opened at root level
    bool _openExportFrame = false;
    bool _openExportSequence = false;
    bool _openExportVideo = false;

    char _pathBuffer[2048] = {};
    char _browserBuffer[2048] = {};
    char _outBuffer[2048] = "output_alpha.webm";
    char _ffmpegBuffer[2048] = "ffmpeg";
    char _extraFfmpeg[2048] = {};
};

} // namespace l2dgui
