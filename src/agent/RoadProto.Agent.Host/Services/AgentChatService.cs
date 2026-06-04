using System.Security.Cryptography;
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Providers;
using RoadProto.Agent.Host.Tools;

namespace RoadProto.Agent.Host.Services;

public sealed class AgentChatService
{
    private readonly SubgradeTemplateToolPlanner planner;
    private readonly AgentConfigurationStore configurations;
    private readonly AgentSecretStore secrets;
    private readonly AgentPromptContextService promptContext;
    private readonly OpenAiCompatibleChatClient modelClient;

    public AgentChatService(
        SubgradeTemplateToolPlanner planner,
        AgentConfigurationStore configurations,
        AgentSecretStore secrets,
        AgentPromptContextService promptContext,
        OpenAiCompatibleChatClient modelClient)
    {
        this.planner = planner;
        this.configurations = configurations;
        this.secrets = secrets;
        this.promptContext = promptContext;
        this.modelClient = modelClient;
    }

    public async Task<AgentChatResponse> ReplyAsync(
        AgentChatRequest request,
        CancellationToken cancellationToken = default)
    {
        var message = request.Message ?? string.Empty;
        if (string.IsNullOrWhiteSpace(message))
        {
            return new AgentChatResponse(
                "请输入要发送给 RoadProto Agent 的消息。",
                null,
                false);
        }

        var toolCall = planner.TryPlan(message, out var guidance);
        if (toolCall != null)
        {
            return new AgentChatResponse(
                "我识别到你要创建路基模板。请确认工具调用参数。",
                toolCall,
                true);
        }

        if (!string.IsNullOrWhiteSpace(guidance))
        {
            return new AgentChatResponse(guidance, null, false);
        }

        var config = await configurations.LoadAsync(cancellationToken).ConfigureAwait(false);
        var profile = FindProfile(config, request.ModelProfile);
        if (profile == null)
        {
            return new AgentChatResponse(
                "当前未配置模型。你仍可使用首个本地规则工具：创建路基模板。",
                null,
                false);
        }

        if (!string.Equals(profile.Provider, "OpenAICompatible", StringComparison.OrdinalIgnoreCase))
        {
            return new AgentChatResponse(
                $"当前模型 Provider {profile.Provider} 暂不支持。你仍可使用本地规则工具：创建路基模板。",
                null,
                false);
        }

        string? apiKey;
        try
        {
            apiKey = string.IsNullOrWhiteSpace(profile.SecretId)
                ? null
                : await secrets.ReadAsync(profile.SecretId, cancellationToken).ConfigureAwait(false);
        }
        catch (Exception ex) when (ex is CryptographicException or IOException or UnauthorizedAccessException)
        {
            return new AgentChatResponse(
                "模型 API Key 无法读取或已损坏。请在管理控制台重新保存该模型的 API Key；当前仍可使用本地规则工具：创建路基模板。",
                null,
                false);
        }

        var systemPrompt = await promptContext.BuildSystemPromptAsync(cancellationToken).ConfigureAwait(false);
        var reply = await modelClient.CompleteAsync(
            profile,
            systemPrompt,
            message,
            apiKey ?? string.Empty,
            cancellationToken).ConfigureAwait(false);

        return new AgentChatResponse(reply, null, false);
    }

    private static StoredModelProfile? FindProfile(
        AgentRuntimeConfiguration config,
        string? requestedName)
    {
        if (!string.IsNullOrWhiteSpace(requestedName))
        {
            var requestedProfile = config.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, requestedName, StringComparison.OrdinalIgnoreCase));
            if (requestedProfile != null)
            {
                return requestedProfile;
            }
        }

        if (!string.IsNullOrWhiteSpace(config.DefaultModelProfile))
        {
            var defaultProfile = config.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, config.DefaultModelProfile, StringComparison.OrdinalIgnoreCase));
            if (defaultProfile != null)
            {
                return defaultProfile;
            }
        }

        return config.ModelProfiles.FirstOrDefault();
    }
}
