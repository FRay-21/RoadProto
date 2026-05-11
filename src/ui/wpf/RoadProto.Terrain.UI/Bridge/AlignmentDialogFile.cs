using System;
using System.Globalization;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class AlignmentDialogFile
{
    public static AlignmentDialogRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        var curveIndex = GetInt(values, "curveIndex");
        var curveParameters = ReadCurveParameters(values);
        var currentParameters = CurveParameterAt(curveParameters, curveIndex);
        return new AlignmentDialogRequest
        {
            Mode = ParseMode(Get(values, "mode", "full")),
            Handle = Get(values, "handle"),
            ResponsePath = Get(values, "responsePath"),
            DeleteOnCancel = GetBool(values, "deleteOnCancel"),
            CurveIndex = curveIndex,
            RoadName = Get(values, "roadName", "K1"),
            RoadGradeIndex = GetInt(values, "roadGradeIndex", 9),
            LinkedTerrainEnabled = GetBool(values, "linkedTerrainEnabled"),
            LinkedTerrainHandle = Get(values, "linkedTerrainHandle"),
            StationLabelInterval = GetDouble(values, "stationLabelInterval", 100.0),
            TangentIn = currentParameters.TangentIn,
            TangentOut = currentParameters.TangentOut,
            Ls1 = currentParameters.Ls1,
            Radius = currentParameters.Radius,
            Ls2 = currentParameters.Ls2,
            CurveParameters = curveParameters,
        };
    }

    public static void WriteResponse(string path, AlignmentDialogResponse response)
    {
        var curveParameters = response.CurveParameters.Count > 0
            ? response.CurveParameters
            : new List<AlignmentCurveParameterDto>
            {
                new()
                {
                    TangentIn = response.TangentIn,
                    TangentOut = response.TangentOut,
                    Ls1 = response.Ls1,
                    Radius = response.Radius,
                    Ls2 = response.Ls2,
                },
            };
        var currentParameters = CurveParameterAt(curveParameters, response.CurveIndex);
        var lines = new List<string>
        {
            Write("accepted", response.Accepted),
            Write("mode", ModeText(response.Mode)),
            Write("handle", response.Handle),
            Write("deleteOnCancel", response.DeleteOnCancel),
            Write("curveIndex", response.CurveIndex),
            Write("roadName", response.RoadName),
            Write("roadGradeIndex", response.RoadGradeIndex),
            Write("linkedTerrainEnabled", response.LinkedTerrainEnabled),
            Write("linkedTerrainHandle", response.LinkedTerrainHandle),
            Write("stationLabelInterval", response.StationLabelInterval),
            Write("tangentIn", currentParameters.TangentIn),
            Write("tangentOut", currentParameters.TangentOut),
            Write("ls1", currentParameters.Ls1),
            Write("radius", currentParameters.Radius),
            Write("ls2", currentParameters.Ls2),
            Write("curveCount", curveParameters.Count),
        };

        for (var i = 0; i < curveParameters.Count; i++)
        {
            var curve = curveParameters[i];
            lines.Add(Write(CurveKey(i, "tangentIn"), curve.TangentIn));
            lines.Add(Write(CurveKey(i, "tangentOut"), curve.TangentOut));
            lines.Add(Write(CurveKey(i, "ls1"), curve.Ls1));
            lines.Add(Write(CurveKey(i, "radius"), curve.Radius));
            lines.Add(Write(CurveKey(i, "ls2"), curve.Ls2));
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

    private static bool GetBool(Dictionary<string, string> values, string key, bool fallback = false)
    {
        var value = Get(values, key, fallback ? "1" : "0");
        return value == "1" || value.Equals("true", StringComparison.OrdinalIgnoreCase);
    }

    private static int GetInt(Dictionary<string, string> values, string key, int fallback = 0)
        => int.TryParse(Get(values, key), NumberStyles.Integer, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private static double GetDouble(Dictionary<string, string> values, string key, double fallback = 0.0)
        => double.TryParse(Get(values, key), NumberStyles.Float, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private static List<AlignmentCurveParameterDto> ReadCurveParameters(Dictionary<string, string> values)
    {
        var fallback = new AlignmentCurveParameterDto
        {
            TangentIn = GetDouble(values, "tangentIn"),
            TangentOut = GetDouble(values, "tangentOut"),
            Ls1 = GetDouble(values, "ls1", 20.0),
            Radius = GetDouble(values, "radius", 80.0),
            Ls2 = GetDouble(values, "ls2", 20.0),
        };
        var count = GetInt(values, "curveCount", 0);
        if (count <= 0)
        {
            return new List<AlignmentCurveParameterDto> { fallback };
        }

        var parameters = new List<AlignmentCurveParameterDto>(count);
        for (var i = 0; i < count; i++)
        {
            parameters.Add(new AlignmentCurveParameterDto
            {
                TangentIn = GetDouble(values, CurveKey(i, "tangentIn"), fallback.TangentIn),
                TangentOut = GetDouble(values, CurveKey(i, "tangentOut"), fallback.TangentOut),
                Ls1 = GetDouble(values, CurveKey(i, "ls1"), fallback.Ls1),
                Radius = GetDouble(values, CurveKey(i, "radius"), fallback.Radius),
                Ls2 = GetDouble(values, CurveKey(i, "ls2"), fallback.Ls2),
            });
        }

        return parameters;
    }

    private static AlignmentCurveParameterDto CurveParameterAt(
        IReadOnlyList<AlignmentCurveParameterDto> parameters,
        int index)
    {
        if (parameters.Count == 0)
        {
            return new AlignmentCurveParameterDto();
        }

        var safeIndex = index < 0 ? 0 : index >= parameters.Count ? parameters.Count - 1 : index;
        return parameters[safeIndex];
    }

    private static string CurveKey(int index, string name)
        => $"curve.{index}.{name}";

    private static string Write(string key, string value)
        => $"{key}={Escape(value)}";

    private static string Write(string key, bool value)
        => $"{key}={(value ? 1 : 0)}";

    private static string Write(string key, int value)
        => $"{key}={value.ToString(CultureInfo.InvariantCulture)}";

    private static string Write(string key, double value)
        => $"{key}={value.ToString("R", CultureInfo.InvariantCulture)}";

    private static AlignmentDialogMode ParseMode(string value)
        => value.Equals("properties", StringComparison.OrdinalIgnoreCase)
            ? AlignmentDialogMode.Properties
            : value.Equals("curve", StringComparison.OrdinalIgnoreCase)
                ? AlignmentDialogMode.Curve
                : AlignmentDialogMode.Full;

    private static string ModeText(AlignmentDialogMode mode)
        => mode switch
        {
            AlignmentDialogMode.Properties => "properties",
            AlignmentDialogMode.Curve => "curve",
            _ => "full",
        };

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
