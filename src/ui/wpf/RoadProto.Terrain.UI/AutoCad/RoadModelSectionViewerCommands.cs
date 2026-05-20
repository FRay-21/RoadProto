using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class RoadModelSectionViewerCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoRoadModelSectionViewer_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_SECTION_ROAD_MODEL_VIEW_SECTION_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowRoadModelSectionViewerDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto road model section viewer pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto road model section viewer request path is empty.");
                return;
            }

            var request = RoadModelSectionViewerFile.ReadRequest(requestPath);
            var window = new RoadModelSectionViewerWindow(request);
            window.ShowDialog();
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto road model section viewer failed: {error.Message}");
        }
    }
}
