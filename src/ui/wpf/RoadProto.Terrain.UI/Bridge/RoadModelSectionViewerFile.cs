using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class RoadModelSectionViewerFile
{
    public static RoadModelSectionViewerRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        var request = new RoadModelSectionViewerRequest
        {
            Handle = Get(values, "handle"),
            RoadCenterlineHandle = Get(values, "roadCenterlineHandle"),
        };

        var previewCount = Math.Max(0, GetInt(values, "previewCount"));
        for (var i = 0; i < previewCount; i++)
        {
            var previewPrefix = $"preview.{i}";
            var preview = new RoadModelSectionViewerPreviewDto
            {
                Station = GetDouble(values, $"{previewPrefix}.station"),
                StationLabel = Get(values, $"{previewPrefix}.stationLabel"),
                StatusMessage = Get(values, $"{previewPrefix}.statusMessage"),
                HasGroundLine = GetBool(values, $"{previewPrefix}.hasGroundLine"),
            };

            var segmentCount = Math.Max(0, GetInt(values, $"{previewPrefix}.segmentCount"));
            for (var j = 0; j < segmentCount; j++)
            {
                var segmentPrefix = $"{previewPrefix}.segment.{j}";
                var segment = new RoadModelSectionViewerSegmentDto
                {
                    Kind = ParseKind(Get(values, $"{segmentPrefix}.kind")),
                    Label = Get(values, $"{segmentPrefix}.label"),
                    ColorR = GetInt(values, $"{segmentPrefix}.colorR"),
                    ColorG = GetInt(values, $"{segmentPrefix}.colorG"),
                    ColorB = GetInt(values, $"{segmentPrefix}.colorB"),
                };

                var pointCount = Math.Max(0, GetInt(values, $"{segmentPrefix}.pointCount"));
                for (var k = 0; k < pointCount; k++)
                {
                    var pointPrefix = $"{segmentPrefix}.point.{k}";
                    segment.Points.Add(new RoadModelSectionViewerPointDto
                    {
                        Offset = GetDouble(values, $"{pointPrefix}.offset"),
                        Elevation = GetDouble(values, $"{pointPrefix}.elevation"),
                    });
                }

                preview.Segments.Add(segment);
            }

            request.Previews.Add(preview);
        }

        return request;
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
        => int.TryParse(Get(values, key), NumberStyles.Integer, CultureInfo.InvariantCulture, out var value)
            ? value
            : fallback;

    private static double GetDouble(Dictionary<string, string> values, string key, double fallback = 0.0)
        => double.TryParse(Get(values, key), NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
            ? value
            : fallback;

    private static bool GetBool(Dictionary<string, string> values, string key)
    {
        var value = Get(values, key);
        return value == "1" || value.Equals("true", StringComparison.OrdinalIgnoreCase);
    }

    private static RoadModelSectionViewerSegmentKind ParseKind(string value)
        => value switch
        {
            "Slope" => RoadModelSectionViewerSegmentKind.Slope,
            "Ground" => RoadModelSectionViewerSegmentKind.Ground,
            "PavementLayer" => RoadModelSectionViewerSegmentKind.PavementLayer,
            _ => RoadModelSectionViewerSegmentKind.Subgrade,
        };

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
