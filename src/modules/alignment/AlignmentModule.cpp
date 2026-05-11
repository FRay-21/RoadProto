#include "modules/alignment/AlignmentModule.h"

#include "cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::alignment {
namespace {

void registerAlignmentCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_CENTERLINE_CREATE",
        L"平面布线",
        L"ALIGNMENT",
        L"Creates a road centerline alignment entity from picked control points.",
        cad_adapter::objectarx::alignmentCenterlineCreateCommandProcedure(),
        true,
        true,
        L"docs/business/alignment/平面布线_道路中线创建.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_CURVE_PARAM_EDIT",
        L"编辑平曲线参数",
        L"ALIGNMENT",
        L"Edits horizontal curve tangent, spiral and radius parameters for a road centerline.",
        cad_adapter::objectarx::alignmentCurveParamEditCommandProcedure(),
        true,
        true,
        L"docs/business/alignment/平曲线参数编辑.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_CENTERLINE_EXPORT_ICD",
        L"导出中线 ICD",
        L"ALIGNMENT",
        L"Exports a road centerline entity to an ICD horizontal alignment file.",
        cad_adapter::objectarx::alignmentCenterlineExportIcdCommandProcedure(),
        true,
        true,
        L"docs/business/alignment/道路中线_ICD导入导出.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_CENTERLINE_IMPORT_ICD",
        L"导入中线 ICD",
        L"ALIGNMENT",
        L"Imports an ICD horizontal alignment file as a road centerline entity.",
        cad_adapter::objectarx::alignmentCenterlineImportIcdCommandProcedure(),
        true,
        true,
        L"docs/business/alignment/道路中线_ICD导入导出.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_CENTERLINE_EDIT_HANDLE",
        L"按 handle 编辑道路中线",
        L"ALIGNMENT",
        L"Internal double-click bridge command that edits road centerline properties by handle.",
        cad_adapter::objectarx::alignmentCenterlineEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/alignment/道路中线_属性编辑.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_APPLY_DIALOG_FILE",
        L"应用道路中线对话框结果",
        L"ALIGNMENT",
        L"Internal WPF bridge command that applies road centerline dialog response files.",
        cad_adapter::objectarx::alignmentApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/alignment/道路中线_WPF桥接回写.md",
        false});
}

void registerAlignmentRibbon(ui::RibbonModel& ribbonModel)
{
    ribbonModel.ensurePanel(L"ALIGNMENT", L"平面设计");
}

} // namespace

core::ModuleDefinition createAlignmentModule()
{
    return core::ModuleDefinition{
        L"Alignment",
        L"ALIGNMENT",
        L"Horizontal alignment and road centerline prototype module.",
        []() { return true; },
        []() { return true; },
        &registerAlignmentCommands,
        &registerAlignmentRibbon,
        L"docs/modules/alignment.md"};
}

} // namespace roadproto::modules::alignment
