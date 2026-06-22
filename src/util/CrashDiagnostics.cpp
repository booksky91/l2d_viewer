#include "util/CrashDiagnostics.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace l2dgui
{
namespace
{
std::mutex g_logMutex;
std::string g_stage = "startup";

std::filesystem::path diagnosticLogPath()
{
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) cwd = std::filesystem::temp_directory_path(ec);
    if (ec) cwd = ".";
    return cwd / "l2d_viewer_crash.log";
}

std::string nowString()
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
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

#if defined(_WIN32)
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info)
{
    unsigned long code = 0;
    const void* address = nullptr;
    if (info && info->ExceptionRecord)
    {
        code = static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode);
        address = info->ExceptionRecord->ExceptionAddress;
    }
    logDiagnostic(formatNativeException(code, address, "Unhandled native exception"));
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif
} // namespace

void installCrashDiagnostics()
{
#if defined(_WIN32)
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
    logDiagnostic("[DIAG] crash diagnostics installed. log=" + diagnosticLogPath().string());
}

void setCrashStage(const char* stage)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_stage = stage ? stage : "(null)";
}

const char* getCrashStage()
{
    return g_stage.c_str();
}

void logDiagnostic(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    const auto line = nowString() + " " + message;
    std::cerr << line << std::endl;

    std::ofstream out(diagnosticLogPath(), std::ios::app | std::ios::binary);
    if (out)
    {
        out << line << "\n";
    }
}

std::string formatNativeException(unsigned long code, const void* address, const char* operation)
{
    std::ostringstream oss;
    oss << "[NATIVE-CRASH] operation=" << (operation ? operation : "unknown")
        << " stage=" << getCrashStage()
        << " code=0x" << std::hex << std::uppercase << code
        << " address=0x" << reinterpret_cast<std::uintptr_t>(address);
    if (code == 0xC0000005UL)
    {
        oss << " (ACCESS_VIOLATION)";
    }
    return oss.str();
}

} // namespace l2dgui
