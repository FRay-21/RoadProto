using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.PavementLayerTemplateDialogCommands))]

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class PavementLayerTemplateDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoPavementLayerTemplateDialog_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_SECTION_PAVEMENT_LAYER_TEMPLATE_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowPavementLayerTemplateDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto pavement layer template dialog pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto pavement layer template dialog request path is empty.");
                return;
            }

            var request = PavementLayerTemplateDialogFile.ReadRequest(requestPath);
            if (string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                editor.WriteMessage("\nRoadProto pavement layer template dialog response path is empty.");
                return;
            }

            var window = new PavementLayerTemplateWindow(request);
            window.ApplyRequested += (_, response) =>
            {
                var applyResponsePath = CreateApplyResponsePath();
                PavementLayerTemplateDialogFile.WriteResponse(applyResponsePath, response);
                SendApplyCommand(document, applyResponsePath);
            };

            var dialogResult = window.ShowDialog();
            var response = window.Response ?? CreateCancelledResponse(request, dialogResult == true);
            PavementLayerTemplateDialogFile.WriteResponse(request.ResponsePath, response);
            SendApplyCommand(document, request.ResponsePath);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto pavement layer template WPF dialog failed: {error.Message}");
        }
    }

    private static PavementLayerTemplateDialogResponse CreateCancelledResponse(
        PavementLayerTemplateDialogRequest request,
        bool accepted)
        => new()
        {
            Accepted = accepted,
            Handle = request.Handle,
            InsertionX = request.InsertionX,
            InsertionY = request.InsertionY,
            InsertionZ = request.InsertionZ,
            TemplateName = request.TemplateName,
            DisplayScale = request.DisplayScale,
            PreviewWidth = request.PreviewWidth,
            Layers = request.Layers,
        };

    private static void SendApplyCommand(Autodesk.AutoCAD.ApplicationServices.Document document, string responsePath)
    {
        var responseCommandPath = responsePath.Replace('\\', '/');
        document.SendStringToExecute(
            $"RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n",
            true,
            false,
            true);
    }

    private static string CreateApplyResponsePath()
        => Path.Combine(
            Path.GetTempPath(),
            $"RoadProtoPavementLayerTemplateApply_{Process.GetCurrentProcess().Id}_{System.Guid.NewGuid():N}.response");
}
