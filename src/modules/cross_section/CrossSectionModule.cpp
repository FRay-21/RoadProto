#include "modules/cross_section/CrossSectionModule.h"

#include "cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h"
#include "cad_adapter/objectarx/cross_section/ObjectArxSlopeTemplateCommand.h"
#include "cad_adapter/objectarx/cross_section/ObjectArxSubgradeTemplateCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::cross_section {
namespace {

void registerCrossSectionCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_SUBGRADE_TEMPLATE_CREATE",
        L"\u521b\u5efa\u8def\u57fa\u6a21\u677f",
        L"CROSS_SECTION",
        L"Creates an independent subgrade template cross-section entity.",
        cad_adapter::objectarx::cross_section::subgradeTemplateCreateCommandProcedure(),
        true,
        true,
        L"docs/business/cross_section/\u8def\u57fa\u6a21\u677f_\u521b\u5efa.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE",
        L"\u6309 handle \u7f16\u8f91\u8def\u57fa\u6a21\u677f",
        L"CROSS_SECTION",
        L"Internal double-click bridge command that edits a subgrade template by handle.",
        cad_adapter::objectarx::cross_section::subgradeTemplateEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/\u8def\u57fa\u6a21\u677f_\u7f16\u8f91.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE",
        L"\u5e94\u7528\u8def\u57fa\u6a21\u677f\u5bf9\u8bdd\u6846\u7ed3\u679c",
        L"CROSS_SECTION",
        L"Internal WPF bridge command that applies subgrade template dialog response files.",
        cad_adapter::objectarx::cross_section::subgradeTemplateApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/\u8def\u57fa\u6a21\u677f_WPF\u6865\u63a5\u56de\u5199.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_SLOPE_TEMPLATE_CREATE",
        L"\u521b\u5efa\u8fb9\u5761\u6a21\u677f",
        L"CROSS_SECTION",
        L"Creates an independent slope template cross-section entity.",
        cad_adapter::objectarx::cross_section::slopeTemplateCreateCommandProcedure(),
        true,
        true,
        L"docs/business/cross_section/\u8fb9\u5761\u6a21\u677f_\u521b\u5efa.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE",
        L"\u6309 handle \u7f16\u8f91\u8fb9\u5761\u6a21\u677f",
        L"CROSS_SECTION",
        L"Internal bridge command that edits a slope template by handle.",
        cad_adapter::objectarx::cross_section::slopeTemplateEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/\u8fb9\u5761\u6a21\u677f_\u7f16\u8f91.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE",
        L"\u5e94\u7528\u8fb9\u5761\u6a21\u677f\u5bf9\u8bdd\u6846\u7ed3\u679c",
        L"CROSS_SECTION",
        L"Internal WPF bridge command that applies slope template dialog response files.",
        cad_adapter::objectarx::cross_section::slopeTemplateApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/\u8fb9\u5761\u6a21\u677f_WPF\u6865\u63a5\u56de\u5199.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_CREATE",
        L"横断面戴帽",
        L"CROSS_SECTION",
        L"Creates a road model from cross-section design inputs.",
        cad_adapter::objectarx::cross_section::roadModelCreateCommandProcedure(),
        true,
        true,
        L"docs/business/cross_section/横断面戴帽_道路模型创建.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_EDIT",
        L"编辑道路模型",
        L"CROSS_SECTION",
        L"Edits an existing road model through the WPF bridge.",
        cad_adapter::objectarx::cross_section::roadModelEditCommandProcedure(),
        true,
        true,
        L"docs/business/cross_section/道路模型_编辑.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_EDIT_HANDLE",
        L"按 handle 编辑道路模型",
        L"CROSS_SECTION",
        L"Internal double-click bridge command that edits a road model by handle.",
        cad_adapter::objectarx::cross_section::roadModelEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/道路模型_编辑.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE",
        L"应用道路模型对话框结果",
        L"CROSS_SECTION",
        L"Internal WPF bridge command that applies road model dialog response files.",
        cad_adapter::objectarx::cross_section::roadModelApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/道路模型_WPF桥接回写.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_VIEW_SECTION",
        L"查看横断面",
        L"CROSS_SECTION",
        L"Opens a read-only station-by-station cross-section preview for a road model.",
        cad_adapter::objectarx::cross_section::roadModelViewSectionCommandProcedure(),
        true,
        true,
        L"docs/business/cross_section/查看横断面.md",
        true});
}

void registerCrossSectionRibbon(ui::RibbonModel& ribbonModel)
{
    ribbonModel.ensurePanel(L"CROSS_SECTION", L"\u6a2a\u65ad\u9762\u8bbe\u8ba1");
}

} // namespace

core::ModuleDefinition createCrossSectionModule()
{
    return core::ModuleDefinition{
        L"Cross Section",
        L"CROSS_SECTION",
        L"Cross-section design prototype module.",
        []() { return true; },
        []() { return true; },
        &registerCrossSectionCommands,
        &registerCrossSectionRibbon,
        L"docs/modules/cross_section.md"};
}

} // namespace roadproto::modules::cross_section
