#pragma once

#include "domain/profile/ProfileVerticalCurveModel.h"

#include "dbents.h"
#include "dbmain.h"

class DnProfileVerticalCurveEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnProfileVerticalCurveEntity);

    DnProfileVerticalCurveEntity();

    void setCurveData(const roadproto::domain::profile::ProfileVerticalCurveData& data);
    const roadproto::domain::profile::ProfileVerticalCurveData& curveData() const;

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;

private:
    roadproto::domain::profile::ProfileVerticalCurveData curveData_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeProfileVerticalCurveEntityClass();
void uninitializeProfileVerticalCurveEntityClass();

} // namespace roadproto::cad_adapter::objectarx
