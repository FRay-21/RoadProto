using Autodesk.AutoCAD.EditorInput;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.AlignmentDialogCommands))]

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class AlignmentDialogCommands
{
    [CommandMethod("RD_ALIGN_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowAlignmentDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        var prompt = new PromptStringOptions("\nRoadProto alignment dialog request file: ")
        {
            AllowSpaces = true,
        };
        var pathResult = editor.GetString(prompt);
        if (pathResult.Status != PromptStatus.OK || string.IsNullOrWhiteSpace(pathResult.StringResult))
        {
            return;
        }

        try
        {
            var requestPath = pathResult.StringResult.Trim().Trim('"');
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
