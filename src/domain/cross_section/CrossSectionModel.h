#pragma once

#include "domain/common/EntityId.h"

#include <string>

namespace roadproto::domain {

struct CrossSectionModel {
    EntityId id;
    std::wstring name;
};

} // namespace roadproto::domain
