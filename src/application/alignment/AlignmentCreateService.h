#pragma once

#include "domain/alignment/HorizontalAlignmentBuilder.h"

namespace roadproto::application {

class AlignmentCreateService {
public:
    domain::alignment::HorizontalAlignmentBuildResult build(
        const domain::alignment::HorizontalAlignmentInput& input) const;
};

} // namespace roadproto::application
