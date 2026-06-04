using System.Net;
using System.Net.Http.Json;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.Extensions.DependencyInjection;
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Providers;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class AgentAdminApiTests : IDisposable
{
    private readonly string root = Path.Combine(Path.GetTempPath(), "RoadProtoAgentAdminApiTests", Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        if (Directory.Exists(root))
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [Fact]
    public async Task AdminStatusReturnsStorageRoot()
    {
        using var factory = CreateFactory();
        using var client = factory.CreateClient();

        var status = await client.GetFromJsonAsync<AdminStatusResponse>("/api/admin/status");

        Assert.NotNull(status);
        Assert.Equal(root, status.StorageRoot);
    }

    [Fact]
    public async Task CanCreateDefaultModelProfileWithEncryptedKey()
    {
        using var factory = CreateFactory();
        using var client = factory.CreateClient();

        using var response = await client.PostAsJsonAsync(
            "/api/admin/model-profiles",
            new UpsertModelProfileRequest(
                "dashscope-qwen",
                "OpenAICompatible",
                "https://dashscope.aliyuncs.com/compatible-mode/v1",
                "qwen-plus",
                0.2,
                60,
                "sk-test-secret",
                true));

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var profiles = await client.GetFromJsonAsync<List<ModelProfileResponse>>("/api/admin/model-profiles");
        var profile = Assert.Single(profiles!);
        Assert.True(profile.IsDefault);
        Assert.True(profile.HasApiKey);
        Assert.Equal("sk-***cret", profile.ApiKeyMask);
        Assert.DoesNotContain("sk-test-secret", await File.ReadAllTextAsync(Path.Combine(root, "config.json")));
    }

    [Fact]
    public async Task RejectsInvalidProfileName()
    {
        using var factory = CreateFactory();
        using var client = factory.CreateClient();

        using var response = await client.PostAsJsonAsync(
            "/api/admin/model-profiles",
            new UpsertModelProfileRequest(
                "../bad",
                "OpenAICompatible",
                "https://api.openai.com/v1",
                "gpt-4.1",
                0.2,
                60,
                "sk-test",
                true));

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
    }

    [Fact]
    public async Task DeletingDefaultModelProfilePromotesRemainingProfileAndDeletesSecret()
    {
        using var factory = CreateFactory();
        using var client = factory.CreateClient();

        using var openAiResponse = await client.PostAsJsonAsync(
            "/api/admin/model-profiles",
            new UpsertModelProfileRequest(
                "openai",
                "OpenAICompatible",
                "https://api.openai.com/v1",
                "gpt-4.1",
                0.2,
                60,
                "sk-openai-secret",
                true));
        using var deepSeekResponse = await client.PostAsJsonAsync(
            "/api/admin/model-profiles",
            new UpsertModelProfileRequest(
                "deepseek",
                "OpenAICompatible",
                "https://api.deepseek.com/v1",
                "deepseek-chat",
                0.2,
                60,
                null,
                false));
        var openAiSecretPath = Path.Combine(
            root,
            "secrets",
            AgentSecretStore.CreateSecretId("openai") + ".bin");

        Assert.Equal(HttpStatusCode.OK, openAiResponse.StatusCode);
        Assert.Equal(HttpStatusCode.OK, deepSeekResponse.StatusCode);
        Assert.True(File.Exists(openAiSecretPath));

        using var deleteOpenAiResponse = await client.DeleteAsync("/api/admin/model-profiles/openai");

        Assert.Equal(HttpStatusCode.NoContent, deleteOpenAiResponse.StatusCode);
        var profiles = await client.GetFromJsonAsync<List<ModelProfileResponse>>("/api/admin/model-profiles");
        var profile = Assert.Single(profiles!);
        Assert.Equal("deepseek", profile.Name);
        Assert.True(profile.IsDefault);
        Assert.False(File.Exists(openAiSecretPath));

        using var deleteDeepSeekResponse = await client.DeleteAsync("/api/admin/model-profiles/deepseek");

        Assert.Equal(HttpStatusCode.NoContent, deleteDeepSeekResponse.StatusCode);
        var status = await client.GetFromJsonAsync<AdminStatusResponse>("/api/admin/status");
        Assert.NotNull(status);
        Assert.Equal(string.Empty, status.DefaultModelProfile);
        Assert.Equal(0, status.ModelProfileCount);
    }

    [Fact]
    public async Task ProfileNamesWithUnderscoreAndDashUseDistinctSecretFiles()
    {
        using var factory = CreateFactory();
        using var client = factory.CreateClient();

        using var underResponse = await client.PostAsJsonAsync(
            "/api/admin/model-profiles",
            new UpsertModelProfileRequest(
                "a_b",
                "OpenAICompatible",
                "https://api.openai.com/v1",
                "gpt-4.1",
                0.2,
                60,
                "sk-under",
                true));
        using var dashResponse = await client.PostAsJsonAsync(
            "/api/admin/model-profiles",
            new UpsertModelProfileRequest(
                "a-b",
                "OpenAICompatible",
                "https://api.deepseek.com/v1",
                "deepseek-chat",
                0.2,
                60,
                "sk-dash",
                false));
        var underSecretPath = Path.Combine(
            root,
            "secrets",
            AgentSecretStore.CreateSecretId("a_b") + ".bin");
        var dashSecretPath = Path.Combine(
            root,
            "secrets",
            AgentSecretStore.CreateSecretId("a-b") + ".bin");

        Assert.Equal(HttpStatusCode.OK, underResponse.StatusCode);
        Assert.Equal(HttpStatusCode.OK, dashResponse.StatusCode);
        Assert.NotEqual(underSecretPath, dashSecretPath);
        Assert.True(File.Exists(underSecretPath));
        Assert.True(File.Exists(dashSecretPath));

        using var deleteUnderResponse = await client.DeleteAsync("/api/admin/model-profiles/a_b");

        Assert.Equal(HttpStatusCode.NoContent, deleteUnderResponse.StatusCode);
        Assert.False(File.Exists(underSecretPath));
        Assert.True(File.Exists(dashSecretPath));
        var profiles = await client.GetFromJsonAsync<List<ModelProfileResponse>>("/api/admin/model-profiles");
        var profile = Assert.Single(profiles!);
        Assert.Equal("a-b", profile.Name);
        Assert.True(profile.HasApiKey);
    }

    private WebApplicationFactory<Program> CreateFactory()
    {
        return new WebApplicationFactory<Program>().WithWebHostBuilder(builder =>
        {
            builder.ConfigureServices(services =>
            {
                services.AddSingleton(new AgentLocalPaths(root));
                services.PostConfigure<RoadProtoAgentOptions>(options => options.ModelProfiles.Clear());
            });
        });
    }
}
