using Microsoft.Extensions.Options;
using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Providers;
using RoadProto.Agent.Host.Services;
using RoadProto.Agent.Host.Skills;
using RoadProto.Agent.Host.Tools;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class AgentChatServiceTests
{
    [Fact]
    public async Task ReplyReturnsPlannerGuidanceBeforeUsingModel()
    {
        var service = CreateService(new ModelProfileOptions
        {
            Name = "missing-key",
            BaseUrl = "https://example.invalid/v1",
            ApiKeyEnvironmentVariable = CreateMissingEnvironmentVariableName(),
            Model = "test-model"
        });

        var response = await service.ReplyAsync(
            new AgentChatRequest("帮我创建一个二级公路路基模板，比例1:25", "missing-key", null));

        Assert.False(response.RequiresConfirmation);
        Assert.Null(response.ToolCall);
        Assert.Contains("1、10、20、50、100", response.Reply);
        Assert.DoesNotContain("API Key", response.Reply);
    }

    [Fact]
    public async Task ReplyReturnsReadablePromptWhenApiKeyIsMissing()
    {
        var service = CreateService(new ModelProfileOptions
        {
            Name = "missing-key",
            BaseUrl = "https://example.invalid/v1",
            ApiKeyEnvironmentVariable = CreateMissingEnvironmentVariableName(),
            Model = "test-model"
        });

        var response = await service.ReplyAsync(
            new AgentChatRequest("请介绍 RoadProto Agent 的功能", "missing-key", null));

        Assert.False(response.RequiresConfirmation);
        Assert.Null(response.ToolCall);
        Assert.Contains("API Key 未配置", response.Reply);
        Assert.Contains("本地规则", response.Reply);
    }

    [Fact]
    public async Task ReplyFallsBackToFirstProfileWhenRequestedProfileIsUnknown()
    {
        var service = CreateService(new ModelProfileOptions
        {
            Name = "first-profile",
            BaseUrl = "https://example.invalid/v1",
            ApiKeyEnvironmentVariable = CreateMissingEnvironmentVariableName(),
            Model = "test-model"
        });

        var response = await service.ReplyAsync(
            new AgentChatRequest("请介绍 RoadProto Agent 的功能", "unknown-profile", null));

        Assert.False(response.RequiresConfirmation);
        Assert.Null(response.ToolCall);
        Assert.Contains("API Key 未配置", response.Reply);
    }

    private static AgentChatService CreateService(params ModelProfileOptions[] profiles)
    {
        var options = new RoadProtoAgentOptions();
        options.ModelProfiles.AddRange(profiles);

        return new AgentChatService(
            new SubgradeTemplateToolPlanner(),
            new MarkdownSkillRepository(Directory.GetCurrentDirectory()),
            new OpenAiCompatibleChatClient(),
            Options.Create(options));
    }

    private static string CreateMissingEnvironmentVariableName()
    {
        var name = "ROADPROTO_AGENT_TEST_MISSING_API_KEY_" + Guid.NewGuid().ToString("N");
        Environment.SetEnvironmentVariable(name, null);
        return name;
    }
}
