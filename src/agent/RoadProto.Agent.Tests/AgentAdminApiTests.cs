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
