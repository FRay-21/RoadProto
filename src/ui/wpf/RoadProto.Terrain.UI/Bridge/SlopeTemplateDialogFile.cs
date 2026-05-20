using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class SlopeTemplateDialogFile
{
    public static SlopeTemplateDialogRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        var request = new SlopeTemplateDialogRequest
        {
            Handle = Get(values, "handle"),
            ResponsePath = Get(values, "responsePath"),
            InsertionX = GetDouble(values, "insertionX"),
            InsertionY = GetDouble(values, "insertionY"),
            InsertionZ = GetDouble(values, "insertionZ"),
            TemplateName = Get(values, "templateName", "边坡模板1"),
            DisplayScale = GetDouble(values, "displayScale", 10.0),
            Kind = ParseEnum(Get(values, "kind", "Fill"), SlopeTemplateKind.Fill),
            StopAtGround = GetBool(values, "stopAtGround"),
            RepeatLastGroupUntilGround = GetBool(values, "repeatLastGroupUntilGround"),
        };

        var componentCount = Math.Max(0, GetInt(values, "componentCount"));
        for (var i = 0; i < componentCount; i++)
        {
            request.Components.Add(ReadComponent(values, $"component.{i}"));
        }
        return request;
    }

    public static void WriteResponse(string path, SlopeTemplateDialogResponse response)
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
            Write("kind", response.Kind.ToString()),
            Write("stopAtGround", response.StopAtGround),
            Write("repeatLastGroupUntilGround", response.RepeatLastGroupUntilGround),
            Write("componentCount", response.Components.Count),
        };

        for (var i = 0; i < response.Components.Count; i++)
        {
            WriteComponent(lines, $"component.{i}", response.Components[i]);
        }
        File.WriteAllLines(path, lines, new UTF8Encoding(false));
    }

    private static SlopeComponentDto ReadComponent(Dictionary<string, string> values, string prefix)
        => new()
        {
            Type = ParseEnum(Get(values, $"{prefix}.type", "FillSlope"), SlopeComponentType.FillSlope),
            ConstraintMode = ParseEnum(Get(values, $"{prefix}.constraintMode", "SlopeAndHeight"), SlopeGeometryConstraintMode.SlopeAndHeight),
            Slope = GetDouble(values, $"{prefix}.slope", -1.0 / 1.5),
            Height = GetDouble(values, $"{prefix}.height", 4.0),
            Width = GetDouble(values, $"{prefix}.width", 6.0),
            GroundSearchHeightIncrement = GetDouble(values, $"{prefix}.groundSearchHeightIncrement"),
            ColorR = GetInt(values, $"{prefix}.colorR", 120),
            ColorG = GetInt(values, $"{prefix}.colorG", 120),
            ColorB = GetInt(values, $"{prefix}.colorB", 120),
        };

    private static void WriteComponent(List<string> lines, string prefix, SlopeComponentDto component)
    {
        lines.Add(Write($"{prefix}.type", component.Type.ToString()));
        lines.Add(Write($"{prefix}.constraintMode", component.ConstraintMode.ToString()));
        lines.Add(Write($"{prefix}.slope", component.Slope));
        lines.Add(Write($"{prefix}.height", component.Height));
        lines.Add(Write($"{prefix}.width", component.Width));
        lines.Add(Write($"{prefix}.groundSearchHeightIncrement", component.GroundSearchHeightIncrement));
        lines.Add(Write($"{prefix}.colorR", component.ColorR));
        lines.Add(Write($"{prefix}.colorG", component.ColorG));
        lines.Add(Write($"{prefix}.colorB", component.ColorB));
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
