#include "application/alignment/AlignmentCreateService.h"

namespace roadproto::application {

domain::alignment::HorizontalAlignmentBuildResult AlignmentCreateService::build(
    const domain::alignment::HorizontalAlignmentInput& input) const
{
    domain::alignment::HorizontalAlignmentBuilder builder;
    return builder.build(input);
}

} // namespace roadproto::application
