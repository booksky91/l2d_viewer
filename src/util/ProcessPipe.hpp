#pragma once

#include <cstdio>
#include <string>
#include <thread>

namespace l2dgui
{

class WritePipe
{
public:
    WritePipe() = default;
    ~WritePipe();

    WritePipe(const WritePipe&) = delete;
    WritePipe& operator=(const WritePipe&) = delete;

    bool open(const std::string& command, std::string* error = nullptr);
    bool write(const void* data, size_t sizeBytes);
    int close();
    bool isOpen() const
    {
#if defined(_WIN32)
        return _hStdinWrite != nullptr;
#else
        return _pipe != nullptr;
#endif
    }

private:
#if defined(_WIN32)
    void* _hProcess = nullptr; // void* to avoid including windows.h in header
    void* _hThread = nullptr;
    void* _hStdinWrite = nullptr;
    void* _hStderrRead = nullptr;
    std::thread _readThread;
#else
    FILE* _pipe = nullptr;
#endif
};

std::string quoteArg(const std::string& arg);

} // namespace l2dgui
