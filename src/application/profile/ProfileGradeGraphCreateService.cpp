#include "application/profile/ProfileGradeGraphCreateService.h"

#include "domain/profile/ProfileGradeGraphLayout.h"

namespace roadproto::application::profile {
namespace {

std::wstring makeGraphName(const std::wstring& roadName)
{
    if (roadName.empty()) {
        return L"\u9053\u8def\u62c9\u5761\u56fe";
    }

    return roadName + L"\u62c9\u5761\u56fe";
}

void applyDefaultProperties(domain::profile::ProfileGradeGraphData& graph, const std::wstring& roadName)
{
    graph.properties.graphName = makeGraphName(roadName);
    graph.properties.groundLineColorIndex = 4;
    graph.properties.groundLineWidth = 1.0;
    graph.properties.groundLinePrecision = 10.0;
    graph.properties.verticalScale = 10.0;
    graph.properties.gridSpacing = 10.0;
}

} // namespace

ProfileGradeGraphCreateResult ProfileGradeGraphCreateService::build(const ProfileGradeGraphCreateInput& input) const
{
    ProfileGradeGraphCreateResult result;
    if (input.groundSamples.size() < 2) {
        result.errorMessage = L"At least two ground samples are required.";
        return result;
    }

    result.graph.sourceType = input.sourceType;
    result.graph.roadCenterlineHandle = input.roadCenterlineHandle;
    result.graph.terrainTinHandle = input.terrainTinHandle;
    result.graph.dmxFilePath = input.dmxFilePath;
    result.graph.groundSamples = input.groundSamples;
    applyDefaultProperties(result.graph, input.roadName);

    const auto layout = domain::profile::ProfileGradeGraphLayout::calculate(result.graph);
    if (!layout.succeeded) {
        result.errorMessage = layout.errorMessage;
        return result;
    }

    result.succeeded = true;
    return result;
}

} // namespace roadproto::application::profile
