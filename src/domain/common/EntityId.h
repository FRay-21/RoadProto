#pragma once

#include <string>

namespace roadproto::domain {

class EntityId {
public:
    EntityId() = default;

    static EntityId fromString(std::wstring value);
    static EntityId create(const std::wstring& prefix);

    const std::wstring& value() const;
    bool empty() const;

    friend bool operator==(const EntityId& lhs, const EntityId& rhs);
    friend bool operator!=(const EntityId& lhs, const EntityId& rhs);
    friend bool operator<(const EntityId& lhs, const EntityId& rhs);

private:
    explicit EntityId(std::wstring value);

    std::wstring value_;
};

} // namespace roadproto::domain
