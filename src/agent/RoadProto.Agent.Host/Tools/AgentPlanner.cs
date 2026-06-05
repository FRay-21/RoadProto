using RoadProto.Agent.Host.Models;

namespace RoadProto.Agent.Host.Tools;

public sealed class AgentPlanner
{
    private readonly SubgradeTemplateCreatePlanner subgradeTemplateCreatePlanner;

    public AgentPlanner(SubgradeTemplateCreatePlanner subgradeTemplateCreatePlanner)
    {
        this.subgradeTemplateCreatePlanner = subgradeTemplateCreatePlanner;
    }

    public AgentToolCall? TryPlan(string message)
    {
        return TryPlan(message, out _);
    }

    public AgentToolCall? TryPlan(string message, out string? guidance)
    {
        return subgradeTemplateCreatePlanner.TryPlan(message, out guidance);
    }
}
