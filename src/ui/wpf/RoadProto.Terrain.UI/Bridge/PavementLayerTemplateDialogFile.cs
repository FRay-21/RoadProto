using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class PavementLayerTemplateDialogFile
{
    public static PavementLayerTemplateDialogRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        var request = new PavementLayerTemplateDialogRequest
        {
            Handle = Get(values, "handle"),
            ResponsePath = Get(values, "responsePath"),
            InsertionX = GetDouble(values, "insertionX"),
            InsertionY = GetDouble(values, "insertionY"),
            InsertionZ = GetDouble(values, "insertionZ"),
            TemplateName = Get(values, "templateName", "路面结构层模板"),
            DisplayScale = GetDouble(values, "displayScale", 10.0),
            PreviewWidth = GetDouble(values, "previewWidth", 3.75),
            DisplayMode = ParseEnum(Get(values, "displayMode", "Color"), PavementLayerTemplateDisplayMode.Color),
        };

        var layerCount = Math.Max(0, GetInt(values, "layerCount"));
        for (var i = 0; i < layerCount; i++)
        {
            request.Layers.Add(ReadLayer(values, $"layer.{i}", i));
        }

        if (request.Layers.Count == 0)
        {
            request.Layers.AddRange(PavementLayerTemplateLabels.DefaultLayers());
        }

        return request;
    }

    public static void WriteResponse(string path, PavementLayerTemplateDialogResponse response)
    {
        var lines = new List<string>
        {
            Write("accepted", response.Accepted),
            Write("handle", response.Handle),
        };

        if (response.Accepted)
        {
            lines.Add(Write("insertionX", response.InsertionX));
            lines.Add(Write("insertionY", response.InsertionY));
            lines.Add(Write("insertionZ", response.InsertionZ));
            lines.Add(Write("templateName", response.TemplateName));
            lines.Add(Write("displayScale", response.DisplayScale));
            lines.Add(Write("previewWidth", response.PreviewWidth));
            lines.Add(Write("displayMode", response.DisplayMode.ToString()));
            lines.Add(Write("layerCount", response.Layers.Count));

            for (var i = 0; i < response.Layers.Count; i++)
            {
                WriteLayer(lines, $"layer.{i}", response.Layers[i]);
            }
        }

        File.WriteAllLines(path, lines, new UTF8Encoding(false));
    }

    private static PavementLayerTemplateLayerDto ReadLayer(Dictionary<string, string> values, string prefix, int index)
    {
        var type = ParseEnum(Get(values, $"{prefix}.type", "UpperSurface"), PavementLayerType.UpperSurface);
        var thickness = GetDouble(values, $"{prefix}.thickness", 0.04);
        var defaultColor = PavementLayerTemplateLabels.DefaultColorForLayerIndex(index);
        return new PavementLayerTemplateLayerDto
        {
            Type = type,
            Name = Get(values, $"{prefix}.name", PavementLayerTemplateLabels.LayerTypeLabel(type)),
            UniformThickness = GetBool(values, $"{prefix}.uniformThickness", true),
            Thickness = thickness,
            InnerThickness = GetDouble(values, $"{prefix}.innerThickness", thickness),
            OuterThickness = GetDouble(values, $"{prefix}.outerThickness", thickness),
            InnerWidening = GetDouble(values, $"{prefix}.innerWidening"),
            OuterWidening = GetDouble(values, $"{prefix}.outerWidening"),
            InnerSlope = GetDouble(values, $"{prefix}.innerSlope"),
            OuterSlope = GetDouble(values, $"{prefix}.outerSlope"),
            ColorR = GetInt(values, $"{prefix}.colorR", defaultColor.R),
            ColorG = GetInt(values, $"{prefix}.colorG", defaultColor.G),
            ColorB = GetInt(values, $"{prefix}.colorB", defaultColor.B),
            HatchPattern = PavementLayerTemplateLabels.NormalizeHatchPattern(Get(values, $"{prefix}.hatchPattern", "SOLID")),
        };
    }

    private static void WriteLayer(List<string> lines, string prefix, PavementLayerTemplateLayerDto layer)
    {
        lines.Add(Write($"{prefix}.type", layer.Type.ToString()));
        lines.Add(Write($"{prefix}.name", layer.Name));
        lines.Add(Write($"{prefix}.uniformThickness", layer.UniformThickness));
        lines.Add(Write($"{prefix}.thickness", layer.Thickness));
        lines.Add(Write($"{prefix}.innerThickness", layer.InnerThickness));
        lines.Add(Write($"{prefix}.outerThickness", layer.OuterThickness));
        lines.Add(Write($"{prefix}.innerWidening", layer.InnerWidening));
        lines.Add(Write($"{prefix}.outerWidening", layer.OuterWidening));
        lines.Add(Write($"{prefix}.innerSlope", layer.InnerSlope));
        lines.Add(Write($"{prefix}.outerSlope", layer.OuterSlope));
        lines.Add(Write($"{prefix}.colorR", ClampColor(layer.ColorR)));
        lines.Add(Write($"{prefix}.colorG", ClampColor(layer.ColorG)));
        lines.Add(Write($"{prefix}.colorB", ClampColor(layer.ColorB)));
        lines.Add(Write($"{prefix}.hatchPattern", PavementLayerTemplateLabels.NormalizeHatchPattern(layer.HatchPattern)));
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

    private static bool GetBool(Dictionary<string, string> values, string key, bool fallback = false)
    {
        var value = Get(values, key, fallback ? "1" : "0");
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

    private static int ClampColor(int value)
        => Math.Max(0, Math.Min(255, value));

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
