#include "gui/ConsoleLog.hpp"
#include <imgui.h>

namespace l2dgui
{

ConsoleLog& ConsoleLog::getInstance()
{
    static ConsoleLog instance;
    return instance;
}

void ConsoleLog::addLog(LogLevel level, const std::string& message)
{
    // Filter out highly repetitive/useless SDK and idle looping logs
    if (message.find("is fired on LAppModel!!") != std::string::npos)
    {
        return;
    }
    if (message.find("[APP]start motion:") != std::string::npos && 
        (message.find("Idle") != std::string::npos || message.find("idle") != std::string::npos))
    {
        return;
    }
    if (message.find("[NATIVE] start motion") != std::string::npos && 
        (message.find("group=Idle") != std::string::npos || message.find("group=idle") != std::string::npos))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    _entries.push_back({level, message});
    if (_entries.size() > MAX_LOGS)
    {
        _entries.erase(_entries.begin());
    }
}

const std::vector<LogEntry>& ConsoleLog::getEntries() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _entries;
}

void ConsoleLog::clear()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _entries.clear();
}

void ConsoleLog::drawConsolePanel()
{
    std::lock_guard<std::mutex> lock(_mutex);

    ImGui::BeginChild("ConsoleLogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : _entries)
    {
        ImVec4 color;
        const char* prefix = "[I]";
        switch (entry.level)
        {
        case LOG_INFO:
            color = ImVec4(0.1f, 0.6f, 0.1f, 1.0f);
            prefix = "[I]";
            break;
        case LOG_DEBUG:
            color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            prefix = "[D]";
            break;
        case LOG_WARNING:
            color = ImVec4(0.7f, 0.5f, 0.1f, 1.0f);
            prefix = "[W]";
            break;
        case LOG_ERROR:
            color = ImVec4(0.9f, 0.1f, 0.1f, 1.0f);
            prefix = "[E]";
            break;
        }

        ImGui::TextColored(color, "%s %s", prefix, entry.message.c_str());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}

} // namespace l2dgui
