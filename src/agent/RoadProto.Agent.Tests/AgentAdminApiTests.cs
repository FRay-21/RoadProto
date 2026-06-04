using System.Net;
using System.Net.Http.Json;
using System.Text;
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
    public async Task AdminPageServesStaticConsole()
    {
        using var factory = CreateFactory();
        using var redirectClient = factory.CreateClient(new WebApplicationFactoryClientOptions
        {
            AllowAutoRedirect = false
        });

        using var response = await redirectClient.GetAsync("/admin");

        Assert.Equal(HttpStatusCode.Redirect, response.StatusCode);
        Assert.Equal("/admin/", response.Headers.Location?.OriginalString);

        using var client = factory.CreateClient();
        using var slashResponse = await client.GetAsync("/admin/");
        var slashHtml = await slashResponse.Content.ReadAsStringAsync();

        Assert.True(slashResponse.IsSuccessStatusCode, $"Unexpected /admin/ status code: {slashResponse.StatusCode}");
        Assert.Contains("text/html", slashResponse.Content.Headers.ContentType?.MediaType ?? string.Empty);
        Assert.Contains("RoadProto Agent", slashHtml, StringComparison.Ordinal);
        Assert.Contains("模型配置", slashHtml, StringComparison.Ordinal);

        var adminPageUri = new Uri("http://localhost/admin/");
        Assert.Equal("/admin/admin.css", new Uri(adminPageUri, "./admin.css").AbsolutePath);
        Assert.Equal("/admin/admin.js", new Uri(adminPageUri, "./admin.js").AbsolutePath);
    }

    [Fact]
    public async Task AdminStaticAssetsAreServed()
    {
        using var factory = CreateFactory();
        using var client = factory.CreateClient();

        using var cssResponse = await client.GetAsync("/admin/admin.css");
        using var jsResponse = await client.GetAsync("/admin/admin.js");
        var css = await cssResponse.Content.ReadAsStringAsync();
        var js = await jsResponse.Content.ReadAsStringAsync();

        Assert.True(cssResponse.IsSuccessStatusCode, $"CSS status code: {cssResponse.StatusCode}");
        Assert.True(jsResponse.IsSuccessStatusCode, $"JS status code: {jsResponse.StatusCode}");
        Assert.True(
            cssResponse.Content.Headers.ContentType?.MediaType?.Contains("css", StringComparison.OrdinalIgnoreCase) == true
                || css.Contains("admin-shell", StringComparison.Ordinal),
            "Admin CSS should be served with a stylesheet content type or known shell styles.");
        Assert.True(
            jsResponse.Content.Headers.ContentType?.MediaType?.Contains("javascript", StringComparison.OrdinalIgnoreCase) == true
                || js.Contains("/api/admin/status", StringComparison.Ordinal),
            "Admin JS should be served with a JavaScript content type or known status loader.");
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

    [Fact]
    public async Task CanUploadPatchAndDeleteSkillDocument()
    {
        using var factory = CreateFactory();
        using var client = factory.CreateClient();
        using var uploadContent = CreateMultipart("skills.md", "# Skill\n内容");

        using var uploadResponse = await client.PostAsync("/api/admin/skills/upload", uploadContent);

        Assert.Equal(HttpStatusCode.OK, uploadResponse.StatusCode);
        var uploaded = await uploadResponse.Content.ReadFromJsonAsync<DocumentResponse>();
        Assert.NotNull(uploaded);
        Assert.True(uploaded.Enabled);
        Assert.Equal("skills.md", uploaded.DisplayName);

        var skills = await client.GetFromJsonAsync<List<DocumentResponse>>("/api/admin/skills");
        var skill = Assert.Single(skills!);
        Assert.Equal(uploaded.Id, skill.Id);

        using var patchResponse = await client.PatchAsJsonAsync(
            $"/api/admin/skills/{uploaded.Id}",
            new UpdateDocumentRequest(false, "disabled skill"));

        Assert.Equal(HttpStatusCode.OK, patchResponse.StatusCode);
        var patched = await patchResponse.Content.ReadFromJsonAsync<DocumentResponse>();
        Assert.NotNull(patched);
        Assert.False(patched.Enabled);
        Assert.Equal("disabled skill", patched.DisplayName);

        using var deleteResponse = await client.DeleteAsync($"/api/admin/skills/{uploaded.Id}");

        Assert.Equal(HttpStatusCode.NoContent, deleteResponse.StatusCode);
        Assert.Empty(await client.GetFromJsonAsync<List<DocumentResponse>>("/api/admin/skills") ?? []);
    }

    [Fact]
    public async Task UploadKnowledgeRejectsNonMarkdown()
    {
        using var factory = CreateFactory();
        using var client = factory.CreateClient();
        using var uploadContent = CreateMultipart("notes.txt", "not markdown");

        using var response = await client.PostAsync("/api/admin/knowledge/upload", uploadContent);

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

    private static MultipartFormDataContent CreateMultipart(string fileName, string content)
    {
        var multipart = new MultipartFormDataContent();
        var fileContent = new ByteArrayContent(Encoding.UTF8.GetBytes(content));
        fileContent.Headers.ContentType = new System.Net.Http.Headers.MediaTypeHeaderValue("text/markdown");
        multipart.Add(fileContent, "file", fileName);
        return multipart;
    }
}
