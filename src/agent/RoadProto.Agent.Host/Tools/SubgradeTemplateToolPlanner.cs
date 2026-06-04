using System.Text.RegularExpressions;
using RoadProto.Agent.Host.Models;

namespace RoadProto.Agent.Host.Tools;

public sealed class SubgradeTemplateToolPlanner
{
    private static readonly HashSet<int> AllowedDisplayScales = new() { 1, 10, 20, 50, 100 };

    private static readonly Regex RatioDisplayScaleRegex = new(
        @"(?:比例\s*)?1\s*(?:[:：]|比)\s*(?<scale>\d+)",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private static readonly Regex NumericDisplayScaleRegex = new(
        @"比例\s*(?:为|是)?\s*(?<scale>\d+)",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private static readonly string[] ExplicitComponentKeywords =
    {
        "行车道",
        "车道",
        "硬路肩",
        "土路肩",
        "中分带",
        "中央分隔带",
        "路缘带",
        "侧分带",
        "慢车道",
        "人行道",
        "自行车道",
        "非机动车道"
    };

    public AgentToolCall? TryPlan(string message)
    {
        return TryPlan(message, out _);
    }

    public AgentToolCall? TryPlan(string message, out string? guidance)
    {
        guidance = null;

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
        if (displayScale.WasExplicit && !AllowedDisplayScales.Contains(displayScale.Value))
        {
            guidance = "路基模板显示比例只支持 1、10、20、50、100。请把比例改为这些取值后再创建。";
            return null;
        }

        if (DescribesExplicitComponents(message))
        {
            guidance = "我识别到你描述了路基模板的具体部件。当前本地规则骨架暂不生成具体部件工具调用，请先确认是否使用道路等级默认部件，或补全每个部件的侧别、类型和宽度后再处理。";
            return null;
        }

        var templateName = DetectTemplateName(message);

        var arguments = new SubgradeTemplateCreateArguments(
            templateName,
            displayScale.Value,
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
            $"将创建 {templateName}，道路等级 {roadGrade}，显示比例 1:{displayScale.Value}，确认后需要在 CAD 中点取插入点。");
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

    private static DisplayScaleDetection DetectDisplayScale(string message)
    {
        var ratioMatch = RatioDisplayScaleRegex.Match(message);
        if (ratioMatch.Success && int.TryParse(ratioMatch.Groups["scale"].Value, out var ratioScale))
        {
            return new DisplayScaleDetection(true, ratioScale);
        }

        var numericMatch = NumericDisplayScaleRegex.Match(message);
        if (numericMatch.Success && int.TryParse(numericMatch.Groups["scale"].Value, out var numericScale))
        {
            return new DisplayScaleDetection(true, numericScale);
        }

        return new DisplayScaleDetection(false, 10);
    }

    private static string DetectTemplateName(string message)
    {
        var match = Regex.Match(message, @"名字叫(?<name>[\u4e00-\u9fa5A-Za-z0-9_ -]+)");
        return match.Success ? match.Groups["name"].Value.Trim() : "默认路基模板";
    }

    private static bool DescribesExplicitComponents(string message)
    {
        return ExplicitComponentKeywords.Any(keyword => message.Contains(keyword, StringComparison.Ordinal));
    }

    private sealed record DisplayScaleDetection(bool WasExplicit, int Value);
}
