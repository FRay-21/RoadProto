#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::cross_section {

enum class SlopeTemplateKind {
    Fill,
    Cut
};

enum class SlopeComponentType {
    FillSlope,
    CutSlope,
    Berm
};

enum class SlopeGeometryConstraintMode {
    SlopeAndHeight,
    SlopeAndWidth,
    HeightAndWidth
};

struct SlopeTemplateRgbColor {
    int r = 120;
    int g = 120;
    int b = 120;
};

struct SlopeTemplateComponent {
    SlopeComponentType type = SlopeComponentType::FillSlope;
    SlopeGeometryConstraintMode constraintMode = SlopeGeometryConstraintMode::SlopeAndHeight;
    double slope = -1.0 / 1.5;
    double height = 4.0;
    double width = 6.0;
    double groundSearchHeightIncrement = 0.0;
    SlopeTemplateRgbColor color;
};

struct SlopeTemplateProperties {
    std::wstring name = L"\u8fb9\u5761\u6a21\u677f1";
    double displayScale = 10.0;
    SlopeTemplateKind kind = SlopeTemplateKind::Fill;
    bool stopAtGround = false;
    bool repeatLastGroupUntilGround = false;
};

struct SlopeTemplateData {
    SlopeTemplateProperties properties;
    std::vector<SlopeTemplateComponent> components;
};

struct SlopeResolvedGeometry {
    bool succeeded = false;
    std::wstring errorMessage;
    double slope = 0.0;
    double width = 0.0;
    double height = 0.0;
    double elevationDelta = 0.0;
};

struct SlopeRepeatGroup {
    std::size_t bermIndex = 0;
    std::size_t slopeIndex = 0;
};

class SlopeTemplateDefaults {
public:
    static SlopeTemplateData create(SlopeTemplateKind kind);
    static SlopeTemplateRgbColor defaultColorFor(SlopeComponentType type);
};

class SlopeTemplateRules {
public:
    static bool isSupportedDisplayScale(double displayScale);
    static SlopeResolvedGeometry resolveGeometry(const SlopeTemplateComponent& component);
    static std::optional<SlopeRepeatGroup> lastRepeatGroup(const SlopeTemplateData& data);
    static bool normalize(SlopeTemplateData& data, std::wstring& errorMessage);
};

const wchar_t* slopeTemplateKindCode(SlopeTemplateKind kind);
const wchar_t* slopeComponentTypeCode(SlopeComponentType type);
const wchar_t* slopeComponentTypeDisplayName(SlopeComponentType type);
const wchar_t* slopeGeometryConstraintModeCode(SlopeGeometryConstraintMode mode);
SlopeTemplateKind slopeTemplateKindFromCode(
    const std::wstring& code,
    SlopeTemplateKind fallback = SlopeTemplateKind::Fill);
SlopeComponentType slopeComponentTypeFromCode(
    const std::wstring& code,
    SlopeComponentType fallback = SlopeComponentType::FillSlope);
SlopeGeometryConstraintMode slopeGeometryConstraintModeFromCode(
    const std::wstring& code,
    SlopeGeometryConstraintMode fallback = SlopeGeometryConstraintMode::SlopeAndHeight);

} // namespace roadproto::domain::cross_section
