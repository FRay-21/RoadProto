using RoadProto.Agent.Host.Models;

namespace RoadProto.Agent.Host.Tools;

public static class SubgradeTemplateComponentOperationFactory
{
    public static IReadOnlyList<SubgradeComponentOperationArgument> DetectOperations(string message)
    {
        if (!MentionsRightSide(message) || !MentionsTravelLane(message) || !MentionsAdd(message))
        {
            return Array.Empty<SubgradeComponentOperationArgument>();
        }

        return new[]
        {
            new SubgradeComponentOperationArgument(
                "AddComponent",
                "Right",
                "TravelLane",
                "OutermostMotorLane",
                null)
        };
    }

    private static bool MentionsRightSide(string message)
    {
        return message.Contains("最右", StringComparison.Ordinal)
            || message.Contains("右侧", StringComparison.Ordinal)
            || message.Contains("右边", StringComparison.Ordinal);
    }

    private static bool MentionsTravelLane(string message)
    {
        return message.Contains("行车道", StringComparison.Ordinal)
            || message.Contains("车道", StringComparison.Ordinal)
            || message.Contains("机动车道", StringComparison.Ordinal);
    }

    private static bool MentionsAdd(string message)
    {
        return message.Contains("增加", StringComparison.Ordinal)
            || message.Contains("新增", StringComparison.Ordinal)
            || message.Contains("加一", StringComparison.Ordinal)
            || message.Contains("再加", StringComparison.Ordinal)
            || message.Contains("添加", StringComparison.Ordinal);
    }
}
