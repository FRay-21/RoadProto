using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class SlopeTemplateDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoSlopeTemplateDialog_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_SECTION_SLOPE_TEMPLATE_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowSlopeTemplateDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto slope template dialog pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto slope template dialog request path is empty.");
                return;
            }

            var request = SlopeTemplateDialogFile.ReadRequest(requestPath);
            if (string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                editor.WriteMessage("\nRoadProto slope template dialog response path is empty.");
                return;
            }

            var window = new SlopeTemplateWindow(request);
            var dialogResult = window.ShowDialog();
            var response = window.Response ?? new SlopeTemplateDialogResponse
            {
                Accepted = dialogResult == true,
                Handle = request.Handle,
                InsertionX = request.InsertionX,
                InsertionY = request.InsertionY,
                InsertionZ = request.InsertionZ,
                TemplateName = request.TemplateName,
                DisplayScale = request.DisplayScale,
                Kind = request.Kind,
                StopAtGround = request.StopAtGround,
                RepeatLastGroupUntilGround = request.RepeatLastGroupUntilGround,
                Components = request.Components,
            };

            SlopeTemplateDialogFile.WriteResponse(request.ResponsePath, response);
            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute($"RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n", true, false, true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto slope template WPF dialog failed: {error.Message}");
        }
    }
}
