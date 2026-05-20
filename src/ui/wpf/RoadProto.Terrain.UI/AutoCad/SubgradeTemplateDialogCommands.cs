using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class SubgradeTemplateDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoSubgradeTemplateDialog_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_SECTION_SUBGRADE_TEMPLATE_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowSubgradeTemplateDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto subgrade template dialog pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto subgrade template dialog request path is empty.");
                return;
            }

            var request = SubgradeTemplateDialogFile.ReadRequest(requestPath);
            if (string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                editor.WriteMessage("\nRoadProto subgrade template dialog response path is empty.");
                return;
            }

            var window = new SubgradeTemplateWindow(request);
            var dialogResult = window.ShowDialog();
            var response = window.Response ?? new SubgradeTemplateDialogResponse
            {
                Action = request.Action,
                PickComponentIndex = request.PickComponentIndex,
                Accepted = dialogResult == true,
                Handle = request.Handle,
                InsertionX = request.InsertionX,
                InsertionY = request.InsertionY,
                InsertionZ = request.InsertionZ,
                TemplateName = request.TemplateName,
                DisplayScale = request.DisplayScale,
                RoadGrade = request.RoadGrade,
                RoadCenterlineHandle = request.RoadCenterlineHandle,
                Components = request.Components,
            };

            SubgradeTemplateDialogFile.WriteResponse(request.ResponsePath, response);
            _ = response.Action;
            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute($"RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n", true, false, true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto subgrade template WPF dialog failed: {error.Message}");
        }
    }
}
