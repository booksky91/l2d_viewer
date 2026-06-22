#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "LAppAllocator_Common.hpp"
#include <CubismFramework.hpp>

class LAppView;
class LAppTextureManager;

class LAppDelegate
{
public:
    static LAppDelegate* GetInstance();
    static void ReleaseInstance();

    bool Initialize();
    void Release();
    void Run();

    void OnMouseCallBack(GLFWwindow* window, int button, int action, int modify);
    void OnMouseCallBack(GLFWwindow* window, double x, double y);

    GLFWwindow* GetWindow() { return _window; }
    LAppView* GetView() { return _view; }
    bool GetIsEnd() { return _isEnd; }
    void AppEnd() { _isEnd = true; }

    LAppTextureManager* GetTextureManager() { return _textureManager; }
    int GetWindowWidth() { return _windowWidth; }
    int GetWindowHeight() { return _windowHeight; }

    // custom helper to size the offscreen framebuffers
    void SetExternalContextSize(int width, int height)
    {
        _windowWidth = width > 0 ? width : 1;
        _windowHeight = height > 0 ? height : 1;
    }

private:
    LAppDelegate();
    ~LAppDelegate();

    // Aligned precisely to the official SDK LAppDelegate memory layout to avoid offsets mismatches
    LAppAllocator_Common _cubismAllocator;
    Csm::CubismFramework::Option _cubismOption;
    GLFWwindow* _window;
    LAppView* _view;
    bool _captured;
    float _mouseX;
    float _mouseY;
    bool _isEnd;
    LAppTextureManager* _textureManager;

    int _windowWidth;
    int _windowHeight;

    static LAppDelegate* s_instance;
};

class EventHandler
{
public:
    static void OnMouseCallBack(GLFWwindow* window, int button, int action, int modify)
    {
        LAppDelegate::GetInstance()->OnMouseCallBack(window, button, action, modify);
    }
    static void OnMouseCallBack(GLFWwindow* window, double x, double y)
    {
         LAppDelegate::GetInstance()->OnMouseCallBack(window, x, y);
    }
};
