using System.Text.Json;
using RoadProto.Agent.Host.Admin;
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
    public async Task ReplyReturnsReadablePromptWhenStoredApiKeyIsCorrupted()
    {
        if (!OperatingSystem.IsWindows())
        {
            return;
        }

        var root = Path.Combine(Path.GetTempPath(), "RoadProtoAgentChatServiceTests", Guid.NewGuid().ToString("N"));
        try
        {
            var paths = new AgentLocalPaths(root);
            paths.Ensure();
            var configStore = new AgentConfigurationStore(paths, new RoadProtoAgentOptions());
            var secretStore = new AgentSecretStore(paths);
            await File.WriteAllBytesAsync(
                Path.Combine(paths.SecretsDirectory, "profile-corrupt.bin"),
                System.Text.Encoding.UTF8.GetBytes("not dpapi protected bytes"));
            await configStore.SaveAsync(new AgentRuntimeConfiguration
            {
                DefaultModelProfile = "corrupt-key",
                ModelProfiles =
                [
                    new StoredModelProfile
                    {
                        Name = "corrupt-key",
                        Provider = "OpenAICompatible",
                        BaseUrl = "https://example.invalid/v1",
                        Model = "runtime-model",
                        SecretId = "profile-corrupt",
                        TimeoutSeconds = 60
                    }
                ]
            });

            var service = CreateService(
                configStore,
                secretStore,
                new OpenAiCompatibleChatClient(new HttpClient(new CapturingChatHandler())));

            var response = await service.ReplyAsync(
                new AgentChatRequest("请介绍 RoadProto Agent", null, null));

            Assert.False(response.RequiresConfirmation);
            Assert.Null(response.ToolCall);
            Assert.Contains("API Key 无法读取", response.Reply);
            Assert.Contains("重新保存", response.Reply);
            Assert.Contains("本地规则工具", response.Reply);
        }
        finally
        {
            if (Directory.Exists(root))
            {
                Directory.Delete(root, recursive: true);
            }
        }
    }

    [Fact]
    public async Task ReplyUsesRuntimeDefaultProfileAndEncryptedApiKey()
    {
        if (!OperatingSystem.IsWindows())
        {
            return;
        }

        var root = Path.Combine(Path.GetTempPath(), "RoadProtoAgentChatServiceTests", Guid.NewGuid().ToString("N"));
        try
        {
            var paths = new AgentLocalPaths(root);
            var configStore = new AgentConfigurationStore(paths, new RoadProtoAgentOptions());
            var secretStore = new AgentSecretStore(paths);
            await secretStore.SaveAsync("profile-runtime-default", "sk-runtime-secret");
            await configStore.SaveAsync(new AgentRuntimeConfiguration
            {
                DefaultModelProfile = "runtime-default",
                ModelProfiles =
                [
                    new StoredModelProfile
                    {
                        Name = "runtime-default",
                        Provider = "OpenAICompatible",
                        BaseUrl = "https://example.invalid/v1",
                        Model = "runtime-model",
                        SecretId = "profile-runtime-default",
                        TimeoutSeconds = 60
                    }
                ]
            });

            var handler = new CapturingChatHandler();
            var service = CreateService(
                configStore,
                secretStore,
                new OpenAiCompatibleChatClient(new HttpClient(handler)));

            var response = await service.ReplyAsync(
                new AgentChatRequest("请介绍 RoadProto Agent", null, null));

            Assert.False(response.RequiresConfirmation);
            Assert.Equal("runtime model reply", response.Reply);
            Assert.Equal("Bearer sk-runtime-secret", handler.Authorization);
            Assert.Contains("runtime-model", handler.RequestBody);
            using var requestJson = JsonDocument.Parse(handler.RequestBody);
            var systemPrompt = requestJson.RootElement
                .GetProperty("messages")[0]
                .GetProperty("content")
                .GetString();
            Assert.Contains("你是 RoadProto 设计软件原型 Agent", systemPrompt);
        }
        finally
        {
            if (Directory.Exists(root))
            {
                Directory.Delete(root, recursive: true);
            }
        }
    }

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
        var paths = new AgentLocalPaths(Path.Combine(
            Path.GetTempPath(),
            "RoadProtoAgentChatServiceTests",
            Guid.NewGuid().ToString("N")));
        var configStore = new AgentConfigurationStore(paths, options);
        var secretStore = new AgentSecretStore(paths);

        return new AgentChatService(
            new SubgradeTemplateToolPlanner(),
            configStore,
            secretStore,
            CreatePromptContext(paths),
            new OpenAiCompatibleChatClient());
    }

    private static AgentChatService CreateService(
        AgentConfigurationStore configStore,
        AgentSecretStore secretStore,
        OpenAiCompatibleChatClient modelClient)
    {
        return new AgentChatService(
            new SubgradeTemplateToolPlanner(),
            configStore,
            secretStore,
            CreatePromptContext(new AgentLocalPaths(Path.Combine(
                Path.GetTempPath(),
                "RoadProtoAgentChatServiceTests",
                Guid.NewGuid().ToString("N")))),
            modelClient);
    }

    private static AgentPromptContextService CreatePromptContext(AgentLocalPaths paths)
    {
        var store = new AgentConfigurationStore(paths, new RoadProtoAgentOptions());
        return new AgentPromptContextService(
            new MarkdownSkillRepository(Directory.GetCurrentDirectory()),
            new AgentDocumentStore(paths, store));
    }

    private static string CreateMissingEnvironmentVariableName()
    {
        var name = "ROADPROTO_AGENT_TEST_MISSING_API_KEY_" + Guid.NewGuid().ToString("N");
        Environment.SetEnvironmentVariable(name, null);
        return name;
    }

    private sealed class CapturingChatHandler : HttpMessageHandler
    {
        public string Authorization { get; private set; } = string.Empty;
        public string RequestBody { get; private set; } = string.Empty;

        protected override async Task<HttpResponseMessage> SendAsync(
            HttpRequestMessage request,
            CancellationToken cancellationToken)
        {
            Authorization = request.Headers.Authorization?.ToString() ?? string.Empty;
            RequestBody = request.Content == null
                ? string.Empty
                : await request.Content.ReadAsStringAsync(cancellationToken);

            return new HttpResponseMessage(System.Net.HttpStatusCode.OK)
            {
                Content = new StringContent(
                    """{"choices":[{"message":{"content":"runtime model reply"}}]}""",
                    System.Text.Encoding.UTF8,
                    "application/json")
            };
        }
    }
}
