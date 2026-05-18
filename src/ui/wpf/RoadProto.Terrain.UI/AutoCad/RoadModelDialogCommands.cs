using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class RoadModelDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoRoadModelDialog_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_SECTION_ROAD_MODEL_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowRoadModelDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto road model dialog pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto road model dialog request path is empty.");
                return;
            }

            var request = RoadModelDialogFile.ReadRequest(requestPath);
            if (string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                editor.WriteMessage("\nRoadProto road model dialog response path is empty.");
                return;
            }

            var window = new RoadModelWindow(request);
            var dialogResult = window.ShowDialog();
            var response = window.Response ?? new RoadModelDialogResponse
            {
                Accepted = dialogResult == true,
                Handle = request.Handle,
                RoadCenterlineHandle = request.RoadCenterlineHandle,
                ProfileVerticalCurveHandle = request.ProfileVerticalCurveHandle,
                SampleInterval = request.SampleInterval,
                Assignments = request.Assignments,
            };

            RoadModelDialogFile.WriteResponse(request.ResponsePath, response);
            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute($"RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n", true, false, true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto road model WPF dialog failed: {error.Message}");
        }
    }
}
