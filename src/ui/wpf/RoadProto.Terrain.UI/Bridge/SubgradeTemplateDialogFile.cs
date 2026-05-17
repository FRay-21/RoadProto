using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class SubgradeTemplateDialogFile
{
    public static SubgradeTemplateDialogRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        var request = new SubgradeTemplateDialogRequest
        {
            Handle = Get(values, "handle"),
            ResponsePath = Get(values, "responsePath"),
            InsertionX = GetDouble(values, "insertionX"),
            InsertionY = GetDouble(values, "insertionY"),
            InsertionZ = GetDouble(values, "insertionZ"),
            TemplateName = Get(values, "templateName", "默认路基模板"),
            DisplayScale = GetDouble(values, "displayScale", 10.0),
            RoadGrade = ParseEnum(Get(values, "roadGrade", "Expressway"), SubgradeRoadGrade.Expressway),
            RoadCenterlineHandle = Get(values, "roadCenterlineHandle"),
        };

        var componentCount = Math.Max(0, GetInt(values, "componentCount"));
        for (var i = 0; i < componentCount; i++)
        {
            var prefix = $"component.{i}";
            request.Components.Add(ReadComponent(values, prefix));
        }

        return request;
    }

    public static void WriteResponse(string path, SubgradeTemplateDialogResponse response)
    {
        var lines = new List<string>
        {
            Write("accepted", response.Accepted),
            Write("handle", response.Handle),
            Write("insertionX", response.InsertionX),
            Write("insertionY", response.InsertionY),
            Write("insertionZ", response.InsertionZ),
            Write("templateName", response.TemplateName),
            Write("displayScale", response.DisplayScale),
            Write("roadGrade", response.RoadGrade.ToString()),
            Write("roadCenterlineHandle", response.RoadCenterlineHandle),
            Write("componentCount", response.Components.Count),
        };

        for (var i = 0; i < response.Components.Count; i++)
        {
            WriteComponent(lines, $"component.{i}", response.Components[i]);
        }

        File.WriteAllLines(path, lines, new UTF8Encoding(false));
    }

    private static SubgradeComponentDto ReadComponent(Dictionary<string, string> values, string prefix)
        => new()
        {
            Side = ParseEnum(Get(values, $"{prefix}.side", "Right"), SubgradeSide.Right),
            Type = ParseEnum(Get(values, $"{prefix}.type", "TravelLane"), SubgradeComponentType.TravelLane),
            Width = GetDouble(values, $"{prefix}.width"),
            Height = GetDouble(values, $"{prefix}.height"),
            FixedSlope = GetDouble(values, $"{prefix}.fixedSlope"),
            SlopeMode = ParseEnum(Get(values, $"{prefix}.slopeMode", "Fixed"), SubgradeSlopeMode.Fixed),
            ColorR = GetInt(values, $"{prefix}.colorR", 120),
            ColorG = GetInt(values, $"{prefix}.colorG", 120),
            ColorB = GetInt(values, $"{prefix}.colorB", 120),
            WideningTable = ReadRows(values, $"{prefix}.widening"),
            VariableSlopeTable = ReadRows(values, $"{prefix}.slopeTable"),
            PavementLayerLinked = GetBool(values, $"{prefix}.pavementLayerLinked"),
            PavementLayerHandle = Get(values, $"{prefix}.pavementLayerHandle"),
            PavementLayerThickness = GetDouble(values, $"{prefix}.pavementLayerThickness"),
        };

    private static void WriteComponent(List<string> lines, string prefix, SubgradeComponentDto component)
    {
        lines.Add(Write($"{prefix}.side", component.Side.ToString()));
        lines.Add(Write($"{prefix}.type", component.Type.ToString()));
        lines.Add(Write($"{prefix}.width", component.Width));
        lines.Add(Write($"{prefix}.height", component.Height));
        lines.Add(Write($"{prefix}.fixedSlope", component.FixedSlope));
        lines.Add(Write($"{prefix}.slopeMode", component.SlopeMode.ToString()));
        lines.Add(Write($"{prefix}.colorR", component.ColorR));
        lines.Add(Write($"{prefix}.colorG", component.ColorG));
        lines.Add(Write($"{prefix}.colorB", component.ColorB));
        WriteRows(lines, $"{prefix}.widening", component.WideningTable);
        WriteRows(lines, $"{prefix}.slopeTable", component.VariableSlopeTable);
        lines.Add(Write($"{prefix}.pavementLayerLinked", component.PavementLayerLinked));
        lines.Add(Write($"{prefix}.pavementLayerHandle", component.PavementLayerHandle));
        lines.Add(Write($"{prefix}.pavementLayerThickness", component.PavementLayerThickness));
    }

    private static List<SubgradeStationValueDto> ReadRows(Dictionary<string, string> values, string prefix)
    {
        var rows = new List<SubgradeStationValueDto>();
        var count = Math.Max(0, GetInt(values, $"{prefix}Count"));
        for (var i = 0; i < count; i++)
        {
            rows.Add(new SubgradeStationValueDto
            {
                Station = GetDouble(values, $"{prefix}.{i}.station"),
                Value = GetDouble(values, $"{prefix}.{i}.value"),
            });
        }

        return rows;
    }

    private static void WriteRows(List<string> lines, string prefix, List<SubgradeStationValueDto> rows)
    {
        lines.Add(Write($"{prefix}Count", rows.Count));
        for (var i = 0; i < rows.Count; i++)
        {
            lines.Add(Write($"{prefix}.{i}.station", rows[i].Station));
            lines.Add(Write($"{prefix}.{i}.value", rows[i].Value));
        }
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

    private static bool GetBool(Dictionary<string, string> values, string key)
    {
        var value = Get(values, key);
        return value.Equals("1", StringComparison.OrdinalIgnoreCase)
            || value.Equals("true", StringComparison.OrdinalIgnoreCase);
    }

    private static T ParseEnum<T>(string value, T fallback)
        where T : struct
        => Enum.TryParse<T>(value, true, out var result) ? result : fallback;

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
