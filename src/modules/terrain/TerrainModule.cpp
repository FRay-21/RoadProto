#include "modules/terrain/TerrainModule.h"

#include "app/startup/ApplicationContext.h"
#include "application/terrain/TerrainUpdateSampleService.h"
#include "cad_adapter/objectarx/terrain/ObjectArxTerrainTinCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::terrain {

namespace {

void runTerrainMarkDirtyCommand()
{
    auto& context = app::ApplicationContext::instance();
    application::TerrainUpdateSampleService service(context.relationManager());
    const auto result = service.run();
    context.editor().writeMessage(result.summary);
}

void registerTerrainCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_TERRAIN_MARKDIRTY",
        L"Terrain Dirty Sample",
        L"TERRAIN",
        L"Marks sample profile and cross-section entities for rebuild after terrain update.",
        &runTerrainMarkDirtyCommand,
        true,
        true,
        L"docs/business/terrain/\u5730\u5f62\u66f4\u65b0\u8054\u52a8\u793a\u4f8b.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"DN_TERRAIN_TIN_CREATE",
        L"地形构网",
        L"TERRAIN",
        L"Extracts terrain points from CAD objects and creates a persistent TIN terrain entity.",
        cad_adapter::objectarx::terrainTinCreateCommandProcedure(),
        true,
        true,
        L"docs/business/terrain/\u5730\u5f62\u6784\u7f51_TIN\u5730\u5f62\u6570\u6a21.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"DN_TERRAIN_TIN_EDIT",
        L"编辑地形数模",
        L"TERRAIN",
        L"Edits display mode and rebuild parameters for an existing TIN terrain entity.",
        cad_adapter::objectarx::terrainTinEditCommandProcedure(),
        true,
        true,
        L"docs/business/terrain/\u5730\u5f62\u6570\u6a21_\u7f16\u8f91.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"DN_TERRAIN_TIN_EDIT_HANDLE",
        L"\u6309 handle \u7f16\u8f91\u6570\u6a21",
        L"TERRAIN",
        L"Internal double-click bridge command that edits an existing TIN terrain entity by handle.",
        cad_adapter::objectarx::terrainTinEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/terrain/\u5730\u5f62\u6570\u6a21_\u7f16\u8f91.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"DN_TERRAIN_TIN_EXPORT",
        L"\u5bfc\u51fa\u6570\u6a21",
        L"TERRAIN",
        L"Exports an existing TIN terrain entity to a portable .rmesh file.",
        cad_adapter::objectarx::terrainTinExportCommandProcedure(),
        true,
        true,
        L"docs/business/terrain/\u5730\u5f62\u6570\u6a21_RMesh\u5bfc\u5165\u5bfc\u51fa.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"DN_TERRAIN_TIN_IMPORT",
        L"\u5bfc\u5165\u6570\u6a21",
        L"TERRAIN",
        L"Imports a portable .rmesh file as a persistent DnTerrainTinEntity.",
        cad_adapter::objectarx::terrainTinImportCommandProcedure(),
        true,
        true,
        L"docs/business/terrain/\u5730\u5f62\u6570\u6a21_RMesh\u5bfc\u5165\u5bfc\u51fa.md",
        true});
}

void registerTerrainRibbon(ui::RibbonModel& ribbonModel)
{
    ribbonModel.ensurePanel(L"TERRAIN", L"数模");
}

} // namespace

core::ModuleDefinition createTerrainModule()
{
    return core::ModuleDefinition{
        L"Terrain Model",
        L"TERRAIN",
        L"Terrain digital model prototype module.",
        []() { return true; },
        []() { return true; },
        &registerTerrainCommands,
        &registerTerrainRibbon,
        L"docs/modules/terrain.md"};
}

} // namespace roadproto::modules::terrain
