using System;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
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
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
    };

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

        using var stream = File.Create(requestPath);
        using var writer = new Utf8JsonWriter(stream, new JsonWriterOptions { Indented = true });
        writer.WriteStartObject();
        writer.WriteString("tool", toolCall.Tool);
        writer.WriteString("requestId", requestId);
        writer.WritePropertyName("arguments");
        WriteArguments(writer, toolCall.Arguments);
        writer.WriteString("resultPath", resultPath);
        writer.WriteEndObject();
        writer.Flush();

        return new AgentToolExecutionPaths(requestPath, resultPath);
    }

    private static void WriteArguments(Utf8JsonWriter writer, object? arguments)
    {
        if (arguments == null)
        {
            writer.WriteStartObject();
            writer.WriteEndObject();
            return;
        }

        if (arguments is JsonElement element)
        {
            if (element.ValueKind == JsonValueKind.Undefined || element.ValueKind == JsonValueKind.Null)
            {
                writer.WriteStartObject();
                writer.WriteEndObject();
                return;
            }

            element.WriteTo(writer);
            return;
        }

        JsonSerializer.Serialize(writer, arguments, arguments.GetType(), JsonOptions);
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
