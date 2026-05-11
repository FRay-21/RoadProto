#pragma once

#include "domain/common/EntityId.h"

#include <string>

namespace roadproto::domain {

struct ProfileModel {
    EntityId id;
    std::wstring name;
};

} // namespace roadproto::domain
