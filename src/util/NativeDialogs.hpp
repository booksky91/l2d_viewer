#pragma once

#include <optional>
#include <filesystem>

namespace l2dgui
{
namespace NativeDialogs
{

std::optional<std::filesystem::path> pickFolder();
std::optional<std::filesystem::path> pickFile(const wchar_t* filter = nullptr);
std::optional<std::filesystem::path> saveFile(const wchar_t* defaultName = nullptr, const wchar_t* defaultExt = nullptr);

} // namespace NativeDialogs
} // namespace l2dgui
