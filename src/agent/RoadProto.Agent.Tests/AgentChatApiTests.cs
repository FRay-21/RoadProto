using System.Net;
using System.Net.Http.Json;
using System.Text;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.Extensions.DependencyInjection;
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Providers;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class AgentChatApiTests : IClassFixture<AgentChatApiTestFactory>
{
    private readonly AgentChatApiTestFactory factory;

    public AgentChatApiTests(AgentChatApiTestFactory factory)
    {
        this.factory = factory;
    }

    [Theory]
    [InlineData("{}")]
    [InlineData("""{"message":""}""")]
    [InlineData("""{"message":"   "}""")]
    public async Task ChatReturnsBadRequestForMissingOrEmptyMessage(string requestJson)
    {
        using var client = factory.CreateClient();
        using var content = new StringContent(requestJson, Encoding.UTF8, "application/json");

        using var response = await client.PostAsync("/api/chat", content);

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        var body = await response.Content.ReadFromJsonAsync<AgentChatResponse>();
        Assert.NotNull(body);
        Assert.False(body.RequiresConfirmation);
        Assert.Null(body.ToolCall);
        Assert.Contains("请输入", body.Reply);
    }

    [Fact]
    public async Task ChatReturnsGuidanceForUnsupportedExplicitDisplayScale()
    {
        using var client = factory.CreateClient();

        using var response = await client.PostAsJsonAsync(
            "/api/chat",
            new AgentChatRequest("帮我创建一个二级公路路基模板，比例1:25", null, null));

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var body = await response.Content.ReadFromJsonAsync<AgentChatResponse>();
        Assert.NotNull(body);
        Assert.False(body.RequiresConfirmation);
        Assert.Null(body.ToolCall);
        Assert.Contains("1、10、20、50、100", body.Reply);
    }

    [Fact]
    public async Task ChatReturnsToolCallWithoutModelConfiguration()
    {
        using var client = factory.CreateClient();

        using var response = await client.PostAsJsonAsync(
            "/api/chat",
            new AgentChatRequest("帮我创建一个二级公路路基模板，比例1:20", null, null));

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var body = await response.Content.ReadFromJsonAsync<AgentChatResponse>();
        Assert.NotNull(body);
        Assert.True(body.RequiresConfirmation);
        Assert.NotNull(body.ToolCall);
        Assert.Equal("cross_section.subgrade_template.create", body.ToolCall.Tool);
    }

    [Fact]
    public async Task ChatReturnsUnconfiguredModelPromptWhenNoProfileExists()
    {
        using var client = factory.CreateClient();

        using var response = await client.PostAsJsonAsync(
            "/api/chat",
            new AgentChatRequest("请介绍一下 RoadProto Agent 的使用方式", null, null));

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var body = await response.Content.ReadFromJsonAsync<AgentChatResponse>();
        Assert.NotNull(body);
        Assert.False(body.RequiresConfirmation);
        Assert.Null(body.ToolCall);
        Assert.Contains("当前未配置模型", body.Reply);
    }

    [Fact]
    public async Task ChatIgnoresExternalModelProfileEnvironment()
    {
        const string profileNameKey = "RoadProtoAgent__ModelProfiles__0__Name";
        const string providerKey = "RoadProtoAgent__ModelProfiles__0__Provider";
        var originalProfileName = Environment.GetEnvironmentVariable(profileNameKey);
        var originalProvider = Environment.GetEnvironmentVariable(providerKey);

        try
        {
            Environment.SetEnvironmentVariable(profileNameKey, "external-profile");
            Environment.SetEnvironmentVariable(providerKey, "ExternalProvider");

            using var externalConfigFactory = new AgentChatApiTestFactory();
            using var client = externalConfigFactory.CreateClient();

            using var response = await client.PostAsJsonAsync(
                "/api/chat",
                new AgentChatRequest("请介绍一下 RoadProto Agent 的使用方式", null, null));

            Assert.Equal(HttpStatusCode.OK, response.StatusCode);
            var body = await response.Content.ReadFromJsonAsync<AgentChatResponse>();
            Assert.NotNull(body);
            Assert.False(body.RequiresConfirmation);
            Assert.Null(body.ToolCall);
            Assert.Contains("当前未配置模型", body.Reply);
        }
        finally
        {
            Environment.SetEnvironmentVariable(profileNameKey, originalProfileName);
            Environment.SetEnvironmentVariable(providerKey, originalProvider);
        }
    }

    [Fact]
    public async Task ChatUsesRuntimeDefaultModelProfile()
    {
        var root = Path.Combine(Path.GetTempPath(), "RoadProtoAgentChatApiTests", Guid.NewGuid().ToString("N"));
        try
        {
            var paths = new AgentLocalPaths(root);
            var store = new AgentConfigurationStore(paths, new RoadProtoAgentOptions());
            await store.SaveAsync(new AgentRuntimeConfiguration
            {
                DefaultModelProfile = "runtime-default",
                ModelProfiles =
                [
                    new StoredModelProfile
                    {
                        Name = "runtime-default",
                        Provider = "RuntimeProvider",
                        BaseUrl = "https://example.invalid/v1",
                        Model = "runtime-model"
                    }
                ]
            });
            using var runtimeFactory = new AgentChatApiTestFactory(root);
            using var client = runtimeFactory.CreateClient();

            using var response = await client.PostAsJsonAsync(
                "/api/chat",
                new AgentChatRequest("请介绍一下 RoadProto Agent 的使用方式", null, null));

            Assert.Equal(HttpStatusCode.OK, response.StatusCode);
            var body = await response.Content.ReadFromJsonAsync<AgentChatResponse>();
            Assert.NotNull(body);
            Assert.Contains("RuntimeProvider", body.Reply);
        }
        finally
        {
            if (Directory.Exists(root))
            {
                Directory.Delete(root, recursive: true);
            }
        }
    }
}

public sealed class AgentChatApiTestFactory : WebApplicationFactory<Program>, IDisposable
{
    private readonly string root;

    public AgentChatApiTestFactory()
        : this(Path.Combine(Path.GetTempPath(), "RoadProtoAgentChatApiTests", Guid.NewGuid().ToString("N")))
    {
    }

    internal AgentChatApiTestFactory(string root)
    {
        this.root = root;
    }

    protected override void ConfigureWebHost(IWebHostBuilder builder)
    {
        builder.ConfigureServices(services =>
        {
            services.AddSingleton(new AgentLocalPaths(root));
            services.PostConfigure<RoadProtoAgentOptions>(options => options.ModelProfiles.Clear());
        });
    }

    protected override void Dispose(bool disposing)
    {
        base.Dispose(disposing);
        if (disposing && Directory.Exists(root))
        {
            Directory.Delete(root, recursive: true);
        }
    }
}
