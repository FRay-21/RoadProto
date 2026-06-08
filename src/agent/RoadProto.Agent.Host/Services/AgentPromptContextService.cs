using System.Text;
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Skills;

namespace RoadProto.Agent.Host.Services;

public sealed class AgentPromptContextService
{
    public const int MaxPromptChars = 30_000;

    private const string DefaultSystemPrompt =
        "你是 RoadProto 设计软件原型 Agent。回答软件操作和道路设计原型问题。"
        + "模型返回只作为普通回复，不自动执行工具。可执行工具由本地 planner 生成并由用户确认。";

    private const string TruncatedMarker = "\n\n[上下文已截断]";

    private readonly MarkdownSkillRepository builtInSkills;
    private readonly AgentDocumentStore documents;

    public AgentPromptContextService(MarkdownSkillRepository builtInSkills, AgentDocumentStore documents)
    {
        this.builtInSkills = builtInSkills;
        this.documents = documents;
    }

    public async Task<string> BuildSystemPromptAsync(CancellationToken cancellationToken = default)
    {
        var builder = new StringBuilder(DefaultSystemPrompt);
        var builtInSkill = builtInSkills.ReadSubgradeTemplateCreateSkill();
        if (!string.IsNullOrWhiteSpace(builtInSkill))
        {
            AppendSection(builder, "## 内置 Skill", builtInSkill);
        }

        foreach (var document in await documents.ReadEnabledDocumentsAsync(
                     AgentDocumentKind.Skill,
                     MaxPromptChars,
                     cancellationToken).ConfigureAwait(false))
        {
            AppendSection(builder, $"## 用户 Skill: {document.DisplayName}", document.Content);
        }

        foreach (var document in await documents.ReadEnabledDocumentsAsync(
                     AgentDocumentKind.Knowledge,
                     MaxPromptChars,
                     cancellationToken).ConfigureAwait(false))
        {
            AppendSection(builder, $"## 知识库: {document.DisplayName}", document.Content);
        }

        return LimitPrompt(builder.ToString());
    }

    private static void AppendSection(StringBuilder builder, string title, string content)
    {
        builder.AppendLine();
        builder.AppendLine();
        builder.AppendLine(title);
        builder.AppendLine(content.Trim());
    }

    private static string LimitPrompt(string prompt)
    {
        if (prompt.Length <= MaxPromptChars)
        {
            return prompt;
        }

        var available = Math.Max(0, MaxPromptChars - TruncatedMarker.Length);
        return prompt[..available] + TruncatedMarker;
    }
}
