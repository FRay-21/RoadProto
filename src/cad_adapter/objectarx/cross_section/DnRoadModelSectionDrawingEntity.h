#pragma once

#include "domain/cross_section/SectionDrawingConfigModel.h"

#include "dbents.h"
#include "dbmain.h"
#include "gept3dar.h"
#include "gepnt3d.h"
#include "gevec3d.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx::cross_section {

struct RoadModelSectionDrawingPoint {
    double x = 0.0;
    double y = 0.0;
};

struct RoadModelSectionDrawingLine {
    int kind = 0;
    int colorR = 0;
    int colorG = 0;
    int colorB = 0;
    std::vector<RoadModelSectionDrawingPoint> points;
};

struct RoadModelSectionDrawingFace {
    std::wstring layerName;
    std::wstring componentName;
    std::wstring faceId;
    std::wstring sourceTemplateHandle;
    int sourceConfigRowIndex = -1;
    bool manualEdited = false;
    int colorR = 0;
    int colorG = 0;
    int colorB = 0;
    std::wstring hatchPattern = L"SOLID";
    double hatchAngle = 0.0;
    double hatchScale = 1.0;
    std::vector<RoadModelSectionDrawingPoint> points;
};

struct RoadModelSectionDrawingData {
    std::wstring roadModelHandle;
    std::wstring roadCenterlineHandle;
    std::wstring stationLabel;
    double station = 0.0;
    AcGePoint3d insertionPoint;
    double width = 0.0;
    double height = 0.0;
    std::vector<RoadModelSectionDrawingLine> lines;
    std::vector<RoadModelSectionDrawingFace> faces;
    roadproto::domain::cross_section::SectionDrawingConfigData config;
};

} // namespace roadproto::cad_adapter::objectarx::cross_section

class DnRoadModelSectionDrawingEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnRoadModelSectionDrawingEntity);

    DnRoadModelSectionDrawingEntity();

    Acad::ErrorStatus setDrawingData(
        const roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingData& data);
    const roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingData& drawingData() const;
    Acad::ErrorStatus setSectionDrawingConfig(
        const roadproto::domain::cross_section::SectionDrawingConfigData& config);
    Acad::ErrorStatus replaceFaces(
        std::vector<roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingFace> faces);
    Acad::ErrorStatus setSectionDrawingConfigAndFaces(
        const roadproto::domain::cross_section::SectionDrawingConfigData& config,
        std::vector<roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingFace> faces);

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;
    Acad::ErrorStatus subGetGripPoints(
        AcGePoint3dArray& gripPoints,
        AcDbIntArray& osnapModes,
        AcDbIntArray& geomIds) const override;
    Acad::ErrorStatus subMoveGripPointsAt(const AcDbIntArray& indices, const AcGeVector3d& offset) override;

private:
    roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingData data_;
    AcGeVector3d xAxis_;
    AcGeVector3d yAxis_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeRoadModelSectionDrawingEntityClass();
void uninitializeRoadModelSectionDrawingEntityClass();

} // namespace roadproto::cad_adapter::objectarx
