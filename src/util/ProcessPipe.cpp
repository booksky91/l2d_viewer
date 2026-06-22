#include "util/ProcessPipe.hpp"
#include "gui/ConsoleLog.hpp"
#include <stdexcept>

#if defined(_WIN32)
  #include "util/PathUtil.hpp"
  #include <windows.h>
#else
  #define POPEN popen
  #define PCLOSE pclose
#endif

namespace l2dgui
{

WritePipe::~WritePipe()
{
    close();
}

bool WritePipe::open(const std::string& command, std::string* error)
{
    close();
#if defined(_WIN32)
    // 1. Create stdin pipe
    HANDLE stdinRead = NULL;
    HANDLE stdinWrite = NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0))
    {
        if (error) *error = "Failed to create stdin pipe.";
        return false;
    }
    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);

    // 2. Create stderr pipe to capture ffmpeg output
    HANDLE stderrRead = NULL;
    HANDLE stderrWrite = NULL;
    if (!CreatePipe(&stderrRead, &stderrWrite, &sa, 0))
    {
        CloseHandle(stdinRead);
        CloseHandle(stdinWrite);
        if (error) *error = "Failed to create stderr pipe.";
        return false;
    }
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

    // 3. Start child process
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = stdinRead;
    si.hStdError = stderrWrite;
    si.hStdOutput = stderrWrite; // Redirect stdout to stderr too
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring wideCmd = utf8ToWide(command);

    // Use CREATE_NO_WINDOW to completely prevent console flashing!
    BOOL success = CreateProcessW(
        NULL,
        &wideCmd[0], // Modifiable buffer
        NULL, NULL,
        TRUE, // Inherit handles
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi
    );

    // Close parent handles to child's ends of the pipes
    CloseHandle(stdinRead);
    CloseHandle(stderrWrite);

    if (!success)
    {
        CloseHandle(stdinWrite);
        CloseHandle(stderrRead);
        if (error) *error = "CreateProcessW failed for command: " + command;
        return false;
    }

    _hProcess = pi.hProcess;
    _hThread = pi.hThread;
    _hStdinWrite = stdinWrite;
    _hStderrRead = stderrRead;

    // 4. Start background thread to read stderr and print to ConsoleLog
    _readThread = std::thread([this]() {
        char buffer[1024];
        DWORD bytesRead = 0;
        std::string lineBuf;
        while (ReadFile(static_cast<HANDLE>(_hStderrRead), buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            for (DWORD i = 0; i < bytesRead; ++i)
            {
                if (buffer[i] == '\n' || buffer[i] == '\r')
                {
                    if (!lineBuf.empty())
                    {
                        ConsoleLog::getInstance().addLog(LOG_INFO, lineBuf);
                        lineBuf.clear();
                    }
                }
                else
                {
                    lineBuf.push_back(buffer[i]);
                }
            }
        }
        if (!lineBuf.empty())
        {
            ConsoleLog::getInstance().addLog(LOG_INFO, lineBuf);
        }
    });

#else
    _pipe = POPEN(command.c_str(), "w");
    if (!_pipe)
    {
        if (error) *error = "Failed to open pipe for command: " + command;
        return false;
    }
#endif
    return true;
}

bool WritePipe::write(const void* data, size_t sizeBytes)
{
#if defined(_WIN32)
    if (!_hStdinWrite) return false;
    DWORD bytesWritten = 0;
    return WriteFile(static_cast<HANDLE>(_hStdinWrite), data, static_cast<DWORD>(sizeBytes), &bytesWritten, NULL) && bytesWritten == sizeBytes;
#else
    if (!_pipe) return false;
    return std::fwrite(data, 1, sizeBytes, _pipe) == sizeBytes;
#endif
}

int WritePipe::close()
{
#if defined(_WIN32)
    if (_hStdinWrite)
    {
        CloseHandle(static_cast<HANDLE>(_hStdinWrite));
        _hStdinWrite = NULL;
    }
    if (_readThread.joinable())
    {
        _readThread.join();
    }
    if (_hStderrRead)
    {
        CloseHandle(static_cast<HANDLE>(_hStderrRead));
        _hStderrRead = NULL;
    }

    int exitCode = 0;
    if (_hProcess)
    {
        WaitForSingleObject(static_cast<HANDLE>(_hProcess), INFINITE);
        DWORD code = 0;
        if (GetExitCodeProcess(static_cast<HANDLE>(_hProcess), &code))
        {
            exitCode = static_cast<int>(code);
        }
        CloseHandle(static_cast<HANDLE>(_hProcess));
        _hProcess = NULL;
    }
    if (_hThread)
    {
        CloseHandle(static_cast<HANDLE>(_hThread));
        _hThread = NULL;
    }
    return exitCode;
#else
    if (!_pipe) return 0;
    FILE* p = _pipe;
    _pipe = nullptr;
    return PCLOSE(p);
#endif
}

std::string quoteArg(const std::string& arg)
{
#if defined(_WIN32)
    std::string out = "\"";
    for (char c : arg)
    {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (char c : arg)
    {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
#endif
}

} // namespace l2dgui
