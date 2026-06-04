using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI.Services;

public sealed class AgentBackendClient
{
    private static readonly JavaScriptSerializer Serializer = new();

    private readonly HttpClient client = new()
    {
        BaseAddress = new Uri("http://127.0.0.1:17831"),
        Timeout = TimeSpan.FromSeconds(20),
    };

    public async Task<bool> IsHealthyAsync()
    {
        try
        {
            using var response = await client.GetAsync("/health").ConfigureAwait(false);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<AgentChatResponse> SendAsync(AgentChatRequest request)
    {
        try
        {
            var json = SerializeRequest(request);
            using var content = new StringContent(json, Encoding.UTF8, "application/json");
            using var response = await client.PostAsync("/api/chat", content).ConfigureAwait(false);
            var responseText = await response.Content.ReadAsStringAsync().ConfigureAwait(false);
            var parsed = TryDeserializeResponse(responseText);
            if (parsed != null)
            {
                return parsed;
            }

            if (!response.IsSuccessStatusCode)
            {
                return new AgentChatResponse
                {
                    Reply = $"Agent 后端返回错误：{(int)response.StatusCode} {response.StatusCode}",
                };
            }

            return new AgentChatResponse
            {
                Reply = "Agent 后端返回了空响应。",
            };
        }
        catch (Exception error)
        {
            return new AgentChatResponse
            {
                Reply = $"Agent 后端连接失败：{error.Message}",
            };
        }
    }

    private static string SerializeRequest(AgentChatRequest request)
    {
        var history = new List<Dictionary<string, object?>>();
        foreach (var item in request.History)
        {
            history.Add(new Dictionary<string, object?>
            {
                ["role"] = item.Role,
                ["content"] = item.Content,
            });
        }

        var payload = new Dictionary<string, object?>
        {
            ["message"] = request.Message,
            ["modelProfile"] = string.IsNullOrWhiteSpace(request.ModelProfile) ? null : request.ModelProfile,
            ["history"] = history,
        };

        return Serializer.Serialize(payload);
    }

    private static AgentChatResponse? TryDeserializeResponse(string responseText)
    {
        if (string.IsNullOrWhiteSpace(responseText))
        {
            return null;
        }

        try
        {
            var root = Serializer.DeserializeObject(responseText) as IDictionary<string, object>;
            if (root == null)
            {
                return null;
            }

            var response = new AgentChatResponse
            {
                Reply = ReadString(root, "reply"),
                RequiresConfirmation = ReadBool(root, "requiresConfirmation"),
            };

            var toolCall = ReadDictionary(root, "toolCall");
            if (toolCall != null)
            {
                response.ToolCall = new AgentToolCall
                {
                    Tool = ReadString(toolCall, "tool"),
                    RequestId = ReadString(toolCall, "requestId"),
                    Arguments = ReadValue(toolCall, "arguments"),
                    ConfirmationTitle = ReadString(toolCall, "confirmationTitle"),
                    ConfirmationBody = ReadString(toolCall, "confirmationBody"),
                };
            }

            return string.IsNullOrWhiteSpace(response.Reply) && response.ToolCall == null
                ? null
                : response;
        }
        catch
        {
            return null;
        }
    }

    private static object? ReadValue(IDictionary<string, object> values, string name)
    {
        return TryGetValue(values, name, out var value) ? value : null;
    }

    private static string ReadString(IDictionary<string, object> values, string name)
    {
        return TryGetValue(values, name, out var value) ? value?.ToString() ?? string.Empty : string.Empty;
    }

    private static bool ReadBool(IDictionary<string, object> values, string name)
    {
        if (!TryGetValue(values, name, out var value) || value == null)
        {
            return false;
        }

        if (value is bool boolValue)
        {
            return boolValue;
        }

        return value is string text && bool.TryParse(text, out var parsed) && parsed;
    }

    private static IDictionary<string, object>? ReadDictionary(IDictionary<string, object> values, string name)
    {
        return TryGetValue(values, name, out var value) ? value as IDictionary<string, object> : null;
    }

    private static bool TryGetValue(IDictionary<string, object> values, string name, out object? value)
    {
        foreach (var item in values)
        {
            if (string.Equals(item.Key, name, StringComparison.OrdinalIgnoreCase))
            {
                value = item.Value;
                return true;
            }
        }

        value = null;
        return false;
    }
}
