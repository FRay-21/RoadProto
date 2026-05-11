using System.Windows;
using RoadProto.Terrain.UI.Bridge;
using RoadProto.Terrain.UI.ViewModels;

namespace RoadProto.Terrain.UI;

public partial class TerrainTinCreateWindow : Window
{
    public TerrainTinCreateWindow()
        : this(null)
    {
    }

    public TerrainTinCreateWindow(ITerrainTinBridge? bridge)
    {
        InitializeComponent();
        var viewModel = new TerrainTinCreateViewModel(bridge);
        viewModel.RequestClose += Close;
        DataContext = viewModel;
    }
}
