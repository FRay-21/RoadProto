#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include "dbents.h"
#include "dbmain.h"

#include <vector>

struct ProfileGradeGraphDrawingFrame {
    bool valid = false;
    double minStation = 0.0;
    double baseElevation = 0.0;
    double verticalScale = 10.0;
    AcGePoint3d insertionPoint;
    AcGeVector3d xAxis;
    AcGeVector3d yAxis;
};

class DnProfileGradeGraphEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnProfileGradeGraphEntity);

    DnProfileGradeGraphEntity();

    void setGraphData(const roadproto::domain::profile::ProfileGradeGraphData& data);
    const roadproto::domain::profile::ProfileGradeGraphData& graphData() const;
    void setInsertionPoint(const AcGePoint3d& point);
    AcGePoint3d insertionPoint() const;
    ProfileGradeGraphDrawingFrame drawingFrame() const;
    void setProperties(const roadproto::domain::profile::ProfileGradeGraphProperties& properties);
    bool replaceGroundSamples(const std::vector<roadproto::domain::profile::ProfileGroundSample>& samples);

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;

private:
    roadproto::domain::profile::ProfileGradeGraphData graphData_;
    AcGePoint3d insertionPoint_;
    AcGeVector3d xAxis_;
    AcGeVector3d yAxis_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeProfileGradeGraphEntityClass();
void uninitializeProfileGradeGraphEntityClass();

} // namespace roadproto::cad_adapter::objectarx
