#include "core/logging/Logger.h"

#include <utility>

namespace roadproto::core {

Logger::Logger(LogSink sink)
    : sink_(std::move(sink))
{
}

void Logger::setSink(LogSink sink)
{
    sink_ = std::move(sink);
}

void Logger::info(const std::wstring& message) const
{
    write(LogLevel::Info, message);
}

void Logger::warning(const std::wstring& message) const
{
    write(LogLevel::Warning, message);
}

void Logger::error(const std::wstring& message) const
{
    write(LogLevel::Error, message);
}

void Logger::write(LogLevel level, const std::wstring& message) const
{
    if (sink_) {
        sink_(level, message);
    }
}

} // namespace roadproto::core
