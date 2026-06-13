using System;
using System.Threading.Tasks;
using Autodesk.AutoCAD.Runtime;
using Autodesk.AutoCAD.Windows;
using RoadProto.Terrain.UI.Services;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class AgentAssistantCommands
{
    private static PaletteSet? palette;
    private static AgentAssistantControl? agentControl;

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
                KeepFocus = true,
            };
            agentControl = new AgentAssistantControl();
            palette.AddVisual("Agent", agentControl, true);
            palette.Focused += (_, _) => agentControl?.FocusInputBox();
            palette.StateChanged += (_, _) =>
            {
                if (palette != null && !palette.Visible)
                {
                    StopOwnedBackend();
                }
            };
            palette.PaletteSetDestroy += (_, _) => StopOwnedBackendAndResetPalette();
        }

        palette.Visible = true;
        palette.Dock = DockSides.Right;
        palette.KeepFocus = true;
        palette.Focus();
        agentControl?.FocusInputBox();
        _ = EnsureBackendAsync();
    }

    public static void StopOwnedBackend()
    {
        AgentBackendProcess.StopOwnedProcess();
    }

    private static void StopOwnedBackendAndResetPalette()
    {
        StopOwnedBackend();
        palette = null;
        agentControl = null;
    }

    private static async Task EnsureBackendAsync()
    {
        try
        {
            agentControl?.Dispatcher.Invoke(() =>
                agentControl.ShowBackendStatus("正在启动本地 Agent 后端"));
            await AgentBackendProcess.StartOrAttachAsync().ConfigureAwait(false);
            agentControl?.Dispatcher.Invoke(() => agentControl.RefreshHealthInBackground());
        }
        catch (System.Exception error)
        {
            agentControl?.Dispatcher.Invoke(() =>
                agentControl.ShowBackendStatus($"Agent 后端启动失败：{error.Message}"));
        }
    }
}
