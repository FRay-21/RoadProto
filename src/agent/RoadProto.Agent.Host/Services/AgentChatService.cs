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

        var confirmedToolCall = TryPlanConfirmedToolFromHistory(request);
        if (confirmedToolCall != null)
        {
            return new AgentChatResponse(
                "我识别到你确认执行创建路基模板。请确认工具调用参数。",
                confirmedToolCall,
                true);
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

    private AgentToolCall? TryPlanConfirmedToolFromHistory(AgentChatRequest request)
    {
        if (!IsConfirmationMessage(request.Message ?? string.Empty) || request.History == null)
        {
            return null;
        }

        var recentHistory = request.History.Reverse().ToArray();
        return TryPlanFromHistoryItems(recentHistory, "user")
            ?? TryPlanFromHistoryItems(recentHistory, "assistant");
    }

    private AgentToolCall? TryPlanFromHistoryItems(
        IEnumerable<AgentChatMessage> history,
        string role)
    {
        foreach (var item in history)
        {
            if (!string.Equals(item.Role, role, StringComparison.OrdinalIgnoreCase)
                || !ShouldUseHistoryForPendingTool(item))
            {
                continue;
            }

            var toolCall = planner.TryPlan(item.Content, out _);
            if (toolCall != null)
            {
                return toolCall;
            }
        }

        return null;
    }

    private static bool IsConfirmationMessage(string message)
    {
        var trimmed = message.Trim();
        if (trimmed.Length == 0 || trimmed.Length > 24)
        {
            return false;
        }

        string[] negativeWords = ["取消", "不确认", "不要", "别", "先不", "停止"];
        if (negativeWords.Any(word => trimmed.Contains(word, StringComparison.Ordinal)))
        {
            return false;
        }

        string[] confirmationWords = ["确认", "继续", "执行", "开始", "可以", "同意", "好的", "好"];
        return confirmationWords.Any(word => trimmed.Contains(word, StringComparison.Ordinal));
    }

    private static bool ShouldUseHistoryForPendingTool(AgentChatMessage message)
    {
        if (string.IsNullOrWhiteSpace(message.Content)
            || !message.Content.Contains("路基模板", StringComparison.Ordinal))
        {
            return false;
        }

        if (string.Equals(message.Role, "user", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }

        if (!string.Equals(message.Role, "assistant", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        string[] pendingMarkers = ["请确认", "确认后", "工具调用", "执行请求", "将创建", "执行创建"];
        return pendingMarkers.Any(marker => message.Content.Contains(marker, StringComparison.Ordinal));
    }
}
