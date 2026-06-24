#include "LAppPal.hpp"
#include <windows.h>
#include <cstdio>
#include <stdarg.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <Model/CubismMoc.hpp>
#include "LAppDefine.hpp"

using std::endl;
using namespace Csm;
using namespace std;
using namespace LAppDefine;

double LAppPal::s_currentFrame = 0.0;
double LAppPal::s_lastFrame = 0.0;
double LAppPal::s_deltaTime = 0.0;
#ifdef CSM_FIXED_FRAME_RATE
int LAppPal::s_frame = 0;
#endif

csmByte* LAppPal::LoadFileAsBytes(const string filePath, csmSizeInt* outSize)
{
    wchar_t wideStr[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0U, filePath.c_str(), -1, wideStr, MAX_PATH);

    // wfstream 대신 ifstream을 사용하여 바이너리를 안전하고 빠르게 한 번에 읽어옵니다.
    // Windows MSVC std::ifstream은 wchar_t* 경로 오픈을 직접 지원합니다.
    std::ifstream file(wideStr, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        if (DebugLogEnable)
        {
            PrintLogLn("File open failed. path:%s", filePath.c_str());
        }
        return NULL;
    }

    std::streamsize size = file.tellg();
    if (size <= 0)
    {
        if (DebugLogEnable)
        {
            PrintLogLn("File size is zero or invalid. path:%s", filePath.c_str());
        }
        return NULL;
    }

    file.seekg(0, std::ios::beg);
    csmChar* buf = new char[static_cast<size_t>(size)];
    if (!file.read(buf, size))
    {
        if (DebugLogEnable)
        {
            PrintLogLn("File read failed. path:%s", filePath.c_str());
        }
        delete[] buf;
        return NULL;
    }

    *outSize = static_cast<csmSizeInt>(size);
    return reinterpret_cast<csmByte*>(buf);
}

void LAppPal::ReleaseBytes(csmByte* byteData)
{
    delete[] byteData;
}

static int g_overrideCount = 0;
static double g_overrideDeltaTime = -1.0;
static double g_overrideCurrentTime = -1.0;
static bool g_isExporting = false;

extern "C" {
void LAppPal_SetOverrideTime(double currentTime, double deltaTime)
{
    g_overrideCurrentTime = currentTime;
    g_overrideDeltaTime = deltaTime;
    g_overrideCount++;
}

void LAppPal_ClearOverrideTime()
{
    g_overrideCount--;
    if (g_overrideCount < 0) g_overrideCount = 0;
}

void LAppPal_SetExporting(bool exporting)
{
    g_isExporting = exporting;
}

bool LAppPal_IsOverridden()
{
    return g_overrideCount > 0;
}

bool LAppPal_IsExporting()
{
    return g_isExporting;
}
}

csmFloat32 LAppPal::GetDeltaTime()
{
    return static_cast<csmFloat32>(s_deltaTime);
}

void LAppPal::UpdateTime()
{
    if (g_overrideCount > 0 || g_isExporting)
    {
        s_currentFrame = g_overrideCurrentTime;
        s_deltaTime = g_overrideDeltaTime;
        s_lastFrame = s_currentFrame - s_deltaTime;
        return;
    }

#ifndef CSM_FIXED_FRAME_RATE
    double now = glfwGetTime();
    double dt = now - s_lastFrame;
    // 1ms 이내의 중복 호출 시에는 이전 프레임의 deltaTime 캐시 유지
    if (dt > 0.001)
    {
        s_currentFrame = now;
        s_deltaTime = dt;
        s_lastFrame = now;
    }
#else
    s_frame += 1;
    s_currentFrame = static_cast<double>(s_frame) / CSM_FIXED_FRAME_RATE;
    s_deltaTime = s_currentFrame - s_lastFrame;
    s_lastFrame = s_currentFrame;
#endif
}

void LAppPal::PrintLog(const csmChar* format, ...)
{
    va_list args;
    csmChar buf[256];
    va_start(args, format);
    vsnprintf_s(buf, sizeof(buf), format, args);
#ifdef CSM_DEBUG_MEMORY_LEAKING
    std::printf("%s", buf);
#else
    std::cout << buf;
#endif
    va_end(args);
}

void LAppPal::PrintLogLn(const Csm::csmChar* format, ...)
{
    va_list args;
    csmChar buf[256];
    va_start(args, format);
    vsnprintf_s(buf, sizeof(buf), format, args);
#ifdef CSM_DEBUG_MEMORY_LEAKING
    std::printf("%s\n", buf);
#else
    std::cout << buf << std::endl;
#endif
    va_end(args);
}

void LAppPal::PrintMessage(const csmChar* message)
{
    PrintLog("%s", message);
}

void LAppPal::PrintMessageLn(const csmChar* message)
{
    PrintLogLn("%s", message);
}

bool LAppPal::ConvertMultiByteToWide(const csmChar* multiByte, wchar_t* wide, int wideSize)
{
    return MultiByteToWideChar(CP_UTF8, 0U, multiByte, -1, wide, wideSize) != 0;
}

bool LAppPal::ConvertWideToMultiByte(const wchar_t* wide, csmChar* multiByte, int multiByteSize)
{
    return WideCharToMultiByte(CP_UTF8, 0U, wide, -1, multiByte, multiByteSize, NULL, NULL) != 0;
}
