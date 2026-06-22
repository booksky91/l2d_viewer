#include "util/PathUtil.hpp"
#include <chrono>
#include <cstdlib>
#include <random>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace fs = std::filesystem;

namespace l2dgui
{

std::string utf8Path(const fs::path& path)
{
    return path.u8string();
}

// On Windows, GLFW drag-and-drop supplies paths in the system ANSI codepage
// (e.g. CP949 for Korean locale).  Convert CP_ACP -> UTF-16 -> UTF-8 first.
// If the string is already valid UTF-8 (pure ASCII or a UTF-8 source), the
// round-trip is transparent because CP_ACP ASCII bytes map to the same values.
fs::path pathFromUtf8(const char* utf8)
{
    if (!utf8) return {};
#ifdef _WIN32
    // Try interpreting as ANSI first (covers Korean filenames from GLFW drops)
    int wlen = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
    if (wlen > 0)
    {
        std::wstring wide(wlen, L'\0');
        MultiByteToWideChar(CP_ACP, 0, utf8, -1, wide.data(), wlen);
        if (!wide.empty() && wide.back() == L'\0')
        {
            wide.pop_back();
        }
        return fs::path(wide);
    }
    // Fallback: treat as UTF-8
    return fs::u8path(utf8);
#else
    return fs::u8path(utf8);
#endif
}

fs::path pathFromUtf8(const std::string& utf8)
{
    return pathFromUtf8(utf8.c_str());
}

fs::path normalizePath(const fs::path& path)
{
    std::error_code ec;
    return fs::absolute(path, ec).lexically_normal();
}

fs::path makeTempDirectory(const std::string& prefix)
{
    const auto base = fs::temp_directory_path();
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<uint64_t>(now));
    for (int i = 0; i < 128; ++i)
    {
        auto candidate = base / (prefix + "_" + std::to_string(now) + "_" + std::to_string(rng()));
        std::error_code ec;
        if (fs::create_directories(candidate, ec)) return candidate;
    }
    return base / prefix;
}

std::wstring utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) return L"";
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (wlen > 0)
    {
        std::wstring wide(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), wlen);
        return wide;
    }
#endif
    return std::wstring(utf8.begin(), utf8.end());
}

bool containsNonAscii(const std::string& s)
{
    for (unsigned char c : s)
    {
        if (c >= 0x80) return true;
    }
    return false;
}

} // namespace l2dgui
