#include "LAppDelegate.hpp"
#include "LAppTextureManager.hpp"

LAppDelegate* LAppDelegate::s_instance = nullptr;

LAppDelegate* LAppDelegate::GetInstance()
{
    if (!s_instance) s_instance = new LAppDelegate();
    return s_instance;
}

void LAppDelegate::ReleaseInstance()
{
    if (s_instance)
    {
        s_instance->Release();
        delete s_instance;
        s_instance = nullptr;
    }
}

LAppDelegate::LAppDelegate()
    : _cubismAllocator()
    , _cubismOption()
    , _window(nullptr)
    , _view(nullptr)
    , _captured(false)
    , _mouseX(0.0f)
    , _mouseY(0.0f)
    , _isEnd(false)
    , _textureManager(nullptr)
    , _windowWidth(2048)
    , _windowHeight(2048)
{
}

LAppDelegate::~LAppDelegate()
{
    Release();
}

bool LAppDelegate::Initialize()
{
    if (!_textureManager) _textureManager = new LAppTextureManager();
    return _textureManager != nullptr;
}

void LAppDelegate::Release()
{
    delete _textureManager;
    _textureManager = nullptr;
}

void LAppDelegate::Run() {}

void LAppDelegate::OnMouseCallBack(GLFWwindow*, int, int, int) {}
void LAppDelegate::OnMouseCallBack(GLFWwindow*, double, double) {}
