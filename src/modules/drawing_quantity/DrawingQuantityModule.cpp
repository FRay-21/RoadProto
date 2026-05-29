#include "modules/drawing_quantity/DrawingQuantityModule.h"

#include "cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.h"
#include "cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::drawing_quantity {
namespace {

void registerDrawingQuantityCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_DRAWING_PAVEMENT_QUANTITY_TABLE",
        L"路面工程量统计表",
        L"DRAWING_QUANTITY",
        L"Exports pavement layer projected area and average-section volume quantities from drawn cross-section entities.",
        cad_adapter::objectarx::drawing_quantity::pavementQuantityTableCommandProcedure(),
        true,
        true,
        L"docs/business/drawing_quantity/路面工程量统计表.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND",
        L"路面结构图例",
        L"DRAWING_QUANTITY",
        L"Draws pavement structure layer template legends from a road model or drawn cross-section entities.",
        cad_adapter::objectarx::drawing_quantity::pavementStructureLegendCommandProcedure(),
        true,
        true,
        L"docs/business/drawing_quantity/路面结构图例.md",
        true});
}

void registerDrawingQuantityRibbon(ui::RibbonModel& ribbonModel)
{
    ribbonModel.ensurePanel(L"DRAWING_QUANTITY", L"出图出表");
}

} // namespace

core::ModuleDefinition createDrawingQuantityModule()
{
    return core::ModuleDefinition{
        L"Drawing Quantity",
        L"DRAWING_QUANTITY",
        L"Drawing, table and quantity prototype module.",
        []() { return true; },
        []() { return true; },
        &registerDrawingQuantityCommands,
        &registerDrawingQuantityRibbon,
        L"docs/modules/drawing_quantity.md"};
}

} // namespace roadproto::modules::drawing_quantity
