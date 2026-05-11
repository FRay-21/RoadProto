using Autodesk.AutoCAD.EditorInput;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.ProfileGradeGraphDialogCommands))]

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class ProfileGradeGraphDialogCommands
{
    [CommandMethod("RD_PROFILE_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowProfileGradeGraphDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        var prompt = new PromptStringOptions("\nRoadProto profile grade graph dialog request file: ")
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
            var request = ProfileGradeGraphDialogFile.ReadRequest(requestPath);
            if (string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                editor.WriteMessage("\nRoadProto profile grade graph dialog response path is empty.");
                return;
            }

            var window = new ProfileGradeGraphWindow(request);
            var dialogResult = window.ShowDialog();
            var response = window.Response ?? new ProfileGradeGraphDialogResponse
            {
                Accepted = dialogResult == true,
                Handle = request.Handle,
                GraphName = request.GraphName,
                GroundLineColorIndex = request.GroundLineColorIndex,
                GroundLineWidth = request.GroundLineWidth,
                GroundLinePrecision = request.GroundLinePrecision,
                VerticalScale = request.VerticalScale,
                GridSpacing = request.GridSpacing,
                UpdateGroundLineRequested = false,
            };

            ProfileGradeGraphDialogFile.WriteResponse(request.ResponsePath, response);
            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute($"RD_PROFILE_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n", true, false, true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto profile grade graph WPF dialog failed: {error.Message}");
        }
    }
}
