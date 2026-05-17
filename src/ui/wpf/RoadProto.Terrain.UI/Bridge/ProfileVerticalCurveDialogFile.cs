using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class ProfileVerticalCurveDialogFile
{
    public static ProfileVerticalCurveDialogRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        var pviCount = Math.Max(0, GetInt(values, "pviCount", 0));
        var pvis = new List<ProfileVerticalCurvePviDto>(pviCount);
        for (var index = 0; index < pviCount; index++)
        {
            pvis.Add(new ProfileVerticalCurvePviDto
            {
                Station = GetDouble(values, $"pvi.{index}.station"),
                Elevation = GetDouble(values, $"pvi.{index}.elevation"),
                Radius = GetDouble(values, $"pvi.{index}.radius", 1000.0),
            });
        }

        return new ProfileVerticalCurveDialogRequest
        {
            Handle = Get(values, "handle"),
            ResponsePath = Get(values, "responsePath"),
            ProfileGraphHandle = Get(values, "profileGraphHandle"),
            Name = Get(values, "name", "竖曲线"),
            StartStation = GetDouble(values, "startStation"),
            StartElevation = GetDouble(values, "startElevation"),
            EndStation = GetDouble(values, "endStation"),
            EndElevation = GetDouble(values, "endElevation"),
            CurrentPviIndex = GetInt(values, "currentPviIndex", 0),
            Pvis = pvis,
        };
    }

    public static void WriteResponse(string path, ProfileVerticalCurveDialogResponse response)
    {
        var pvis = response.Pvis ?? [];
        var lines = new List<string>
        {
            Write("accepted", response.Accepted),
            Write("handle", response.Handle),
            Write("profileGraphHandle", response.ProfileGraphHandle),
            Write("name", response.Name),
            Write("startStation", response.StartStation),
            Write("startElevation", response.StartElevation),
            Write("endStation", response.EndStation),
            Write("endElevation", response.EndElevation),
            Write("currentPviIndex", response.CurrentPviIndex),
            Write("pviCount", pvis.Count),
        };

        for (var index = 0; index < pvis.Count; index++)
        {
            lines.Add(Write($"pvi.{index}.station", pvis[index].Station));
            lines.Add(Write($"pvi.{index}.elevation", pvis[index].Elevation));
            lines.Add(Write($"pvi.{index}.radius", pvis[index].Radius));
        }

        File.WriteAllLines(path, lines, new UTF8Encoding(false));
    }

    private static Dictionary<string, string> ReadValues(string path)
    {
        var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var rawLine in File.ReadAllLines(path, Encoding.UTF8))
        {
            var line = rawLine.TrimEnd('\r');
            if (line.Length == 0 || line[0] == '#')
            {
                continue;
            }

            var separator = line.IndexOf('=');
            if (separator <= 0)
            {
                continue;
            }

            var key = line.Substring(0, separator).TrimStart('\uFEFF');
            values[key] = Unescape(line.Substring(separator + 1));
        }

        return values;
    }

    private static string Get(Dictionary<string, string> values, string key, string fallback = "")
        => values.TryGetValue(key, out var value) ? value : fallback;

    private static int GetInt(Dictionary<string, string> values, string key, int fallback = 0)
        => int.TryParse(Get(values, key), NumberStyles.Integer, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private static double GetDouble(Dictionary<string, string> values, string key, double fallback = 0.0)
        => double.TryParse(Get(values, key), NumberStyles.Float, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private static string Write(string key, string value)
        => $"{key}={Escape(value)}";

    private static string Write(string key, bool value)
        => $"{key}={(value ? 1 : 0)}";

    private static string Write(string key, int value)
        => $"{key}={value.ToString(CultureInfo.InvariantCulture)}";

    private static string Write(string key, double value)
        => $"{key}={value.ToString("R", CultureInfo.InvariantCulture)}";

    private static string Escape(string value)
    {
        var builder = new StringBuilder();
        foreach (var ch in value ?? string.Empty)
        {
            if (ch is '%' or '\r' or '\n')
            {
                builder.Append('%');
                builder.Append(((int)ch).ToString("X2", CultureInfo.InvariantCulture));
            }
            else
            {
                builder.Append(ch);
            }
        }

        return builder.ToString();
    }

    private static string Unescape(string value)
    {
        var builder = new StringBuilder();
        for (var i = 0; i < value.Length; i++)
        {
            if (value[i] == '%' && i + 2 < value.Length
                && int.TryParse(value.Substring(i + 1, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var code))
            {
                builder.Append((char)code);
                i += 2;
            }
            else
            {
                builder.Append(value[i]);
            }
        }

        return builder.ToString();
    }
}
