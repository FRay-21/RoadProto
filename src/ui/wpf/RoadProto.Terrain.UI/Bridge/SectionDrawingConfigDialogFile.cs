using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace RoadProto.Terrain.UI.Bridge;

public static class SectionDrawingConfigDialogFile
{
    private const int MaxConfigRows = 10000;
    private const int MaxConfigComponents = 1000;
    private const int MaxComponentOptions = 1000;
    private const string CsvHeader = "起点桩号,终点桩号,路基类型,模板Handle,模板名称";
    private static readonly UTF8Encoding Utf8NoBom = new(encoderShouldEmitUTF8Identifier: false);
    private static readonly UTF8Encoding Utf8Bom = new(encoderShouldEmitUTF8Identifier: true);

    public static SectionDrawingConfigRequest ReadRequest(string path)
    {
        var values = ReadValues(path);
        var request = new SectionDrawingConfigRequest
        {
            DrawingHandle = Get(values, "drawingHandle"),
            RoadModelHandle = Get(values, "roadModelHandle"),
            ResponsePath = Get(values, "responsePath"),
            ConfigPath = Get(values, "configPath"),
        };

        var optionCount = ClampCount(GetInt(values, "componentOptionCount"), MaxComponentOptions);
        for (var i = 0; i < optionCount; i++)
        {
            request.ComponentOptions.Add(new SectionDrawingConfigComponentOptionDto
            {
                Code = Get(values, $"componentOption.{i}.code"),
                DisplayName = Get(values, $"componentOption.{i}.displayName"),
            });
        }

        var rowCount = ClampCount(GetInt(values, "pavementRowCount"), MaxConfigRows);
        for (var i = 0; i < rowCount; i++)
        {
            request.PavementRows.Add(ReadRow(values, $"pavementRow.{i}", request.ComponentOptions));
        }

        return request;
    }

    public static void WriteResponse(string path, SectionDrawingConfigResponse response)
    {
        var lines = new List<string>
        {
            Write("action", ActionCode(response.Action)),
            Write("accepted", response.Accepted),
            Write("drawingHandle", response.DrawingHandle),
            Write("roadModelHandle", response.RoadModelHandle),
            Write("responsePath", response.ResponsePath),
            Write("pickRowIndex", response.PickRowIndex),
            Write("configPath", response.ConfigPath),
            Write("componentOptionCount", response.ComponentOptions.Count),
        };

        for (var i = 0; i < response.ComponentOptions.Count; i++)
        {
            lines.Add(Write($"componentOption.{i}.code", response.ComponentOptions[i].Code));
            lines.Add(Write($"componentOption.{i}.displayName", response.ComponentOptions[i].DisplayName));
        }

        lines.AddRange(new[]
        {
            Write("pavementRowCount", response.PavementRows.Count),
        });

        for (var i = 0; i < response.PavementRows.Count; i++)
        {
            WriteRow(lines, $"pavementRow.{i}", response.PavementRows[i]);
        }

        File.WriteAllLines(path, lines, Utf8NoBom);
    }

    public static List<SectionDrawingConfigRowDto> ImportCsv(
        string path,
        IReadOnlyList<SectionDrawingConfigComponentOptionDto> options)
    {
        var rows = new List<SectionDrawingConfigRowDto>();
        var lines = File.ReadAllLines(path, Encoding.UTF8).ToList();
        if (lines.Count == 0)
        {
            return rows;
        }

        if (lines[0].TrimStart('\uFEFF') != CsvHeader)
        {
            throw new InvalidDataException("Section drawing config CSV header is invalid.");
        }

        for (var i = 1; i < lines.Count; i++)
        {
            if (string.IsNullOrWhiteSpace(lines[i]))
            {
                continue;
            }

            var cells = ParseCsvLine(lines[i]);
            if (cells.Count != 5)
            {
                throw new InvalidDataException("Section drawing config CSV row must have exactly five columns.");
            }

            var row = new SectionDrawingConfigRowDto
            {
                StartStation = ParseDouble(cells[0], i + 1, "起点桩号"),
                EndStation = ParseDouble(cells[1], i + 1, "终点桩号"),
                TemplateHandle = cells[3].Trim(),
                TemplateName = cells[4].Trim(),
            };
            ApplyComponentText(row, cells[2], options);
            rows.Add(row);
        }

        return rows;
    }

    public static void ExportCsv(
        string path,
        IReadOnlyList<SectionDrawingConfigRowDto> rows,
        IReadOnlyList<SectionDrawingConfigComponentOptionDto> options)
    {
        var lines = new List<string> { CsvHeader };
        foreach (var row in rows)
        {
            lines.Add(string.Join(
                ",",
                CsvEscape(row.StartStation.ToString("R", CultureInfo.InvariantCulture)),
                CsvEscape(row.EndStation.ToString("R", CultureInfo.InvariantCulture)),
                CsvEscape(SanitizeCsvField(ComponentDisplayText(row, options))),
                CsvEscape(SanitizeCsvField(row.TemplateHandle)),
                CsvEscape(SanitizeCsvField(row.TemplateName))));
        }

        File.WriteAllLines(path, lines, Utf8Bom);
    }

