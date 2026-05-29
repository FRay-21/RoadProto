using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class SectionDrawingConfigDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoSectionDrawingConfig_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_SECTION_DRAWING_CONFIG_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowSectionDrawingConfigDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto section drawing config pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto section drawing config request path is empty.");
                return;
            }

            var request = SectionDrawingConfigDialogFile.ReadRequest(requestPath);
            if (string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                editor.WriteMessage("\nRoadProto section drawing config response path is empty.");
                return;
            }

            var window = new SectionDrawingConfigWindow(request);
            var dialogResult = window.ShowDialog();
            var response = window.Response ?? CreateCancelledResponse(request, dialogResult == true);
            SectionDrawingConfigDialogFile.WriteResponse(request.ResponsePath, response);

            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute(
                $"RD_SECTION_DRAWING_CONFIG_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n",
                true,
                false,
                true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto section drawing config WPF dialog failed: {error.Message}");
        }
    }

    private static SectionDrawingConfigResponse CreateCancelledResponse(
        SectionDrawingConfigRequest request,
        bool accepted)
        => new()
        {
            Action = SectionDrawingConfigAction.None,
            Accepted = accepted,
            DrawingHandle = request.DrawingHandle,
            RoadModelHandle = request.RoadModelHandle,
            ResponsePath = request.ResponsePath,
            ConfigPath = request.ConfigPath,
            ComponentOptions = request.ComponentOptions,
            PavementRows = request.PavementRows,
            ClearTableRows = request.ClearTableRows,
        };
}
