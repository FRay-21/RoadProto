#pragma once

#include "domain/terrain/TerrainTinData.h"

#include "dbmain.h"
#include "dbents.h"
#include "dbcolor.h"

class DnTerrainTinEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnTerrainTinEntity);

    DnTerrainTinEntity();

    void setTinResult(const roadproto::domain::terrain::TinBuildResult& result);
    const std::vector<roadproto::domain::terrain::TinPoint>& getPoints() const;
    const std::vector<roadproto::domain::terrain::TinTriangle>& getTriangles() const;
    const std::vector<std::pair<std::size_t, std::size_t>>& getBoundaryEdges() const;
    const roadproto::domain::terrain::TinBuildOptions& buildOptions() const;
    const roadproto::domain::terrain::TinExtractOptions& extractOptions() const;
    double minElevation() const;
    double maxElevation() const;
    roadproto::domain::terrain::TerrainTinDisplayMode displayMode() const;
    void setDisplayMode(roadproto::domain::terrain::TerrainTinDisplayMode mode);
    void setBuildOptions(const roadproto::domain::terrain::TinBuildOptions& options);

    bool SampleElevation(double x, double y, double& z) const;
    bool FindTriangle(double x, double y, roadproto::domain::terrain::TinTriangle& triangle) const;
    bool RebuildFromSourceObjects();
    bool isDirty() const;

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;

private:
    std::vector<std::pair<std::size_t, std::size_t>> boundaryEdges() const;
    void rebuildTriangleEdges();
    AcCmEntityColor colorForElevation(double elevation) const;

    std::vector<roadproto::domain::terrain::TinPoint> points_;
    std::vector<roadproto::domain::terrain::TinTriangle> triangles_;
    std::vector<std::pair<std::size_t, std::size_t>> boundaryEdges_;
    std::vector<std::pair<std::size_t, std::size_t>> triangleEdges_;
    roadproto::domain::terrain::TinBuildOptions buildOptions_;
    roadproto::domain::terrain::TinExtractOptions extractOptions_;
    roadproto::domain::terrain::TerrainTinDisplayMode displayMode_ =
        roadproto::domain::terrain::TerrainTinDisplayMode::ColoredTriangles;
    double minElevation_ = 0.0;
    double maxElevation_ = 0.0;
    bool isDirty_ = false;
};

namespace roadproto::cad_adapter::objectarx {

void initializeTerrainTinEntityClass();
void uninitializeTerrainTinEntityClass();

} // namespace roadproto::cad_adapter::objectarx
