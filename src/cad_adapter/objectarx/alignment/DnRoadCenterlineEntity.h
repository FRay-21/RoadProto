#pragma once

#include "domain/alignment/HorizontalAlignmentBuilder.h"

#include "dbents.h"
#include "dbmain.h"

#include <vector>

class DnRoadCenterlineEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnRoadCenterlineEntity);

    DnRoadCenterlineEntity();

    void setAlignment(const roadproto::domain::alignment::HorizontalAlignment& alignment);
    const roadproto::domain::alignment::HorizontalAlignment& alignment() const;
    void setProperties(const roadproto::domain::alignment::RoadCenterlineProperties& properties);
    roadproto::domain::alignment::RoadCenterlineProperties properties() const;
    void dragStatus(const AcDb::DragStat status) override;
    bool rebuildWithCurveParameters(
        const roadproto::domain::alignment::HorizontalCurveParameters& parameters,
        std::size_t curveIndex = 0);
    bool rebuildWithCurveParameterList(
        const std::vector<roadproto::domain::alignment::HorizontalCurveParameters>& parameters);

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
    Acad::ErrorStatus subGetGripPoints(
        AcDbGripDataPtrArray& grips,
        const double curViewUnitSize,
        const int gripSize,
        const AcGeVector3d& curViewDir,
        const int bitflags) const override;
    Acad::ErrorStatus subMoveGripPointsAt(const AcDbIntArray& indices, const AcGeVector3d& offset) override;
    Acad::ErrorStatus subMoveGripPointsAt(
        const AcDbVoidPtrArray& gripAppData,
        const AcGeVector3d& offset,
        const int bitflags) override;
    Adesk::Boolean subCloneMeForDragging() override;

private:
    enum DragDataFlag {
        kCloneMeForDraggingCalled = 0x1,
        kUseDragCache = 0x2,
    };

    const roadproto::domain::alignment::HorizontalAlignment& drawAlignment() const;
    Acad::ErrorStatus moveGripPointIndices(
        const std::vector<std::size_t>& gripIndices,
        const AcGeVector3d& offset,
        bool previewOnly);
    bool rebuildFromStoredData();
    void drawText(AcGiWorldDraw* worldDraw, const roadproto::domain::alignment::AlignmentPoint2d& point, const std::wstring& text, double height) const;

    roadproto::domain::alignment::HorizontalAlignment alignment_;
    mutable int dragDataFlags_ = 0;
    mutable roadproto::domain::alignment::HorizontalAlignment dragAlignment_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeRoadCenterlineEntityClass();
void uninitializeRoadCenterlineEntityClass();

} // namespace roadproto::cad_adapter::objectarx
