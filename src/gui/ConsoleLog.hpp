#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace l2dgui
{

enum LogLevel
{
    LOG_INFO = 0,
    LOG_DEBUG,
    LOG_WARNING,
    LOG_ERROR
};

struct LogEntry
{
    LogLevel level;
    std::string message;
};

class ConsoleLog
{
public:
    static ConsoleLog& getInstance();

    void addLog(LogLevel level, const std::string& message);
    const std::vector<LogEntry>& getEntries() const;
    void clear();

    void drawConsolePanel();

private:
    ConsoleLog() = default;
    std::vector<LogEntry> _entries;
    mutable std::mutex _mutex;
    const size_t MAX_LOGS = 500;
};

} // namespace l2dgui
