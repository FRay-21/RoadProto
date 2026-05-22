#include "app/startup/ApplicationContext.h"
#include "app/startup/Startup.h"
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"
#include "cad_adapter/objectarx/ObjectArxCommandRegistrar.h"
#include "cad_adapter/objectarx/ObjectArxEditor.h"
#include "cad_adapter/objectarx/ObjectArxRibbonAdapter.h"
#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"
#include "cad_adapter/objectarx/terrain/DnTerrainTinEntity.h"

#include "rxregsvc.h"

namespace {

roadproto::cad_adapter::objectarx::ObjectArxEditor g_editor;

void initializeCustomEntityClasses()
{
    using namespace roadproto;

    cad_adapter::objectarx::initializeTerrainTinEntityClass();
    cad_adapter::objectarx::initializeRoadCenterlineEntityClass();
    cad_adapter::objectarx::initializeProfileGradeGraphEntityClass();
    cad_adapter::objectarx::initializeProfileVerticalCurveEntityClass();
    cad_adapter::objectarx::initializeSubgradeTemplateEntityClass();
    cad_adapter::objectarx::initializeSlopeTemplateEntityClass();
    cad_adapter::objectarx::initializePavementLayerTemplateEntityClass();
    cad_adapter::objectarx::initializeRoadModelEntityClass();
}

void uninitializeCustomEntityClasses()
{
    using namespace roadproto;

    cad_adapter::objectarx::uninitializeRoadModelEntityClass();
    cad_adapter::objectarx::uninitializePavementLayerTemplateEntityClass();
    cad_adapter::objectarx::uninitializeSlopeTemplateEntityClass();
    cad_adapter::objectarx::uninitializeSubgradeTemplateEntityClass();
    cad_adapter::objectarx::uninitializeProfileVerticalCurveEntityClass();
    cad_adapter::objectarx::uninitializeProfileGradeGraphEntityClass();
    cad_adapter::objectarx::uninitializeRoadCenterlineEntityClass();
    cad_adapter::objectarx::uninitializeTerrainTinEntityClass();
}

} // namespace

extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt)
{
    using namespace roadproto;

    switch (msg) {
    case AcRx::kInitAppMsg: {
        acrxDynamicLinker->unlockApplication(pkt);
        acrxDynamicLinker->registerAppMDIAware(pkt);
        initializeCustomEntityClasses();

        if (!app::initialize(g_editor)) {
            uninitializeCustomEntityClasses();
            return AcRx::kRetError;
        }

        auto& context = app::ApplicationContext::instance();
        const auto commandsRegistered = cad_adapter::objectarx::registerCommands(
            context.commandRegistry(),
            context.config().commandGroupName,
            g_editor);
        if (!commandsRegistered) {
            cad_adapter::objectarx::unregisterCommands(context.config().commandGroupName);
            app::shutdown();
            uninitializeCustomEntityClasses();
            return AcRx::kRetError;
        }

        cad_adapter::objectarx::ObjectArxRibbonAdapter ribbonAdapter;
        ribbonAdapter.install(context.ribbonModel(), g_editor);
        return AcRx::kRetOK;
    }
    case AcRx::kUnloadAppMsg: {
        auto& context = app::ApplicationContext::instance();
        cad_adapter::objectarx::unregisterCommands(context.config().commandGroupName);
        app::shutdown();
        uninitializeCustomEntityClasses();
        return AcRx::kRetOK;
    }
    default:
        return AcRx::kRetOK;
    }
}
