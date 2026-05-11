#pragma once

#include "domain/alignment/AlignmentGeometry.h"

#include <cstddef>
#include <vector>

namespace roadproto::domain::alignment {

struct AlignmentGripEditResult {
    bool succeeded = true;
    bool changed = false;
};

class AlignmentGripEditService {
public:
    AlignmentGripEditResult applyGripOffsets(
        HorizontalAlignment& alignment,
        const std::vector<std::size_t>& gripIndices,
        double offsetX,
        double offsetY) const;
};

} // namespace roadproto::domain::alignment
