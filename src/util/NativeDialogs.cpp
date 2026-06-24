#include "util/NativeDialogs.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#include <vector>

namespace l2dgui
{
namespace NativeDialogs
{

std::optional<std::filesystem::path> pickFolder()
{
    std::optional<std::filesystem::path> result;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool needUninit = SUCCEEDED(hr) || hr == S_FALSE;

    IFileOpenDialog* pFileOpen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr))
    {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
        {
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
        }

        if (SUCCEEDED(pFileOpen->Show(NULL)))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem)))
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
                {
                    result = std::filesystem::path(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }

    if (needUninit)
    {
        CoUninitialize();
    }
    return result;
}

std::optional<std::filesystem::path> pickFile(const wchar_t* filter)
{
    std::optional<std::filesystem::path> result;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool needUninit = SUCCEEDED(hr) || hr == S_FALSE;

    IFileOpenDialog* pFileOpen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

    if (SUCCEEDED(hr))
    {
        if (filter != nullptr)
        {
            std::vector<COMDLG_FILTERSPEC> fileTypes;
            const wchar_t* p = filter;
            while (*p != L'\0')
            {
                const wchar_t* name = p;
                p += wcslen(p) + 1;
                if (*p == L'\0') break; // Malformed or end of filter
                const wchar_t* spec = p;
                p += wcslen(p) + 1;
                fileTypes.push_back({ name, spec });
            }
            fileTypes.push_back({ L"All Files (*.*)", L"*.*" });
            pFileOpen->SetFileTypes(static_cast<UINT>(fileTypes.size()), fileTypes.data());
        }

        if (SUCCEEDED(pFileOpen->Show(NULL)))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem)))
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
                {
                    result = std::filesystem::path(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }

    if (needUninit)
    {
        CoUninitialize();
    }
    return result;
}

} // namespace NativeDialogs

namespace NativeDialogs
{

std::optional<std::filesystem::path> saveFile(const wchar_t* defaultName, const wchar_t* defaultExt)
{
    std::optional<std::filesystem::path> result;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool needUninit = SUCCEEDED(hr) || hr == S_FALSE;

    IFileSaveDialog* pFileSave = nullptr;
    hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));

    if (SUCCEEDED(hr))
    {
        COMDLG_FILTERSPEC fileTypes[] = {
            { L"Live2D Project (*.json)", L"*.json" },
            { L"All Files (*.*)", L"*.*" }
        };
        pFileSave->SetFileTypes(2, fileTypes);
        if (defaultName) pFileSave->SetFileName(defaultName);
        if (defaultExt) pFileSave->SetDefaultExtension(defaultExt);

        if (SUCCEEDED(pFileSave->Show(NULL)))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileSave->GetResult(&pItem)))
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)))
                {
                    result = std::filesystem::path(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileSave->Release();
    }

    if (needUninit)
    {
        CoUninitialize();
    }
    return result;
}

} // namespace NativeDialogs
} // namespace l2dgui
