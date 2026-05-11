#pragma once

#include "domain/alignment/AlignmentElementChainBuilder.h"

#include <string>
#include <vector>

namespace roadproto::domain::alignment {

enum class IcdUnitType {
    Line = 1,
    CircularArc = 2,
    SpiralIn = 3,
    SpiralOut = 4,
    PartialSpiralLargeToSmall = 5,
    PartialSpiralSmallToLarge = 6,
};

struct IcdAlignmentUnit {
    IcdUnitType type = IcdUnitType::Line;
    double length = 0.0;
    double radius = 0.0;
    double a = 0.0;
    double startRadius = 0.0;
    double endRadius = 0.0;
    int turn = 0;
    bool hasEndHeading = false;
    double endHeading = 0.0;
};

struct IcdAlignmentDocument {
    std::wstring startStationText = L"0.000000";
    double startStation = 0.0;
    AlignmentPoint2d startPoint;
    double startHeading = 0.0;
    std::vector<IcdAlignmentUnit> units;
};

struct IcdAlignmentReadResult {
    bool succeeded = false;
    std::wstring errorMessage;
    IcdAlignmentDocument document;
};

class IcdAlignmentFile {
public:
    IcdAlignmentReadResult read(const std::wstring& path) const;
    bool write(const std::wstring& path, const IcdAlignmentDocument& document, std::wstring& errorMessage) const;
    IcdAlignmentDocument documentFromAlignment(const HorizontalAlignment& alignment) const;
    HorizontalAlignmentBuildResult alignmentFromDocument(
        const IcdAlignmentDocument& document,
        const RoadCenterlineProperties& properties) const;
};

} // namespace roadproto::domain::alignment