    private static SectionDrawingConfigRowDto ReadRow(
        Dictionary<string, string> values,
        string prefix,
        IReadOnlyList<SectionDrawingConfigComponentOptionDto> options)
    {
        var row = new SectionDrawingConfigRowDto
        {
            StartStation = GetDouble(values, $"{prefix}.startStation"),
            EndStation = GetDouble(values, $"{prefix}.endStation"),
            TemplateHandle = Get(values, $"{prefix}.templateHandle"),
            TemplateName = Get(values, $"{prefix}.templateName"),
        };
        ApplyComponentText(row, Get(values, $"{prefix}.componentCodes"), options);
        return row;
    }

    private static void WriteRow(List<string> lines, string prefix, SectionDrawingConfigRowDto row)
    {
        lines.Add(Write($"{prefix}.startStation", row.StartStation));
        lines.Add(Write($"{prefix}.endStation", row.EndStation));
        lines.Add(Write($"{prefix}.componentCodes", string.Join(";", row.ComponentCodes.Take(MaxConfigComponents))));
        lines.Add(Write($"{prefix}.templateHandle", row.TemplateHandle));
        lines.Add(Write($"{prefix}.templateName", row.TemplateName));
    }

    private static void ApplyComponentText(
        SectionDrawingConfigRowDto row,
        string value,
        IReadOnlyList<SectionDrawingConfigComponentOptionDto> options)
    {
        row.ComponentCodes.Clear();
        var displayNames = new List<string>();
        foreach (var rawPart in value.Split(new[] { ';' }, StringSplitOptions.RemoveEmptyEntries).Take(MaxConfigComponents))
        {
            var part = rawPart.Trim();
            var option = options.FirstOrDefault(candidate =>
                candidate.Code.Equals(part, StringComparison.OrdinalIgnoreCase)
                || candidate.DisplayName.Equals(part, StringComparison.OrdinalIgnoreCase));
            row.ComponentCodes.Add(option?.Code ?? part);
            displayNames.Add(option?.DisplayName ?? part);
        }
        row.ComponentDisplayText = string.Join(";", displayNames);
    }

    private static string ComponentDisplayText(
        SectionDrawingConfigRowDto row,
        IReadOnlyList<SectionDrawingConfigComponentOptionDto> options)
    {
        if (!string.IsNullOrWhiteSpace(row.ComponentDisplayText))
        {
            return row.ComponentDisplayText;
        }

        return string.Join(
            ";",
            row.ComponentCodes.Select(code =>
                options.FirstOrDefault(option => option.Code.Equals(code, StringComparison.OrdinalIgnoreCase))?.DisplayName
                ?? code));
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

    private static double ParseDouble(string value, int lineNumber, string columnName)
    {
        if (double.TryParse(value.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out var result)
            && !double.IsNaN(result)
            && !double.IsInfinity(result))
        {
            return result;
        }

        throw new InvalidDataException($"CSV line {lineNumber}, column {columnName} is not a valid number.");
    }

    private static int ClampCount(int value, int maxValue)
        => Math.Max(0, Math.Min(maxValue, value));

    private static string ActionCode(SectionDrawingConfigAction action)
        => action switch
        {
            SectionDrawingConfigAction.Draw => "draw",
            SectionDrawingConfigAction.PickTemplate => "pickTemplate",
            _ => "none",
        };

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

    private static string CsvEscape(string value)
    {
        if (value.IndexOfAny(new[] { ',', '"', '\r', '\n' }) < 0)
        {
            return value;
        }
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }

    private static string SanitizeCsvField(string value)
        => (value ?? string.Empty).Replace('\r', ' ').Replace('\n', ' ');

    private static List<string> ParseCsvLine(string line)
    {
        var cells = new List<string>();
        var current = new StringBuilder();
        var quoted = false;
        for (var i = 0; i < line.Length; i++)
        {
            var ch = line[i];
            if (quoted)
            {
                if (ch == '"' && i + 1 < line.Length && line[i + 1] == '"')
                {
                    current.Append('"');
                    i++;
                }
                else if (ch == '"')
                {
                    quoted = false;
                }
                else
                {
                    current.Append(ch);
                }
            }
            else if (ch == ',')
            {
                cells.Add(current.ToString());
                current.Clear();
            }
            else if (ch == '"')
            {
                quoted = true;
            }
            else
            {
                current.Append(ch);
            }
        }
        cells.Add(current.ToString());
        return cells;
    }
}
