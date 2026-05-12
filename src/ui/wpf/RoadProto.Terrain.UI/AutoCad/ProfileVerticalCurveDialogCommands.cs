using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.ProfileVerticalCurveDialogCommands))]

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class ProfileVerticalCurveDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoProfileVerticalCurveDialog_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_PROFILE_VERTICAL_CURVE_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowProfileVerticalCurveDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto profile vertical curve dialog pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto profile vertical curve dialog request path is empty.");
                return;
            }

            var request = ProfileVerticalCurveDialogFile.ReadRequest(requestPath);
            TryDeleteFile(requestPath);
            if (string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                editor.WriteMessage("\nRoadProto profile vertical curve dialog response path is empty.");
                return;
            }

            var window = new ProfileVerticalCurveWindow(request);
            window.ShowDialog();
            var response = window.Response ?? CreateRejectedResponse(request);

            ProfileVerticalCurveDialogFile.WriteResponse(request.ResponsePath, response);
            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute(
                $"RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n",
                true,
                false,
                true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto profile vertical curve WPF dialog failed: {error.Message}");
        }
    }

    private static void TryDeleteFile(string path)
    {
        try
        {
            if (!string.IsNullOrWhiteSpace(path) && File.Exists(path))
            {
                File.Delete(path);
            }
        }
        catch
        {
        }
    }

    private static ProfileVerticalCurveDialogResponse CreateRejectedResponse(ProfileVerticalCurveDialogRequest request)
        => new()
        {
            Accepted = false,
            Handle = request.Handle,
            ProfileGraphHandle = request.ProfileGraphHandle,
            Name = request.Name,
            StartStation = request.StartStation,
            StartElevation = request.StartElevation,
            EndStation = request.EndStation,
            EndElevation = request.EndElevation,
            CurrentPviIndex = request.CurrentPviIndex,
            Pvis = ClonePvis(request.Pvis),
        };

    private static List<ProfileVerticalCurvePviDto> ClonePvis(IEnumerable<ProfileVerticalCurvePviDto>? pvis)
    {
        var clone = new List<ProfileVerticalCurvePviDto>();
        if (pvis == null)
        {
            return clone;
        }

        foreach (var pvi in pvis)
        {
            clone.Add(new ProfileVerticalCurvePviDto
            {
                Station = pvi.Station,
                Elevation = pvi.Elevation,
                Radius = pvi.Radius,
            });
        }

        return clone;
    }
}
