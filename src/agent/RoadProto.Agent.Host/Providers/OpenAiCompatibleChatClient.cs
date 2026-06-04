using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;

namespace RoadProto.Agent.Host.Providers;

public sealed class OpenAiCompatibleChatClient
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web);

    private readonly HttpClient httpClient;

    public OpenAiCompatibleChatClient()
        : this(new HttpClient())
    {
    }

    public OpenAiCompatibleChatClient(HttpClient httpClient)
    {
        this.httpClient = httpClient;
    }

    public async Task<string> CompleteAsync(
        ModelProfileOptions profile,
        string systemPrompt,
        string userMessage,
        CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(profile.ApiKeyEnvironmentVariable))
        {
            return "模型 API Key 未配置，当前可继续使用本地规则工具（例如创建路基模板）。请先在模型配置中填写 API Key 环境变量名。";
        }

        var apiKey = Environment.GetEnvironmentVariable(profile.ApiKeyEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(apiKey))
        {
            return $"模型 API Key 未配置，当前可继续使用本地规则工具（例如创建路基模板）。请设置环境变量 {profile.ApiKeyEnvironmentVariable} 后再使用模型回答。";
        }

        if (string.IsNullOrWhiteSpace(profile.BaseUrl))
        {
            return "模型 BaseUrl 未配置，当前可继续使用本地规则工具（例如创建路基模板）。";
        }

        if (string.IsNullOrWhiteSpace(profile.Model))
        {
            return "模型名称未配置，当前可继续使用本地规则工具（例如创建路基模板）。";
        }

        if (!TryCreateEndpoint(profile.BaseUrl, out var endpoint))
        {
            return "模型 BaseUrl 格式无效，当前可继续使用本地规则工具（例如创建路基模板）。";
        }

        using var request = new HttpRequestMessage(HttpMethod.Post, endpoint);
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", apiKey);
        request.Content = JsonContent.Create(
            new
            {
                model = profile.Model,
                temperature = profile.Temperature,
                stream = false,
                messages = new object[]
                {
                    new { role = "system", content = systemPrompt },
                    new { role = "user", content = userMessage }
                }
            },
            options: JsonOptions);

        try
        {
            using var timeout = CreateTimeout(profile.TimeoutSeconds, cancellationToken);
            using var response = await httpClient.SendAsync(request, timeout.Token).ConfigureAwait(false);
            if (!response.IsSuccessStatusCode)
            {
                return $"模型服务返回错误：{(int)response.StatusCode} {response.ReasonPhrase}";
            }

            await using var stream = await response.Content.ReadAsStreamAsync(timeout.Token).ConfigureAwait(false);
            using var document = await JsonDocument.ParseAsync(stream, cancellationToken: timeout.Token).ConfigureAwait(false);
            return ExtractReply(document.RootElement);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            return "模型服务请求超时，当前可继续使用本地规则工具（例如创建路基模板）。";
        }
        catch (HttpRequestException ex)
        {
            return $"模型服务请求失败：{ex.Message}";
        }
        catch (JsonException)
        {
            return "模型服务响应格式无法解析。";
        }
    }

    private static bool TryCreateEndpoint(string baseUrl, out Uri endpoint)
    {
        endpoint = null!;
        if (!Uri.TryCreate(baseUrl.TrimEnd('/') + "/", UriKind.Absolute, out var root))
        {
            return false;
        }

        endpoint = new Uri(root, "chat/completions");
        return true;
    }

    private static CancellationTokenSource CreateTimeout(int timeoutSeconds, CancellationToken cancellationToken)
    {
        var timeout = timeoutSeconds > 0 ? timeoutSeconds : 60;
        var source = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        source.CancelAfter(TimeSpan.FromSeconds(timeout));
        return source;
    }

    private static string ExtractReply(JsonElement root)
    {
        if (root.TryGetProperty("choices", out var choices)
            && choices.ValueKind == JsonValueKind.Array
            && choices.GetArrayLength() > 0
            && choices[0].TryGetProperty("message", out var message)
            && message.TryGetProperty("content", out var content))
        {
            return content.GetString() ?? string.Empty;
        }

        return "模型服务响应中缺少回复内容。";
    }
}
