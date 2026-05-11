using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.AlignmentDialogCommands))]

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class AlignmentDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoAlignmentDialog_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_ALIGN_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowAlignmentDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto alignment dialog pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto alignment dialog request path is empty.");
                return;
            }

            var request = AlignmentDialogFile.ReadRequest(requestPath);
            if (string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                editor.WriteMessage("\nRoadProto alignment dialog response path is empty.");
                return;
            }

            var window = new AlignmentCenterlineWindow(request);
            var dialogResult = window.ShowDialog();
            var response = window.Response ?? new AlignmentDialogResponse
            {
                Action = AlignmentDialogAction.None,
                Accepted = dialogResult == true,
                Mode = request.Mode,
                Handle = request.Handle,
                DeleteOnCancel = request.DeleteOnCancel,
                CurveIndex = request.CurveIndex,
                RoadName = request.RoadName,
                RoadGradeIndex = request.RoadGradeIndex,
                LinkedTerrainEnabled = request.LinkedTerrainEnabled,
                LinkedTerrainHandle = request.LinkedTerrainHandle,
                StationLabelInterval = request.StationLabelInterval,
                TangentIn = request.TangentIn,
                TangentOut = request.TangentOut,
                Ls1 = request.Ls1,
                Radius = request.Radius,
                Ls2 = request.Ls2,
                CurveParameters = request.CurveParameters,
            };

            AlignmentDialogFile.WriteResponse(request.ResponsePath, response);
            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute($"RD_ALIGN_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n", true, false, true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto alignment WPF dialog failed: {error.Message}");
        }
    }
}
