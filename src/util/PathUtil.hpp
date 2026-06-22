#pragma once

#include <filesystem>
#include <string>

namespace l2dgui
{

std::string utf8Path(const std::filesystem::path& path);
std::filesystem::path pathFromUtf8(const char* utf8);
std::filesystem::path pathFromUtf8(const std::string& utf8);
std::filesystem::path normalizePath(const std::filesystem::path& path);
std::filesystem::path makeTempDirectory(const std::string& prefix);
std::wstring utf8ToWide(const std::string& utf8);
bool containsNonAscii(const std::string& s);

} // namespace l2dgui
