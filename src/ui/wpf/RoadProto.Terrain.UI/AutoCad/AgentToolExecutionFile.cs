using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class AgentToolExecutionPaths
{
    public AgentToolExecutionPaths(string requestPath, string resultPath)
    {
        RequestPath = requestPath;
        ResultPath = resultPath;
    }

    public string RequestPath { get; }

    public string ResultPath { get; }
}

public static class AgentToolExecutionFile
{
    private static readonly JavaScriptSerializer Serializer = new();
    private static readonly Encoding Utf8NoBom = new UTF8Encoding(false);

    public static AgentToolExecutionPaths WriteToolRequest(AgentToolCall toolCall)
    {
        if (string.IsNullOrWhiteSpace(toolCall.Tool))
        {
            throw new InvalidOperationException("Agent tool name is empty.");
        }

        var requestId = string.IsNullOrWhiteSpace(toolCall.RequestId)
            ? Guid.NewGuid().ToString("N")
            : toolCall.RequestId.Trim();
        var safeRequestId = CreateSafeFileToken(requestId);
        var directory = Path.Combine(Path.GetTempPath(), "RoadProtoAgent");
        Directory.CreateDirectory(directory);

        var processId = Process.GetCurrentProcess().Id;
        var requestPath = Path.Combine(
            directory,
            $"RoadProtoAgentToolRequest_{processId}_{safeRequestId}.json");
        var resultPath = Path.Combine(
            directory,
            $"RoadProtoAgentToolResult_{processId}_{safeRequestId}.json");

        if (File.Exists(resultPath))
        {
            File.Delete(resultPath);
        }

        var payload = new Dictionary<string, object?>
        {
            ["tool"] = toolCall.Tool,
            ["requestId"] = requestId,
            ["arguments"] = NormalizeArguments(toolCall.Arguments),
            ["resultPath"] = resultPath,
        };
        File.WriteAllText(requestPath, Serializer.Serialize(payload), Utf8NoBom);

        return new AgentToolExecutionPaths(requestPath, resultPath);
    }

    private static object NormalizeArguments(object? arguments)
    {
        return NormalizeJsonValue(arguments) ?? new Dictionary<string, object?>();
    }

    private static object? NormalizeJsonValue(object? value)
    {
        if (value == null)
        {
            return null;
        }

        if (value is string || value is bool || value is char || value is byte || value is sbyte ||
            value is short || value is ushort || value is int || value is uint || value is long ||
            value is ulong || value is float || value is double || value is decimal)
        {
            return value;
        }

        if (value is IDictionary<string, object> stringDictionary)
        {
            var normalized = new Dictionary<string, object?>();
            foreach (var item in stringDictionary)
            {
                normalized[item.Key] = NormalizeJsonValue(item.Value);
            }

            return normalized;
        }

        if (value is IDictionary dictionary)
        {
            var normalized = new Dictionary<string, object?>();
            foreach (DictionaryEntry item in dictionary)
            {
                var key = item.Key?.ToString();
                if (string.IsNullOrWhiteSpace(key))
                {
                    continue;
                }

                normalized[key!] = NormalizeJsonValue(item.Value);
            }

            return normalized;
        }

        if (value is IEnumerable values)
        {
            var normalized = new List<object?>();
            foreach (var item in values)
            {
                normalized.Add(NormalizeJsonValue(item));
            }

            return normalized;
        }

        return value;
    }

    private static string CreateSafeFileToken(string text)
    {
        var buffer = new char[Math.Min(text.Length, 80)];
        var length = 0;
        foreach (var item in text)
        {
            if (length == buffer.Length)
            {
                break;
            }

            buffer[length++] = char.IsLetterOrDigit(item) || item == '-' || item == '_'
                ? item
                : '_';
        }

        return length == 0 ? Guid.NewGuid().ToString("N") : new string(buffer, 0, length);
    }
}
