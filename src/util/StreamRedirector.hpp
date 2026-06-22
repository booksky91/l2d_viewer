#pragma once

#include "gui/ConsoleLog.hpp"
#include <iostream>
#include <streambuf>
#include <string>
#include <mutex>

namespace l2dgui
{

class StreamToLogRedirector : public std::streambuf
{
public:
    StreamToLogRedirector(std::ostream& stream, LogLevel level)
        : _stream(stream), _level(level)
    {
        _oldBuf = _stream.rdbuf(this);
    }

    ~StreamToLogRedirector()
    {
        _stream.rdbuf(_oldBuf);
        if (!_buffer.empty())
        {
            ConsoleLog::getInstance().addLog(_level, _buffer);
        }
    }

protected:
    virtual int_type overflow(int_type c) override
    {
        if (c == EOF)
        {
            return traits_type::not_eof(c);
        }

        std::lock_guard<std::mutex> lock(_mutex);
        if (c == '\n')
        {
            ConsoleLog::getInstance().addLog(_level, _buffer);
            _buffer.clear();
        }
        else
        {
            _buffer.push_back(static_cast<char>(c));
        }

        return c;
    }

    virtual std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (std::streamsize i = 0; i < n; ++i)
        {
            if (s[i] == '\n')
            {
                ConsoleLog::getInstance().addLog(_level, _buffer);
                _buffer.clear();
            }
            else
            {
                _buffer.push_back(s[i]);
            }
        }
        return n;
    }

private:
    std::ostream& _stream;
    std::streambuf* _oldBuf;
    LogLevel _level;
    std::string _buffer;
    std::mutex _mutex;
};

} // namespace l2dgui
