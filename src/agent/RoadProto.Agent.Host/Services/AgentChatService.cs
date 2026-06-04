using Microsoft.Extensions.Options;
using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Providers;
using RoadProto.Agent.Host.Skills;
using RoadProto.Agent.Host.Tools;

namespace RoadProto.Agent.Host.Services;

public sealed class AgentChatService
{
    private const string DefaultSystemPrompt =
        "你是 RoadProto 设计软件原型 Agent。回答软件操作和道路设计原型问题。"
        + "模型返回只作为普通回复，不自动执行工具。可执行工具由本地 planner 生成并由用户确认。";

    private readonly SubgradeTemplateToolPlanner planner;
    private readonly MarkdownSkillRepository skills;
    private readonly OpenAiCompatibleChatClient modelClient;
    private readonly RoadProtoAgentOptions options;

    public AgentChatService(
        SubgradeTemplateToolPlanner planner,
        MarkdownSkillRepository skills,
        OpenAiCompatibleChatClient modelClient,
        IOptions<RoadProtoAgentOptions> options)
    {
        this.planner = planner;
        this.skills = skills;
        this.modelClient = modelClient;
        this.options = options.Value;
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

        var profile = FindProfile(request.ModelProfile);
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

        var reply = await modelClient.CompleteAsync(
            profile,
            CreateSystemPrompt(),
            message,
            cancellationToken).ConfigureAwait(false);

        return new AgentChatResponse(reply, null, false);
    }

    private ModelProfileOptions? FindProfile(string? requestedName)
    {
        if (!string.IsNullOrWhiteSpace(requestedName))
        {
            var requestedProfile = options.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, requestedName, StringComparison.OrdinalIgnoreCase));
            if (requestedProfile != null)
            {
                return requestedProfile;
            }
        }

        return options.ModelProfiles.FirstOrDefault();
    }

    private string CreateSystemPrompt()
    {
        var skill = skills.ReadSubgradeTemplateCreateSkill();
        if (string.IsNullOrWhiteSpace(skill))
        {
            return DefaultSystemPrompt;
        }

        return DefaultSystemPrompt + "\n\n可用 skill：\n" + skill;
    }
}
