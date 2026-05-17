using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class ProfileGradeGraphDialogFile
{
    public static ProfileGradeGraphDialogRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        return new ProfileGradeGraphDialogRequest
        {
            Handle = Get(values, "handle"),
            ResponsePath = Get(values, "responsePath"),
            RoadCenterlineHandle = Get(values, "roadCenterlineHandle"),
            TerrainTinHandle = Get(values, "terrainTinHandle"),
            GraphName = Get(values, "graphName", "拉坡图"),
            SourceType = ParseSourceType(Get(values, "sourceType", "DmxFile")),
            DmxFilePath = Get(values, "dmxFilePath"),
            GroundLineColorIndex = GetInt(values, "groundLineColorIndex", 4),
            GroundLineWidth = GetDouble(values, "groundLineWidth", 1.0),
            GroundLinePrecision = GetDouble(values, "groundLinePrecision", 10.0),
            VerticalScale = GetDouble(values, "verticalScale", 10.0),
            GridSpacing = GetDouble(values, "gridSpacing", 10.0),
            SampleCount = GetInt(values, "sampleCount", 0),
        };
    }

    public static void WriteResponse(string path, ProfileGradeGraphDialogResponse response)
    {
        var lines = new List<string>
        {
            Write("accepted", response.Accepted),
            Write("handle", response.Handle),
            Write("graphName", response.GraphName),
            Write("groundLineColorIndex", response.GroundLineColorIndex),
            Write("groundLineWidth", response.GroundLineWidth),
            Write("groundLinePrecision", response.GroundLinePrecision),
            Write("verticalScale", response.VerticalScale),
            Write("gridSpacing", response.GridSpacing),
            Write("updateGroundLineRequested", response.UpdateGroundLineRequested),
        };

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

    private static ProfileGradeGraphSourceType ParseSourceType(string value)
        => value.Equals("TerrainTin", StringComparison.OrdinalIgnoreCase)
            ? ProfileGradeGraphSourceType.TerrainTin
            : ProfileGradeGraphSourceType.DmxFile;

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
