#pragma once

#include <optional>
#include <string>

namespace roadproto::domain::terrain {

class TerrainTextElevationParser {
public:
    std::optional<double> tryParse(const std::wstring& text) const;
};

} // namespace roadproto::domain::terrain
