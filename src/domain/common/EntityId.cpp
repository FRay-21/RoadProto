#include "domain/common/EntityId.h"

#include <atomic>
#include <sstream>
#include <utility>

namespace roadproto::domain {

namespace {
std::atomic<unsigned long long> g_nextEntityNumber{1};
}

EntityId::EntityId(std::wstring value)
    : value_(std::move(value))
{
}

EntityId EntityId::fromString(std::wstring value)
{
    return EntityId(std::move(value));
}

EntityId EntityId::create(const std::wstring& prefix)
{
    std::wstringstream stream;
    stream << prefix << L"." << g_nextEntityNumber.fetch_add(1);
    return EntityId(stream.str());
}

const std::wstring& EntityId::value() const
{
    return value_;
}

bool EntityId::empty() const
{
    return value_.empty();
}

bool operator==(const EntityId& lhs, const EntityId& rhs)
{
    return lhs.value_ == rhs.value_;
}

bool operator!=(const EntityId& lhs, const EntityId& rhs)
{
    return !(lhs == rhs);
}

bool operator<(const EntityId& lhs, const EntityId& rhs)
{
    return lhs.value_ < rhs.value_;
}

} // namespace roadproto::domain
