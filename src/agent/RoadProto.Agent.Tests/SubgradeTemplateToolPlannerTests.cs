using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Tools;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class AgentPlannerTests
{
    [Fact]
    public void PlansSecondClassSubgradeTemplateThroughAgentPlanner()
    {
        var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

        var result = planner.TryPlan("帮我创建一个二级公路路基模板，比例1:20，名字叫K1路基模板", out var guidance);

        Assert.Null(guidance);
        Assert.NotNull(result);
        Assert.Equal("cross_section.subgrade_template.create", result.Tool);
        var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
        Assert.Equal("K1路基模板", args.TemplateName);
        Assert.Equal(20, args.DisplayScale);
        Assert.Equal("SecondClass", args.RoadGrade);
        Assert.Equal("DefaultByRoadGrade", args.ComponentSource);
        Assert.Empty(args.Components);
        Assert.Empty(args.ComponentOperations);
        Assert.Equal("PickInCad", args.InsertionPoint.Mode);
    }

    [Theory]
    [InlineData("帮我创建一个二级公路路基模板，比例1:20")]
    [InlineData("帮我创建一个二级公路路基模板，比例 1：20")]
    [InlineData("帮我创建一个二级公路路基模板，比例20")]
    [InlineData("帮我创建一个二级公路路基模板，1比20")]
    public void PlansSupportedDisplayScaleExpressions(string message)
    {
        var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

        var result = planner.TryPlan(message);

        Assert.NotNull(result);
        var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
        Assert.Equal(20, args.DisplayScale);
    }

    [Fact]
    public void DefaultsDisplayScaleWhenScaleIsNotExplicit()
    {
        var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

        var result = planner.TryPlan("帮我创建一个二级公路路基模板");

        Assert.NotNull(result);
        var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
        Assert.Equal(10, args.DisplayScale);
    }

    [Theory]
    [InlineData("创建市政道路路基模板")]
    [InlineData("创建城市道路路基模板")]
    [InlineData("帮我生成一个市政道路的模板")]
    public void PlansMunicipalRoadAsUrbanArterial(string message)
    {
        var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

        var result = planner.TryPlan(message, out var guidance);

        Assert.Null(guidance);
        Assert.NotNull(result);
        var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
        Assert.Equal("UrbanArterial", args.RoadGrade);
        Assert.Equal("默认路基模板", args.TemplateName);
        Assert.Equal(10, args.DisplayScale);
        Assert.Equal("DefaultByRoadGrade", args.ComponentSource);
    }

    [Theory]
    [InlineData("我想创建一个市政道路的路基模板，并基于默认参数上，最右侧增加一个行车道部件")]
    [InlineData("创建城市道路模板，在右边再加一条车道")]
    [InlineData("生成市政路模板，右侧外侧增加机动车道")]
    public void PlansRightTravelLaneOperationOnDefaultUrbanTemplate(string message)
    {
        var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

        var result = planner.TryPlan(message, out var guidance);

        Assert.Null(guidance);
        Assert.NotNull(result);
        var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
        Assert.Equal("UrbanArterial", args.RoadGrade);
        Assert.Equal("DefaultByRoadGrade", args.ComponentSource);
        var operation = Assert.Single(args.ComponentOperations);
        Assert.Equal("AddComponent", operation.Action);
        Assert.Equal("Right", operation.Side);
        Assert.Equal("TravelLane", operation.Type);
        Assert.Equal("OutermostMotorLane", operation.Position);
        Assert.Null(operation.Width);
        Assert.Contains("右侧机动车道组外侧新增 1 个行车道", result.ConfirmationBody);
    }

    [Theory]
    [InlineData("帮我创建一个二级公路路基模板，比例1:25")]
    [InlineData("帮我创建一个二级公路路基模板，比例25")]
    public void DoesNotPlanUnsupportedExplicitDisplayScale(string message)
    {
        var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

        var result = planner.TryPlan(message, out var guidance);

        Assert.Null(result);
        Assert.Contains("1、10、20、50、100", guidance);
    }

    [Theory]
    [InlineData("帮我创建一个左右各两条 3.5 米车道的路基模板")]
    [InlineData("帮我创建一个带硬路肩的路基模板")]
    [InlineData("帮我创建一个带土路肩和行车道的路基模板")]
    public void DoesNotPlanWhenUserDescribesExplicitComponents(string message)
    {
        var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

        var result = planner.TryPlan(message, out var guidance);

        Assert.Null(result);
        Assert.Contains("具体部件", guidance);
    }

    [Fact]
    public void DoesNotPlanForQuestionOnly()
    {
        var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

        var result = planner.TryPlan("路基模板是什么？");

        Assert.Null(result);
    }
}
