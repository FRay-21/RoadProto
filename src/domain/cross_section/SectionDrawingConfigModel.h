#pragma once

#include "domain/cross_section/SubgradeTemplateModel.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::cross_section {

struct SectionDrawingComponentTypeSelection {
    SubgradeSide side = SubgradeSide::Right;
    SubgradeComponentType componentType = SubgradeComponentType::TravelLane;
};

struct SectionPavementLayerConfigRow {
    double startStation = 0.0;
    double endStation = 0.0;
    std::vector<SectionDrawingComponentTypeSelection> componentTypes;
    std::wstring templateHandle;
    std::wstring templateName;
};

enum class SectionClearTableScope {
    Left,
    Right,
    Both,
};

struct SectionClearTableConfigRow {
    double startStation = 0.0;
    double endStation = 0.0;
    double leftSlopeRatio = 1.5;
    double rightSlopeRatio = 1.5;
    double thickness = 0.3;
    SectionClearTableScope scope = SectionClearTableScope::Both;
    bool clearCut = true;
};

struct SectionDrawingConfigData {
    std::wstring configPath;
    std::vector<SectionPavementLayerConfigRow> pavementRows;
    std::vector<SectionClearTableConfigRow> clearTableRows;
    int version = 1;
};

struct SectionDrawingResolvedPavementRow {
    std::size_t rowIndex = 0;
    SectionPavementLayerConfigRow row;
};

struct SectionDrawingResolvedClearTableRow {
    std::size_t rowIndex = 0;
    SectionClearTableConfigRow row;
};

class SectionDrawingConfigRules {
public:
    static bool normalize(SectionDrawingConfigData& data, std::wstring& errorMessage);

    static std::optional<SectionDrawingResolvedPavementRow> resolvePavementRow(
        const SectionDrawingConfigData& data,
        double station);
    static std::optional<SectionDrawingResolvedPavementRow> resolvePavementRow(
        const SectionDrawingConfigData& data,
        double station,
        SubgradeSide side,
        SubgradeComponentType componentType);

    static bool matchesComponent(
        const SectionPavementLayerConfigRow& row,
        SubgradeSide side,
        SubgradeComponentType componentType);

    static std::optional<SectionDrawingResolvedClearTableRow> resolveClearTableRow(
        const SectionDrawingConfigData& data,
        double station,
        SubgradeSide side,
        bool isCutSection);

    static bool matchesClearTableScope(
        SectionClearTableScope scope,
        SubgradeSide side);

    static std::wstring componentSelectionCode(const SectionDrawingComponentTypeSelection& selection);
    static std::wstring componentSelectionDisplayName(const SectionDrawingComponentTypeSelection& selection);

    static std::optional<SectionDrawingComponentTypeSelection> componentSelectionFromText(
        const std::wstring& text);

    static std::wstring clearTableScopeCode(SectionClearTableScope scope);
    static std::wstring clearTableScopeDisplayName(SectionClearTableScope scope);
    static std::optional<SectionClearTableScope> clearTableScopeFromText(const std::wstring& text);
};

class SectionDrawingConfigCsv {
public:
    static std::string write(const SectionDrawingConfigData& data);

    static std::optional<SectionDrawingConfigData> read(
        const std::string& csv,
        const std::wstring& configPath,
        std::wstring& errorMessage);
};

} // namespace roadproto::domain::cross_section
