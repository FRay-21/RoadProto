#pragma once

#include "domain/cross_section/RoadModel.h"

#include "dbcolor.h"
#include "dbents.h"
#include "dbmain.h"

class DnRoadModelEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnRoadModelEntity);

    DnRoadModelEntity();

    void setRoadModelData(const roadproto::domain::cross_section::RoadModelData& data);
    const roadproto::domain::cross_section::RoadModelData& roadModelData() const;

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;

private:
    roadproto::domain::cross_section::RoadModelData data_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeRoadModelEntityClass();
void uninitializeRoadModelEntityClass();

} // namespace roadproto::cad_adapter::objectarx
