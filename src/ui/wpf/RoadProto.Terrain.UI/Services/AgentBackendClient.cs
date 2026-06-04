using System;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI.Services;

public sealed class AgentBackendClient
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

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
            var json = JsonSerializer.Serialize(request, JsonOptions);
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

    private static AgentChatResponse? TryDeserializeResponse(string responseText)
    {
        if (string.IsNullOrWhiteSpace(responseText))
        {
            return null;
        }

        try
        {
            return JsonSerializer.Deserialize<AgentChatResponse>(responseText, JsonOptions);
        }
        catch
        {
            return null;
        }
    }
}
