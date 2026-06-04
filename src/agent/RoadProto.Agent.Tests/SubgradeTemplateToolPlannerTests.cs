using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Tools;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class SubgradeTemplateToolPlannerTests
{
    [Fact]
    public void PlansSecondClassSubgradeTemplate()
    {
        var planner = new SubgradeTemplateToolPlanner();

        var result = planner.TryPlan("帮我创建一个二级公路路基模板，比例1:20，名字叫K1路基模板");

        Assert.NotNull(result);
        Assert.Equal("cross_section.subgrade_template.create", result.Tool);
        var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
        Assert.Equal("K1路基模板", args.TemplateName);
        Assert.Equal(20, args.DisplayScale);
        Assert.Equal("SecondClass", args.RoadGrade);
        Assert.Equal("DefaultByRoadGrade", args.ComponentSource);
        Assert.Empty(args.Components);
        Assert.Equal("PickInCad", args.InsertionPoint.Mode);
    }

    [Fact]
    public void DoesNotPlanForQuestionOnly()
    {
        var planner = new SubgradeTemplateToolPlanner();

        var result = planner.TryPlan("路基模板是什么？");

        Assert.Null(result);
    }
}
