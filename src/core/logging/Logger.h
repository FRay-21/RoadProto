#pragma once

#include <functional>
#include <string>

namespace roadproto::core {

enum class LogLevel {
    Info,
    Warning,
    Error
};

using LogSink = std::function<void(LogLevel, const std::wstring&)>;

class Logger {
public:
    explicit Logger(LogSink sink = {});

    void setSink(LogSink sink);
    void info(const std::wstring& message) const;
    void warning(const std::wstring& message) const;
    void error(const std::wstring& message) const;

private:
    void write(LogLevel level, const std::wstring& message) const;

    LogSink sink_;
};

} // namespace roadproto::core
