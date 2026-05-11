namespace RoadProto.Terrain.UI.Bridge;

public interface ITerrainTinBridge
{
    TerrainTinExtractSummaryDto Reextract(TerrainTinOptionsDto options);

    TerrainTinBuildResultDto BuildTin(TerrainTinOptionsDto options);

    TerrainTinBuildResultDto ToggleDisplayMode(string displayMode);
}
