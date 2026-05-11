#include "domain/alignment/AlignmentGeometry.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::alignment {

double distance(const AlignmentPoint2d& a, const AlignmentPoint2d& b)
{
    const auto dx = b.x - a.x;
    const auto dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

double clothoidA(double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength < 0.0) {
        return 0.0;
    }
    return std::sqrt(radius * spiralLength);
}

double clothoidCurvatureAt(double stationOnSpiral, double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength <= 0.0) {
        return 0.0;
    }
    const auto s = std::clamp(stationOnSpiral, 0.0, spiralLength);
    return s / (radius * spiralLength);
}

double clothoidTangentAngleAt(double stationOnSpiral, double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength <= 0.0) {
        return 0.0;
    }
    const auto s = std::clamp(stationOnSpiral, 0.0, spiralLength);
    return (s * s) / (2.0 * radius * spiralLength);
}

double clothoidEndX(double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength < 0.0) {
        return 0.0;
    }
    const auto l = spiralLength;
    const auto r2 = radius * radius;
    const auto r4 = r2 * r2;
    const auto r6 = r4 * r2;
    return l
        - (l * l * l) / (40.0 * r2)
        + (l * l * l * l * l) / (3456.0 * r4)
        - (l * l * l * l * l * l * l) / (599040.0 * r6);
}

double clothoidEndY(double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength < 0.0) {
        return 0.0;
    }
    const auto l = spiralLength;
    const auto r = radius;
    const auto r3 = r * r * r;
    const auto r5 = r3 * r * r;
    return (l * l) / (6.0 * r)
        - (l * l * l * l) / (336.0 * r3)
        + (l * l * l * l * l * l) / (42240.0 * r5);
}

double spiralShiftP(double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength <= 0.0) {
        return 0.0;
    }
    const auto beta = clothoidTangentAngleAt(spiralLength, radius, spiralLength);
    return clothoidEndY(radius, spiralLength) - radius * (1.0 - std::cos(beta));
}

double spiralTangentOffsetM(double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength <= 0.0) {
        return 0.0;
    }
    const auto beta = clothoidTangentAngleAt(spiralLength, radius, spiralLength);
    return clothoidEndX(radius, spiralLength) - radius * std::sin(beta);
}

double defaultSpiralTangentLength(double turnAngle, double radius, double spiralLength1, double spiralLength2)
{
    if (radius <= 0.0 || turnAngle <= 0.0) {
        return 0.0;
    }
    const auto p = (spiralShiftP(radius, spiralLength1) + spiralShiftP(radius, spiralLength2)) / 2.0;
    const auto m = (spiralTangentOffsetM(radius, spiralLength1) + spiralTangentOffsetM(radius, spiralLength2)) / 2.0;
    return (radius + p) * std::tan(turnAngle / 2.0) + m;
}

std::vector<std::wstring> roadGradeDisplayNames()
{
    return {
        L"高速公路",
        L"一级道路",
        L"二级道路",
        L"三级道路",
        L"四级道路",
        L"城市快速路",
        L"城市主干道",
        L"城市次干道",
        L"城市支路",
        L"其他道路",
        L"等外公路",
    };
}

std::wstring roadGradeToDisplayName(RoadGrade grade)
{
    const auto names = roadGradeDisplayNames();
    const auto index = static_cast<std::size_t>(grade);
    if (index >= names.size()) {
        return names[static_cast<std::size_t>(RoadGrade::Other)];
    }
    return names[index];
}

RoadGrade roadGradeFromIndex(std::size_t index)
{
    const auto names = roadGradeDisplayNames();
    if (index >= names.size()) {
        return RoadGrade::Other;
    }
    return static_cast<RoadGrade>(index);
}

} // namespace roadproto::domain::alignment
