#pragma once

#include <string>

namespace roadproto::domain::alignment {

class StationFormatter {
public:
    static std::wstring format(double station);
};

} // namespace roadproto::domain::alignment
