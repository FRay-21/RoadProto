#include "cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h"
#include "domain/quantity/PavementStructureLegend.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"

#include <string>
#include <vector>
#endif

namespace roadproto::cad_adapter::objectarx::drawing_quantity {
namespace {

#ifndef ROADPROTO_TEST_BUILD

std::vector<AcDbObjectId> collectSectionDrawingsForRoadModel(const std::wstring& roadModelHandle)
{
    (void)roadModelHandle;
    return {};
}

std::vector<std::wstring> collectTemplateHandlesFromSectionDrawings(const std::vector<AcDbObjectId>& sectionDrawingIds)
{
    (void)sectionDrawingIds;
    return {};
}

std::vector<std::wstring> collectTemplateHandlesFromRoadModel(
    const roadproto::domain::cross_section::RoadModelData& roadModelData)
{
    (void)roadModelData;
    return {};
}

bool appendOrdinaryLegendEntities(
    const roadproto::domain::quantity::PavementStructureLegendPlan& plan,
    const AcGePoint3d& insertionPoint,
    roadproto::cad_adapter::IEditor& editor)
{
    (void)plan;
    (void)insertionPoint;
    editor.writeWarning(L"路面结构图例绘制功能尚未完成。");
    return false;
}

void runPavementStructureLegendCommand()
{
    auto* context = roadproto::app::startup::currentApplicationContext();
    if (context == nullptr || context->editor == nullptr) {
        return;
    }

    context->editor->writeWarning(L"路面结构图例命令尚未完成。");
}

#else

void runPavementStructureLegendCommand()
{
}

#endif

} // namespace

core::CommandProcedure pavementStructureLegendCommandProcedure()
{
    return &runPavementStructureLegendCommand;
}

} // namespace roadproto::cad_adapter::objectarx::drawing_quantity
