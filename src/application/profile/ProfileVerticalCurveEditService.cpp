#include "application/profile/ProfileVerticalCurveEditService.h"

#include <cmath>

namespace roadproto::application::profile {

namespace {

bool isFinite(double value)
{
    return std::isfinite(value);
}

ProfileVerticalCurveEditResult fail(const std::wstring& message)
{
    return ProfileVerticalCurveEditResult{false, false, message};
}

} // namespace

ProfileVerticalCurveEditResult ProfileVerticalCurveEditService::applyDialogEdit(
    domain::profile::ProfileVerticalCurveData& data,
    const ProfileVerticalCurveDialogEdit& edit) const
{
    if (!isFinite(edit.startStation) || !isFinite(edit.startElevation) ||
        !isFinite(edit.endStation) || !isFinite(edit.endElevation)) {
        return fail(L"Start and end station/elevation values must be finite.");
    }
    if (edit.endStation <= edit.startStation) {
        return fail(L"End station must be greater than start station.");
    }

    const auto original = data;
    data.properties.name = edit.name.empty() ? L"\u7ad6\u66f2\u7ebf" : edit.name;
    data.pvis = edit.pvis;
    data.controlPoints.clear();
    data.controlPoints.push_back(
        domain::profile::VerticalCurveControlPoint{
            domain::profile::VerticalCurvePointRole::Start,
            edit.startStation,
            edit.startElevation});
    for (const auto& pvi : edit.pvis) {
        data.controlPoints.push_back(
            domain::profile::VerticalCurveControlPoint{
                domain::profile::VerticalCurvePointRole::Pvi,
                pvi.station,
                pvi.elevation});
    }
    data.controlPoints.push_back(
        domain::profile::VerticalCurveControlPoint{
            domain::profile::VerticalCurvePointRole::End,
            edit.endStation,
            edit.endElevation});

    const auto built = domain::profile::ProfileVerticalCurveCalculator::rebuild(data);
    if (!built.succeeded) {
        data = original;
        return fail(built.errorMessage);
    }

    return ProfileVerticalCurveEditResult{true, true, L""};
}

ProfileVerticalCurveEditResult ProfileVerticalCurveEditService::applyGripMove(
    domain::profile::ProfileVerticalCurveData& data,
    const ProfileVerticalCurveGripEdit& edit) const
{
    using domain::profile::ProfileVerticalCurveCalculator;
    using domain::profile::VerticalCurveGripRole;

    switch (edit.role) {
    case VerticalCurveGripRole::StartPoint:
        return ProfileVerticalCurveCalculator::moveControlPoint(data, 0, edit.station, edit.elevation);
    case VerticalCurveGripRole::EndPoint:
        if (data.controlPoints.empty()) {
            return fail(L"Control point index is out of range.");
        }
        return ProfileVerticalCurveCalculator::moveControlPoint(
            data,
            data.controlPoints.size() - 1,
            edit.station,
            edit.elevation);
    case VerticalCurveGripRole::PviPoint:
        return ProfileVerticalCurveCalculator::movePvi(data, edit.index, edit.station, edit.elevation);
    case VerticalCurveGripRole::RadiusPoint:
        return ProfileVerticalCurveCalculator::updateRadius(data, edit.index, edit.radius);
    }

    return fail(L"Unsupported vertical curve grip role.");
}

ProfileVerticalCurveEditResult ProfileVerticalCurveEditService::addPvi(
    domain::profile::ProfileVerticalCurveData& data,
    double station,
    double elevation,
    double radius) const
{
    return domain::profile::ProfileVerticalCurveCalculator::insertPvi(data, station, elevation, radius);
}

ProfileVerticalCurveEditResult ProfileVerticalCurveEditService::deletePvi(
    domain::profile::ProfileVerticalCurveData& data,
    std::size_t pviIndex) const
{
    return domain::profile::ProfileVerticalCurveCalculator::removePvi(data, pviIndex);
}

} // namespace roadproto::application::profile
