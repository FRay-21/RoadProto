using System.Text.RegularExpressions;
using RoadProto.Agent.Host.Models;

namespace RoadProto.Agent.Host.Tools;

public sealed class SubgradeTemplateToolPlanner
{
    public AgentToolCall? TryPlan(string message)
    {
        if (!message.Contains("路基模板", StringComparison.Ordinal))
        {
            return null;
        }

        if (message.Contains("是什么", StringComparison.Ordinal)
            || message.Contains("怎么", StringComparison.Ordinal)
            || message.Contains("如何", StringComparison.Ordinal))
        {
            return null;
        }

        if (!message.Contains("创建", StringComparison.Ordinal)
            && !message.Contains("新建", StringComparison.Ordinal)
            && !message.Contains("生成", StringComparison.Ordinal))
        {
            return null;
        }

        var roadGrade = DetectRoadGrade(message);
        var displayScale = DetectDisplayScale(message);
        var templateName = DetectTemplateName(message);

        var arguments = new SubgradeTemplateCreateArguments(
            templateName,
            displayScale,
            roadGrade,
            string.Empty,
            new AgentInsertionPoint("PickInCad", null, null, 0),
            "DefaultByRoadGrade",
            Array.Empty<SubgradeComponentArgument>());

        return new AgentToolCall(
            "cross_section.subgrade_template.create",
            Guid.NewGuid().ToString("N"),
            arguments,
            "创建路基模板",
            $"将创建 {templateName}，道路等级 {roadGrade}，显示比例 1:{displayScale}，确认后需要在 CAD 中点取插入点。");
    }

    private static string DetectRoadGrade(string message)
    {
        if (message.Contains("一级", StringComparison.Ordinal)) return "FirstClass";
        if (message.Contains("二级", StringComparison.Ordinal)) return "SecondClass";
        if (message.Contains("三级", StringComparison.Ordinal)) return "ThirdClass";
        if (message.Contains("四级", StringComparison.Ordinal)) return "FourthClass";
        if (message.Contains("城市快速", StringComparison.Ordinal)) return "UrbanExpressway";
        if (message.Contains("城市主干", StringComparison.Ordinal)) return "UrbanArterial";
        if (message.Contains("城市次干", StringComparison.Ordinal)) return "UrbanSubArterial";
        if (message.Contains("城市支路", StringComparison.Ordinal)) return "UrbanBranch";
        return "Expressway";
    }

    private static double DetectDisplayScale(string message)
    {
        var match = Regex.Match(message, @"1\s*[:：]\s*(1|10|20|50|100)");
        return match.Success ? double.Parse(match.Groups[1].Value) : 10;
    }

    private static string DetectTemplateName(string message)
    {
        var match = Regex.Match(message, @"名字叫(?<name>[\u4e00-\u9fa5A-Za-z0-9_ -]+)");
        return match.Success ? match.Groups["name"].Value.Trim() : "默认路基模板";
    }
}
