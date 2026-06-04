using Autodesk.AutoCAD.Runtime;
using Autodesk.AutoCAD.Windows;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class AgentAssistantCommands
{
    private static PaletteSet? palette;

    [CommandMethod("RD_AI_ASSISTANT_OPEN", CommandFlags.Session)]
    public void OpenAgentAssistant()
    {
        if (palette == null)
        {
            palette = new PaletteSet("RoadProto Agent")
            {
                Style = PaletteSetStyles.ShowCloseButton
                    | PaletteSetStyles.ShowAutoHideButton
                    | PaletteSetStyles.ShowPropertiesMenu,
                DockEnabled = DockSides.Right | DockSides.Left,
                MinimumSize = new System.Drawing.Size(360, 480),
                Size = new System.Drawing.Size(420, 720),
            };
            palette.AddVisual("Agent", new AgentAssistantControl());
        }

        palette.Visible = true;
        palette.Dock = DockSides.Right;
    }
}
