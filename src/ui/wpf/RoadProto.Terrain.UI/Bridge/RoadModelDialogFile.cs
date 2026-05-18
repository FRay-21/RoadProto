using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class RoadModelDialogFile
{
    private const string AssignmentStartStationKeyPattern = "assignment.{i}.startStation";

    public static RoadModelDialogRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        var responsePath = Get(values, "responsePath");
        if (string.IsNullOrWhiteSpace(responsePath))
        {
            throw new InvalidDataException("Road model dialog request must include responsePath.");
        }

        var request = new RoadModelDialogRequest
        {
            Handle = Get(values, "handle"),
            ResponsePath = responsePath,
            RoadCenterlineHandle = Get(values, "roadCenterlineHandle"),
            ProfileVerticalCurveHandle = Get(values, "profileVerticalCurveHandle"),
            SampleInterval = GetDouble(values, "sampleInterval", 10.0),
        };

        var assignmentCount = Math.Max(0, GetInt(values, "assignmentCount"));
        for (var i = 0; i < assignmentCount; i++)
        {
            request.Assignments.Add(ReadAssignment(values, $"assignment.{i}"));
        }

        return request;
    }

    public static void WriteResponse(string path, RoadModelDialogResponse response)
    {
        var lines = new List<string>
        {
            Write("accepted", response.Accepted),
            Write("handle", response.Handle),
            Write("roadCenterlineHandle", response.RoadCenterlineHandle),
            Write("profileVerticalCurveHandle", response.ProfileVerticalCurveHandle),
            Write("sampleInterval", response.SampleInterval),
            Write("assignmentCount", response.Assignments.Count),
        };

        for (var i = 0; i < response.Assignments.Count; i++)
        {
            WriteAssignment(lines, $"assignment.{i}", response.Assignments[i]);
        }

        File.WriteAllLines(path, lines, new UTF8Encoding(false));
    }

    private static RoadModelTemplateAssignmentDto ReadAssignment(Dictionary<string, string> values, string prefix)
        => new()
        {
            StartStation = GetDouble(values, $"{prefix}.startStation"),
            EndStation = GetDouble(values, $"{prefix}.endStation"),
            TemplateHandle = Get(values, $"{prefix}.templateHandle"),
            TemplateName = Get(values, $"{prefix}.templateName"),
        };

    private static void WriteAssignment(List<string> lines, string prefix, RoadModelTemplateAssignmentDto assignment)
    {
        lines.Add(Write($"{prefix}.startStation", assignment.StartStation));
        lines.Add(Write($"{prefix}.endStation", assignment.EndStation));
        lines.Add(Write($"{prefix}.templateHandle", assignment.TemplateHandle));
        lines.Add(Write($"{prefix}.templateName", assignment.TemplateName));
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
