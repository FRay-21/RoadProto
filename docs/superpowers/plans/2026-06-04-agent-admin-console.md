# RoadProto Agent 管理控制台 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `RoadProto.Agent.Host` 中实现 `http://127.0.0.1:17831/admin` 独立浏览器管理控制台，用于配置模型、加密保存 API Key、导入 skill 和知识库 Markdown。

**Architecture:** 管理控制台完全落在 `.NET 8` Agent sidecar 中，新增本地配置存储、DPAPI 密钥存储、文档存储、管理 API 和静态前端。AutoCAD WPF 面板继续只负责聊天、工具确认和结果展示，不承载密钥或文档管理。

**Tech Stack:** ASP.NET Core Minimal API、静态 HTML/CSS/JavaScript、`System.Text.Json`、Windows DPAPI `ProtectedData`、xUnit、`WebApplicationFactory`。

---

## 文件结构

- Modify: `src/agent/RoadProto.Agent.Host/RoadProto.Agent.Host.csproj`
  - 添加 `System.Security.Cryptography.ProtectedData` 引用，供 API Key 加密使用。
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentAdminModels.cs`
  - 管理控制台 DTO、本地配置模型、文档模型和 API 响应模型。
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentLocalPaths.cs`
  - 统一计算 `%LOCALAPPDATA%\RoadProto\Agent\` 及测试覆盖用根目录。
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentSecretStore.cs`
  - 使用 Windows CurrentUser DPAPI 保存、读取、清除 API Key。
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentConfigurationStore.cs`
  - 读写 `config.json`，支持从 `appsettings` seed 初始 profile。
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentDocumentStore.cs`
  - 管理用户上传 skill 和知识库 Markdown。
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentAdminEndpoints.cs`
  - 注册 `/api/admin/*` 和 `/admin` 路由。
- Create: `src/agent/RoadProto.Agent.Host/Services/AgentPromptContextService.cs`
  - 组装 system prompt，包含内置 skill、用户 skill 和知识库。
- Modify: `src/agent/RoadProto.Agent.Host/Services/AgentChatService.cs`
  - 改为从 `AgentConfigurationStore` 读取当前默认 profile，并使用 `AgentPromptContextService`。
- Modify: `src/agent/RoadProto.Agent.Host/Providers/OpenAiCompatibleChatClient.cs`
  - 增加可直接传入 API Key 的完成和连接测试方法。
- Modify: `src/agent/RoadProto.Agent.Host/Program.cs`
  - 注册管理服务、静态页面和管理 API。
- Create: `src/agent/RoadProto.Agent.Host/wwwroot/admin/index.html`
  - 管理控制台页面结构。
- Create: `src/agent/RoadProto.Agent.Host/wwwroot/admin/admin.css`
  - 工具型管理界面样式。
- Create: `src/agent/RoadProto.Agent.Host/wwwroot/admin/admin.js`
  - 前端状态、模型配置、上传和连接测试逻辑。
- Create: `src/agent/RoadProto.Agent.Tests/AgentConfigurationStoreTests.cs`
  - 配置与密钥存储测试。
- Create: `src/agent/RoadProto.Agent.Tests/AgentDocumentStoreTests.cs`
  - Markdown 文档管理测试。
- Create: `src/agent/RoadProto.Agent.Tests/AgentAdminApiTests.cs`
  - 管理 API 和 `/admin` 页面测试。
- Create: `src/agent/RoadProto.Agent.Tests/AgentPromptContextServiceTests.cs`
  - prompt 组装测试。
- Modify: `src/agent/RoadProto.Agent.Tests/AgentChatApiTests.cs`
  - 调整测试工厂以使用临时配置目录，保留“无模型配置”行为。
- Modify: `docs/business/agent/设计软件原型Agent.md`
  - 增加管理控制台业务说明。
- Modify: `docs/modules/agent.md`
  - 增加管理控制台代码落点和 API。
- Modify: `docs/reuse/agent_tool_gateway.md`
  - 不改工具网关行为，只补充管理控制台不直接执行工具的边界。
- Modify: `docs/dev/version_log.md`
  - 记录本轮未发布 Agent 管理控制台。
- Modify: `README.md`
  - 增加 `/admin` 启动与使用说明。
- Modify: `tests/README.md`
  - 增加管理控制台自动化验证范围。

---

### Task 1: 本地配置与密钥存储

**Files:**
- Modify: `src/agent/RoadProto.Agent.Host/RoadProto.Agent.Host.csproj`
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentAdminModels.cs`
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentLocalPaths.cs`
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentSecretStore.cs`
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentConfigurationStore.cs`
- Create: `src/agent/RoadProto.Agent.Tests/AgentConfigurationStoreTests.cs`

- [ ] **Step 1: Write failing configuration and secret tests**

Create `src/agent/RoadProto.Agent.Tests/AgentConfigurationStoreTests.cs`:

```csharp
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Providers;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class AgentConfigurationStoreTests : IDisposable
{
    private readonly string root = Path.Combine(Path.GetTempPath(), "RoadProtoAgentTests", Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        if (Directory.Exists(root))
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [Fact]
    public async Task StoreSeedsProfilesFromOptionsWhenConfigDoesNotExist()
    {
        var options = new RoadProtoAgentOptions
        {
            ModelProfiles =
            [
                new ModelProfileOptions
                {
                    Name = "dashscope-qwen",
                    Provider = "OpenAICompatible",
                    BaseUrl = "https://dashscope.aliyuncs.com/compatible-mode/v1",
                    Model = "qwen-plus",
                    Temperature = 0.2,
                    TimeoutSeconds = 60,
                    ApiKeyEnvironmentVariable = "DASHSCOPE_API_KEY"
                }
            ]
        };
        var store = new AgentConfigurationStore(new AgentLocalPaths(root), options);

        var config = await store.LoadAsync();

        Assert.Equal("dashscope-qwen", config.DefaultModelProfile);
        var profile = Assert.Single(config.ModelProfiles);
        Assert.Equal("dashscope-qwen", profile.Name);
        Assert.False(profile.HasApiKey);
    }

    [Fact]
    public async Task StorePersistsProfilesAndDefaultProfile()
    {
        var store = new AgentConfigurationStore(new AgentLocalPaths(root), new RoadProtoAgentOptions());
        var config = new AgentRuntimeConfiguration
        {
            DefaultModelProfile = "openai",
            ModelProfiles =
            [
                new StoredModelProfile
                {
                    Name = "openai",
                    Provider = "OpenAICompatible",
                    BaseUrl = "https://api.openai.com/v1",
                    Model = "gpt-4.1",
                    Temperature = 0.2,
                    TimeoutSeconds = 60,
                    SecretId = "profile-openai",
                    ApiKeyMask = "sk-***1234"
                }
            ]
        };

        await store.SaveAsync(config);
        var reloaded = await store.LoadAsync();

        Assert.Equal("openai", reloaded.DefaultModelProfile);
        Assert.Equal("gpt-4.1", reloaded.ModelProfiles.Single().Model);
        Assert.True(File.Exists(Path.Combine(root, "config.json")));
    }

    [Fact]
    public async Task StoreBacksUpCorruptedConfigAndReturnsEmptyConfig()
    {
        Directory.CreateDirectory(root);
        await File.WriteAllTextAsync(Path.Combine(root, "config.json"), "{ invalid json");
        var store = new AgentConfigurationStore(new AgentLocalPaths(root), new RoadProtoAgentOptions());

        var config = await store.LoadAsync();

        Assert.Empty(config.ModelProfiles);
        Assert.True(Directory.GetFiles(root, "config.corrupt.*.json").Length == 1);
    }

    [Fact]
    public async Task SecretStoreEncryptsAndMasksApiKey()
    {
        var secrets = new AgentSecretStore(new AgentLocalPaths(root));

        await secrets.SaveAsync("profile-openai", "sk-test-secret-123456");
        var raw = await File.ReadAllBytesAsync(Path.Combine(root, "secrets", "profile-openai.bin"));
        var rawText = System.Text.Encoding.UTF8.GetString(raw);
        var decrypted = await secrets.ReadAsync("profile-openai");
        var mask = AgentSecretStore.Mask("sk-test-secret-123456");

        Assert.DoesNotContain("sk-test-secret-123456", rawText);
        Assert.Equal("sk-test-secret-123456", decrypted);
        Assert.Equal("sk-***3456", mask);
    }
}
```

- [ ] **Step 2: Run tests and verify they fail**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AgentConfigurationStoreTests
```

Expected: FAIL because `RoadProto.Agent.Host.Admin` types do not exist.

- [ ] **Step 3: Add DPAPI package reference**

Modify `src/agent/RoadProto.Agent.Host/RoadProto.Agent.Host.csproj`:

```xml
<Project Sdk="Microsoft.NET.Sdk.Web">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <AssemblyName>RoadProto.Agent.Host</AssemblyName>
    <RootNamespace>RoadProto.Agent.Host</RootNamespace>
    <OutputPath>$(MSBuildProjectDirectory)\..\..\..\artifacts\agent\$(Configuration)\</OutputPath>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="System.Security.Cryptography.ProtectedData" Version="8.0.0" />
  </ItemGroup>
</Project>
```

- [ ] **Step 4: Create configuration models**

Create `src/agent/RoadProto.Agent.Host/Admin/AgentAdminModels.cs`:

```csharp
namespace RoadProto.Agent.Host.Admin;

public sealed class AgentRuntimeConfiguration
{
    public string DefaultModelProfile { get; set; } = string.Empty;
    public List<StoredModelProfile> ModelProfiles { get; set; } = new();
    public List<StoredAgentDocument> Skills { get; set; } = new();
    public List<StoredAgentDocument> Knowledge { get; set; } = new();
}

public sealed class StoredModelProfile
{
    public string Name { get; set; } = string.Empty;
    public string Provider { get; set; } = "OpenAICompatible";
    public string BaseUrl { get; set; } = string.Empty;
    public string Model { get; set; } = string.Empty;
    public double Temperature { get; set; } = 0.2;
    public int TimeoutSeconds { get; set; } = 60;
    public string SecretId { get; set; } = string.Empty;
    public string ApiKeyMask { get; set; } = string.Empty;
    public bool HasApiKey => !string.IsNullOrWhiteSpace(SecretId);
}

public sealed class StoredAgentDocument
{
    public string Id { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;
    public string FileName { get; set; } = string.Empty;
    public bool Enabled { get; set; } = true;
    public bool BuiltIn { get; set; }
    public long SizeBytes { get; set; }
    public DateTimeOffset UpdatedAt { get; set; } = DateTimeOffset.UtcNow;
}

public sealed record AdminStatusResponse(
    string StorageRoot,
    string DefaultModelProfile,
    int ModelProfileCount,
    int EnabledSkillCount,
    int EnabledKnowledgeCount);

public sealed record UpsertModelProfileRequest(
    string Name,
    string Provider,
    string BaseUrl,
    string Model,
    double Temperature,
    int TimeoutSeconds,
    string? ApiKey,
    bool IsDefault);

public sealed record ModelProfileResponse(
    string Name,
    string Provider,
    string BaseUrl,
    string Model,
    double Temperature,
    int TimeoutSeconds,
    bool HasApiKey,
    string ApiKeyMask,
    bool IsDefault);

public sealed record DocumentResponse(
    string Id,
    string DisplayName,
    bool Enabled,
    bool BuiltIn,
    long SizeBytes,
    DateTimeOffset UpdatedAt);

public sealed record UpdateDocumentRequest(bool Enabled, string? DisplayName);
```

- [ ] **Step 5: Create path helper**

Create `src/agent/RoadProto.Agent.Host/Admin/AgentLocalPaths.cs`:

```csharp
namespace RoadProto.Agent.Host.Admin;

public sealed class AgentLocalPaths
{
    public AgentLocalPaths()
        : this(Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "RoadProto",
            "Agent"))
    {
    }

    public AgentLocalPaths(string root)
    {
        Root = root;
    }

    public string Root { get; }
    public string ConfigPath => Path.Combine(Root, "config.json");
    public string SecretsDirectory => Path.Combine(Root, "secrets");
    public string SkillsDirectory => Path.Combine(Root, "skills");
    public string KnowledgeDirectory => Path.Combine(Root, "knowledge");

    public void Ensure()
    {
        Directory.CreateDirectory(Root);
        Directory.CreateDirectory(SecretsDirectory);
        Directory.CreateDirectory(SkillsDirectory);
        Directory.CreateDirectory(KnowledgeDirectory);
    }
}
```

- [ ] **Step 6: Create secret store**

Create `src/agent/RoadProto.Agent.Host/Admin/AgentSecretStore.cs`:

```csharp
using System.Security.Cryptography;
using System.Text;

namespace RoadProto.Agent.Host.Admin;

public sealed class AgentSecretStore
{
    private readonly AgentLocalPaths paths;

    public AgentSecretStore(AgentLocalPaths paths)
    {
        this.paths = paths;
    }

    public async Task SaveAsync(string secretId, string apiKey, CancellationToken cancellationToken = default)
    {
        paths.Ensure();
        var path = GetSecretPath(secretId);
        var protectedBytes = ProtectedData.Protect(
            Encoding.UTF8.GetBytes(apiKey),
            optionalEntropy: null,
            DataProtectionScope.CurrentUser);
        await File.WriteAllBytesAsync(path, protectedBytes, cancellationToken).ConfigureAwait(false);
    }

    public async Task<string?> ReadAsync(string secretId, CancellationToken cancellationToken = default)
    {
        var path = GetSecretPath(secretId);
        if (!File.Exists(path))
        {
            return null;
        }

        var protectedBytes = await File.ReadAllBytesAsync(path, cancellationToken).ConfigureAwait(false);
        var bytes = ProtectedData.Unprotect(
            protectedBytes,
            optionalEntropy: null,
            DataProtectionScope.CurrentUser);
        return Encoding.UTF8.GetString(bytes);
    }

    public Task DeleteAsync(string secretId)
    {
        var path = GetSecretPath(secretId);
        if (File.Exists(path))
        {
            File.Delete(path);
        }

        return Task.CompletedTask;
    }

    public static string CreateSecretId(string profileName)
    {
        return "profile-" + new string(profileName
            .Trim()
            .ToLowerInvariant()
            .Select(ch => char.IsLetterOrDigit(ch) ? ch : '-')
            .ToArray()).Trim('-');
    }

    public static string Mask(string apiKey)
    {
        if (string.IsNullOrWhiteSpace(apiKey))
        {
            return string.Empty;
        }

        var suffix = apiKey.Length <= 4 ? apiKey : apiKey[^4..];
        var prefix = apiKey.StartsWith("sk-", StringComparison.OrdinalIgnoreCase) ? "sk-" : string.Empty;
        return prefix + "***" + suffix;
    }

    private string GetSecretPath(string secretId)
    {
        var fileName = Path.GetFileName(secretId);
        if (!string.Equals(fileName, secretId, StringComparison.Ordinal))
        {
            throw new InvalidOperationException("SecretId 不能包含路径片段。");
        }

        return Path.Combine(paths.SecretsDirectory, fileName + ".bin");
    }
}
```

- [ ] **Step 7: Create configuration store**

Create `src/agent/RoadProto.Agent.Host/Admin/AgentConfigurationStore.cs`:

```csharp
using System.Text.Json;
using RoadProto.Agent.Host.Providers;

namespace RoadProto.Agent.Host.Admin;

public sealed class AgentConfigurationStore
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };

    private readonly AgentLocalPaths paths;
    private readonly RoadProtoAgentOptions seedOptions;
    private readonly SemaphoreSlim gate = new(1, 1);

    public AgentConfigurationStore(AgentLocalPaths paths, RoadProtoAgentOptions seedOptions)
    {
        this.paths = paths;
        this.seedOptions = seedOptions;
    }

    public async Task<AgentRuntimeConfiguration> LoadAsync(CancellationToken cancellationToken = default)
    {
        await gate.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            paths.Ensure();
            if (!File.Exists(paths.ConfigPath))
            {
                return SeedFromOptions();
            }

            try
            {
                await using var stream = File.OpenRead(paths.ConfigPath);
                return await JsonSerializer.DeserializeAsync<AgentRuntimeConfiguration>(
                    stream,
                    JsonOptions,
                    cancellationToken).ConfigureAwait(false) ?? new AgentRuntimeConfiguration();
            }
            catch (JsonException)
            {
                var backup = Path.Combine(
                    paths.Root,
                    $"config.corrupt.{DateTimeOffset.UtcNow:yyyyMMddHHmmss}.json");
                File.Move(paths.ConfigPath, backup, overwrite: true);
                return new AgentRuntimeConfiguration();
            }
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task SaveAsync(AgentRuntimeConfiguration config, CancellationToken cancellationToken = default)
    {
        await gate.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            paths.Ensure();
            await using var stream = File.Create(paths.ConfigPath);
            await JsonSerializer.SerializeAsync(stream, config, JsonOptions, cancellationToken).ConfigureAwait(false);
        }
        finally
        {
            gate.Release();
        }
    }

    private AgentRuntimeConfiguration SeedFromOptions()
    {
        var profiles = seedOptions.ModelProfiles
            .Where(item => !string.IsNullOrWhiteSpace(item.Name))
            .Select(item => new StoredModelProfile
            {
                Name = item.Name.Trim(),
                Provider = string.IsNullOrWhiteSpace(item.Provider) ? "OpenAICompatible" : item.Provider.Trim(),
                BaseUrl = item.BaseUrl.Trim(),
                Model = item.Model.Trim(),
                Temperature = item.Temperature,
                TimeoutSeconds = item.TimeoutSeconds <= 0 ? 60 : item.TimeoutSeconds
            })
            .ToList();

        return new AgentRuntimeConfiguration
        {
            DefaultModelProfile = profiles.FirstOrDefault()?.Name ?? string.Empty,
            ModelProfiles = profiles
        };
    }
}
```

- [ ] **Step 8: Run tests and verify pass**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AgentConfigurationStoreTests
```

Expected: PASS for all `AgentConfigurationStoreTests`.

- [ ] **Step 9: Commit Task 1**

Run:

```powershell
git add src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj src\agent\RoadProto.Agent.Host\Admin src\agent\RoadProto.Agent.Tests\AgentConfigurationStoreTests.cs
git commit -m "feat: add agent admin configuration store"
```

---

### Task 2: 模型 Profile 管理 API

**Files:**
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentAdminEndpoints.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Providers/OpenAiCompatibleChatClient.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Program.cs`
- Create: `src/agent/RoadProto.Agent.Tests/AgentAdminApiTests.cs`

- [ ] **Step 1: Write failing admin API tests**

Create `src/agent/RoadProto.Agent.Tests/AgentAdminApiTests.cs`:

```csharp
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
```

- [ ] **Step 2: Run tests and verify they fail**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AgentAdminApiTests
```

Expected: FAIL because admin endpoints are not mapped.

- [ ] **Step 3: Extend model client for explicit API Key and test call**

Modify `src/agent/RoadProto.Agent.Host/Providers/OpenAiCompatibleChatClient.cs` by adding overloads while preserving existing behavior:

```csharp
public async Task<string> CompleteAsync(
    ModelProfileOptions profile,
    string systemPrompt,
    string userMessage,
    string? apiKey,
    CancellationToken cancellationToken = default)
{
    if (string.IsNullOrWhiteSpace(apiKey))
    {
        return "模型 API Key 未配置，当前可继续使用本地规则工具（例如创建路基模板）。";
    }

    return await CompleteWithApiKeyAsync(profile, systemPrompt, userMessage, apiKey, cancellationToken)
        .ConfigureAwait(false);
}

public async Task<string> TestConnectionAsync(
    ModelProfileOptions profile,
    string? apiKey,
    CancellationToken cancellationToken = default)
{
    return await CompleteAsync(
        profile,
        "你是 RoadProto Agent 连接测试助手。请只回复 OK。",
        "请回复 OK",
        apiKey,
        cancellationToken).ConfigureAwait(false);
}
```

Move the current request-sending body into a private method:

```csharp
private async Task<string> CompleteWithApiKeyAsync(
    ModelProfileOptions profile,
    string systemPrompt,
    string userMessage,
    string apiKey,
    CancellationToken cancellationToken)
{
    if (string.IsNullOrWhiteSpace(profile.BaseUrl))
    {
        return "模型 BaseUrl 未配置，当前可继续使用本地规则工具（例如创建路基模板）。";
    }

    if (string.IsNullOrWhiteSpace(profile.Model))
    {
        return "模型名称未配置，当前可继续使用本地规则工具（例如创建路基模板）。";
    }

    if (!TryCreateEndpoint(profile.BaseUrl, out var endpoint))
    {
        return "模型 BaseUrl 格式无效，当前可继续使用本地规则工具（例如创建路基模板）。";
    }

    using var request = new HttpRequestMessage(HttpMethod.Post, endpoint);
    request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", apiKey);
    request.Content = JsonContent.Create(
        new
        {
            model = profile.Model,
            temperature = profile.Temperature,
            stream = false,
            messages = new object[]
            {
                new { role = "system", content = systemPrompt },
                new { role = "user", content = userMessage }
            }
        },
        options: JsonOptions);

    try
    {
        using var timeout = CreateTimeout(profile.TimeoutSeconds, cancellationToken);
        using var response = await httpClient.SendAsync(request, timeout.Token).ConfigureAwait(false);
        if (!response.IsSuccessStatusCode)
        {
            return $"模型服务返回错误：{(int)response.StatusCode} {response.ReasonPhrase}";
        }

        await using var stream = await response.Content.ReadAsStreamAsync(timeout.Token).ConfigureAwait(false);
        using var document = await JsonDocument.ParseAsync(stream, cancellationToken: timeout.Token).ConfigureAwait(false);
        return ExtractReply(document.RootElement);
    }
    catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
    {
        return "模型服务请求超时，当前可继续使用本地规则工具（例如创建路基模板）。";
    }
    catch (HttpRequestException ex)
    {
        return $"模型服务请求失败：{ex.Message}";
    }
    catch (JsonException)
    {
        return "模型服务响应格式无法解析。";
    }
}
```

Keep the original environment-variable `CompleteAsync` as a wrapper that resolves the environment variable then calls `CompleteWithApiKeyAsync`.

- [ ] **Step 4: Create admin endpoint mapper**

Create `src/agent/RoadProto.Agent.Host/Admin/AgentAdminEndpoints.cs`:

```csharp
using RoadProto.Agent.Host.Providers;

namespace RoadProto.Agent.Host.Admin;

public static class AgentAdminEndpoints
{
    public static void MapAgentAdminEndpoints(this WebApplication app)
    {
        app.MapGet("/api/admin/status", async (AgentConfigurationStore store, AgentLocalPaths paths) =>
        {
            var config = await store.LoadAsync().ConfigureAwait(false);
            return Results.Ok(new AdminStatusResponse(
                paths.Root,
                config.DefaultModelProfile,
                config.ModelProfiles.Count,
                config.Skills.Count(item => item.Enabled),
                config.Knowledge.Count(item => item.Enabled)));
        });

        app.MapGet("/api/admin/model-profiles", async (AgentConfigurationStore store) =>
        {
            var config = await store.LoadAsync().ConfigureAwait(false);
            return Results.Ok(config.ModelProfiles.Select(profile => ToResponse(profile, config.DefaultModelProfile)));
        });

        app.MapPost("/api/admin/model-profiles", async (
            UpsertModelProfileRequest request,
            AgentConfigurationStore store,
            AgentSecretStore secrets) =>
        {
            var validation = ValidateProfile(request);
            if (validation != null)
            {
                return Results.BadRequest(new { message = validation });
            }

            var config = await store.LoadAsync().ConfigureAwait(false);
            var existing = config.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, request.Name.Trim(), StringComparison.OrdinalIgnoreCase));
            if (existing == null)
            {
                existing = new StoredModelProfile { Name = request.Name.Trim() };
                config.ModelProfiles.Add(existing);
            }

            existing.Provider = string.IsNullOrWhiteSpace(request.Provider) ? "OpenAICompatible" : request.Provider.Trim();
            existing.BaseUrl = request.BaseUrl.Trim().TrimEnd('/');
            existing.Model = request.Model.Trim();
            existing.Temperature = request.Temperature;
            existing.TimeoutSeconds = request.TimeoutSeconds <= 0 ? 60 : request.TimeoutSeconds;

            if (!string.IsNullOrWhiteSpace(request.ApiKey))
            {
                existing.SecretId = AgentSecretStore.CreateSecretId(existing.Name);
                existing.ApiKeyMask = AgentSecretStore.Mask(request.ApiKey);
                await secrets.SaveAsync(existing.SecretId, request.ApiKey).ConfigureAwait(false);
            }

            if (request.IsDefault || string.IsNullOrWhiteSpace(config.DefaultModelProfile))
            {
                config.DefaultModelProfile = existing.Name;
            }

            await store.SaveAsync(config).ConfigureAwait(false);
            return Results.Ok(ToResponse(existing, config.DefaultModelProfile));
        });

        app.MapDelete("/api/admin/model-profiles/{name}", async (
            string name,
            AgentConfigurationStore store,
            AgentSecretStore secrets) =>
        {
            var config = await store.LoadAsync().ConfigureAwait(false);
            var existing = config.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, name, StringComparison.OrdinalIgnoreCase));
            if (existing == null)
            {
                return Results.NotFound();
            }

            config.ModelProfiles.Remove(existing);
            if (!string.IsNullOrWhiteSpace(existing.SecretId))
            {
                await secrets.DeleteAsync(existing.SecretId).ConfigureAwait(false);
            }

            if (string.Equals(config.DefaultModelProfile, existing.Name, StringComparison.OrdinalIgnoreCase))
            {
                config.DefaultModelProfile = config.ModelProfiles.FirstOrDefault()?.Name ?? string.Empty;
            }

            await store.SaveAsync(config).ConfigureAwait(false);
            return Results.NoContent();
        });

        app.MapPost("/api/admin/model-profiles/{name}/default", async (
            string name,
            AgentConfigurationStore store) =>
        {
            var config = await store.LoadAsync().ConfigureAwait(false);
            var existing = config.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, name, StringComparison.OrdinalIgnoreCase));
            if (existing == null)
            {
                return Results.NotFound();
            }

            config.DefaultModelProfile = existing.Name;
            await store.SaveAsync(config).ConfigureAwait(false);
            return Results.Ok(ToResponse(existing, config.DefaultModelProfile));
        });
    }

    private static ModelProfileResponse ToResponse(StoredModelProfile profile, string defaultName)
    {
        return new ModelProfileResponse(
            profile.Name,
            profile.Provider,
            profile.BaseUrl,
            profile.Model,
            profile.Temperature,
            profile.TimeoutSeconds,
            profile.HasApiKey,
            profile.ApiKeyMask,
            string.Equals(profile.Name, defaultName, StringComparison.OrdinalIgnoreCase));
    }

    private static string? ValidateProfile(UpsertModelProfileRequest request)
    {
        if (string.IsNullOrWhiteSpace(request.Name))
        {
            return "Profile 名称不能为空。";
        }

        if (request.Name.Any(ch => !(char.IsLetterOrDigit(ch) || ch == '-' || ch == '_')))
        {
            return "Profile 名称只能包含字母、数字、短横线和下划线。";
        }

        if (!Uri.TryCreate(request.BaseUrl, UriKind.Absolute, out _))
        {
            return "Base URL 格式无效。";
        }

        if (string.IsNullOrWhiteSpace(request.Model))
        {
            return "模型名称不能为空。";
        }

        return null;
    }
}
```

- [ ] **Step 5: Register services and endpoints**

Modify `src/agent/RoadProto.Agent.Host/Program.cs`:

```csharp
builder.Services.AddSingleton<AgentLocalPaths>();
builder.Services.AddSingleton(serviceProvider =>
{
    var options = serviceProvider
        .GetRequiredService<Microsoft.Extensions.Options.IOptions<RoadProtoAgentOptions>>()
        .Value;
    var paths = serviceProvider.GetRequiredService<AgentLocalPaths>();
    return new AgentConfigurationStore(paths, options);
});
builder.Services.AddSingleton<AgentSecretStore>();
```

After `var app = builder.Build();`, add:

```csharp
app.MapAgentAdminEndpoints();
```

Add `using RoadProto.Agent.Host.Admin;` at the top.

- [ ] **Step 6: Run admin API tests**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AgentAdminApiTests
```

Expected: PASS.

- [ ] **Step 7: Commit Task 2**

Run:

```powershell
git add src\agent\RoadProto.Agent.Host\Admin src\agent\RoadProto.Agent.Host\Program.cs src\agent\RoadProto.Agent.Host\Providers\OpenAiCompatibleChatClient.cs src\agent\RoadProto.Agent.Tests\AgentAdminApiTests.cs
git commit -m "feat: add agent admin model profile api"
```

---

### Task 3: Skill、知识库与 prompt 组装

**Files:**
- Create: `src/agent/RoadProto.Agent.Host/Admin/AgentDocumentStore.cs`
- Create: `src/agent/RoadProto.Agent.Host/Services/AgentPromptContextService.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Admin/AgentAdminEndpoints.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Services/AgentChatService.cs`
- Create: `src/agent/RoadProto.Agent.Tests/AgentDocumentStoreTests.cs`
- Create: `src/agent/RoadProto.Agent.Tests/AgentPromptContextServiceTests.cs`

- [ ] **Step 1: Write failing document store tests**

Create `src/agent/RoadProto.Agent.Tests/AgentDocumentStoreTests.cs`:

```csharp
using Microsoft.AspNetCore.Http;
using RoadProto.Agent.Host.Admin;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class AgentDocumentStoreTests : IDisposable
{
    private readonly string root = Path.Combine(Path.GetTempPath(), "RoadProtoAgentDocTests", Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        if (Directory.Exists(root))
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [Fact]
    public async Task UploadMarkdownSkillCreatesSafeDocument()
    {
        var config = new AgentConfigurationStore(new AgentLocalPaths(root), new());
        var store = new AgentDocumentStore(new AgentLocalPaths(root), config);
        var file = CreateFile("../../../bad.md", "# Skill\n\n内容");

        var saved = await store.UploadAsync(AgentDocumentKind.Skill, file);
        var loaded = await config.LoadAsync();

        Assert.Equal("bad.md", saved.DisplayName);
        Assert.Single(loaded.Skills);
        Assert.True(File.Exists(Path.Combine(root, "skills", saved.FileName)));
    }

    [Fact]
    public async Task UploadRejectsNonMarkdown()
    {
        var config = new AgentConfigurationStore(new AgentLocalPaths(root), new());
        var store = new AgentDocumentStore(new AgentLocalPaths(root), config);
        var file = CreateFile("notes.txt", "plain");

        await Assert.ThrowsAsync<InvalidOperationException>(() =>
            store.UploadAsync(AgentDocumentKind.Knowledge, file));
    }

    [Fact]
    public async Task CanDisableAndDeleteKnowledgeDocument()
    {
        var config = new AgentConfigurationStore(new AgentLocalPaths(root), new());
        var store = new AgentDocumentStore(new AgentLocalPaths(root), config);
        var saved = await store.UploadAsync(AgentDocumentKind.Knowledge, CreateFile("rules.md", "# Rules"));

        await store.UpdateAsync(AgentDocumentKind.Knowledge, saved.Id, enabled: false, displayName: "规则");
        await store.DeleteAsync(AgentDocumentKind.Knowledge, saved.Id);
        var loaded = await config.LoadAsync();

        Assert.Empty(loaded.Knowledge);
        Assert.Empty(Directory.GetFiles(Path.Combine(root, "knowledge")));
    }

    private static IFormFile CreateFile(string name, string content)
    {
        var bytes = System.Text.Encoding.UTF8.GetBytes(content);
        return new FormFile(new MemoryStream(bytes), 0, bytes.Length, "file", name);
    }
}
```

- [ ] **Step 2: Write failing prompt context tests**

Create `src/agent/RoadProto.Agent.Tests/AgentPromptContextServiceTests.cs`:

```csharp
using Microsoft.AspNetCore.Http;
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Services;
using RoadProto.Agent.Host.Skills;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class AgentPromptContextServiceTests : IDisposable
{
    private readonly string root = Path.Combine(Path.GetTempPath(), "RoadProtoAgentPromptTests", Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        if (Directory.Exists(root))
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [Fact]
    public async Task PromptIncludesEnabledSkillAndKnowledge()
    {
        var paths = new AgentLocalPaths(root);
        var config = new AgentConfigurationStore(paths, new());
        var documents = new AgentDocumentStore(paths, config);
        await documents.UploadAsync(AgentDocumentKind.Skill, CreateFile("subgrade.md", "# 用户 Skill\n创建路基模板规则"));
        await documents.UploadAsync(AgentDocumentKind.Knowledge, CreateFile("knowledge.md", "# 知识库\n道路等级解释"));
        var service = new AgentPromptContextService(config, documents, new MarkdownSkillRepository(root));

        var prompt = await service.CreateSystemPromptAsync();

        Assert.Contains("RoadProto 设计软件原型 Agent", prompt);
        Assert.Contains("创建路基模板规则", prompt);
        Assert.Contains("道路等级解释", prompt);
    }

    private static IFormFile CreateFile(string name, string content)
    {
        var bytes = System.Text.Encoding.UTF8.GetBytes(content);
        return new FormFile(new MemoryStream(bytes), 0, bytes.Length, "file", name);
    }
}
```

- [ ] **Step 3: Run tests and verify they fail**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter "AgentDocumentStoreTests|AgentPromptContextServiceTests"
```

Expected: FAIL because document and prompt services do not exist.

- [ ] **Step 4: Create document store**

Create `src/agent/RoadProto.Agent.Host/Admin/AgentDocumentStore.cs`:

```csharp
using Microsoft.AspNetCore.Http;

namespace RoadProto.Agent.Host.Admin;

public enum AgentDocumentKind
{
    Skill,
    Knowledge
}

public sealed class AgentDocumentStore
{
    private const long MaxMarkdownBytes = 2 * 1024 * 1024;
    private readonly AgentLocalPaths paths;
    private readonly AgentConfigurationStore configStore;

    public AgentDocumentStore(AgentLocalPaths paths, AgentConfigurationStore configStore)
    {
        this.paths = paths;
        this.configStore = configStore;
    }

    public async Task<IReadOnlyList<StoredAgentDocument>> ListAsync(AgentDocumentKind kind)
    {
        var config = await configStore.LoadAsync().ConfigureAwait(false);
        return SelectList(config, kind).ToList();
    }

    public async Task<StoredAgentDocument> UploadAsync(
        AgentDocumentKind kind,
        IFormFile file,
        CancellationToken cancellationToken = default)
    {
        if (file.Length <= 0 || file.Length > MaxMarkdownBytes)
        {
            throw new InvalidOperationException("Markdown 文件大小必须在 1 字节到 2 MB 之间。");
        }

        var displayName = Path.GetFileName(file.FileName);
        if (!displayName.EndsWith(".md", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("只允许上传 .md 文件。");
        }

        paths.Ensure();
        var id = Guid.NewGuid().ToString("N");
        var fileName = id + ".md";
        var directory = GetDirectory(kind);
        Directory.CreateDirectory(directory);
        var path = Path.Combine(directory, fileName);
        await using (var output = File.Create(path))
        {
            await file.CopyToAsync(output, cancellationToken).ConfigureAwait(false);
        }

        var document = new StoredAgentDocument
        {
            Id = id,
            DisplayName = displayName,
            FileName = fileName,
            Enabled = true,
            BuiltIn = false,
            SizeBytes = file.Length,
            UpdatedAt = DateTimeOffset.UtcNow
        };

        var config = await configStore.LoadAsync(cancellationToken).ConfigureAwait(false);
        SelectList(config, kind).Add(document);
        await configStore.SaveAsync(config, cancellationToken).ConfigureAwait(false);
        return document;
    }

    public async Task UpdateAsync(AgentDocumentKind kind, string id, bool enabled, string? displayName)
    {
        var config = await configStore.LoadAsync().ConfigureAwait(false);
        var document = SelectList(config, kind).FirstOrDefault(item => item.Id == id);
        if (document == null)
        {
            throw new InvalidOperationException("文档不存在。");
        }

        document.Enabled = enabled;
        if (!string.IsNullOrWhiteSpace(displayName))
        {
            document.DisplayName = displayName.Trim();
        }

        document.UpdatedAt = DateTimeOffset.UtcNow;
        await configStore.SaveAsync(config).ConfigureAwait(false);
    }

    public async Task DeleteAsync(AgentDocumentKind kind, string id)
    {
        var config = await configStore.LoadAsync().ConfigureAwait(false);
        var list = SelectList(config, kind);
        var document = list.FirstOrDefault(item => item.Id == id);
        if (document == null)
        {
            return;
        }

        if (document.BuiltIn)
        {
            throw new InvalidOperationException("内置 skill 不允许删除。");
        }

        list.Remove(document);
        var path = Path.Combine(GetDirectory(kind), Path.GetFileName(document.FileName));
        if (File.Exists(path))
        {
            File.Delete(path);
        }

        await configStore.SaveAsync(config).ConfigureAwait(false);
    }

    public async Task<IReadOnlyList<string>> ReadEnabledContentsAsync(AgentDocumentKind kind)
    {
        var documents = await ListAsync(kind).ConfigureAwait(false);
        var result = new List<string>();
        foreach (var document in documents.Where(item => item.Enabled))
        {
            var path = Path.Combine(GetDirectory(kind), Path.GetFileName(document.FileName));
            if (File.Exists(path))
            {
                result.Add(await File.ReadAllTextAsync(path, System.Text.Encoding.UTF8).ConfigureAwait(false));
            }
        }

        return result;
    }

    private string GetDirectory(AgentDocumentKind kind)
    {
        return kind == AgentDocumentKind.Skill ? paths.SkillsDirectory : paths.KnowledgeDirectory;
    }

    private static List<StoredAgentDocument> SelectList(AgentRuntimeConfiguration config, AgentDocumentKind kind)
    {
        return kind == AgentDocumentKind.Skill ? config.Skills : config.Knowledge;
    }
}
```

- [ ] **Step 5: Create prompt context service**

Create `src/agent/RoadProto.Agent.Host/Services/AgentPromptContextService.cs`:

```csharp
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Skills;

namespace RoadProto.Agent.Host.Services;

public sealed class AgentPromptContextService
{
    private const int MaxContextChars = 30000;
    private const string DefaultSystemPrompt =
        "你是 RoadProto 设计软件原型 Agent。回答软件操作和道路设计原型问题。"
        + "模型返回只作为普通回复，不自动执行工具。可执行工具由本地 planner 生成并由用户确认。";

    private readonly AgentConfigurationStore configStore;
    private readonly AgentDocumentStore documentStore;
    private readonly MarkdownSkillRepository builtInSkills;

    public AgentPromptContextService(
        AgentConfigurationStore configStore,
        AgentDocumentStore documentStore,
        MarkdownSkillRepository builtInSkills)
    {
        this.configStore = configStore;
        this.documentStore = documentStore;
        this.builtInSkills = builtInSkills;
    }

    public async Task<string> CreateSystemPromptAsync(CancellationToken cancellationToken = default)
    {
        var parts = new List<string> { DefaultSystemPrompt };
        var builtInSkill = builtInSkills.ReadSubgradeTemplateCreateSkill();
        if (!string.IsNullOrWhiteSpace(builtInSkill))
        {
            parts.Add("内置 skill：\n" + builtInSkill);
        }

        var userSkills = await documentStore.ReadEnabledContentsAsync(AgentDocumentKind.Skill).ConfigureAwait(false);
        if (userSkills.Count > 0)
        {
            parts.Add("用户 skill：\n" + string.Join("\n\n---\n\n", userSkills));
        }

        var knowledge = await documentStore.ReadEnabledContentsAsync(AgentDocumentKind.Knowledge).ConfigureAwait(false);
        if (knowledge.Count > 0)
        {
            parts.Add("知识库：\n" + string.Join("\n\n---\n\n", knowledge));
        }

        var prompt = string.Join("\n\n", parts);
        return prompt.Length <= MaxContextChars ? prompt : prompt[..MaxContextChars];
    }
}
```

- [ ] **Step 6: Register document API endpoints**

Append to `MapAgentAdminEndpoints` in `src/agent/RoadProto.Agent.Host/Admin/AgentAdminEndpoints.cs`:

```csharp
MapDocumentEndpoints(app, "/api/admin/skills", AgentDocumentKind.Skill);
MapDocumentEndpoints(app, "/api/admin/knowledge", AgentDocumentKind.Knowledge);
```

Add helper methods:

```csharp
private static void MapDocumentEndpoints(WebApplication app, string prefix, AgentDocumentKind kind)
{
    app.MapGet(prefix, async (AgentDocumentStore documents) =>
    {
        var items = await documents.ListAsync(kind).ConfigureAwait(false);
        return Results.Ok(items.Select(ToDocumentResponse));
    });

    app.MapPost(prefix + "/upload", async (
        IFormFile file,
        AgentDocumentStore documents) =>
    {
        try
        {
            var saved = await documents.UploadAsync(kind, file).ConfigureAwait(false);
            return Results.Ok(ToDocumentResponse(saved));
        }
        catch (InvalidOperationException error)
        {
            return Results.BadRequest(new { message = error.Message });
        }
    }).DisableAntiforgery();

    app.MapPatch(prefix + "/{id}", async (
        string id,
        UpdateDocumentRequest request,
        AgentDocumentStore documents) =>
    {
        try
        {
            await documents.UpdateAsync(kind, id, request.Enabled, request.DisplayName).ConfigureAwait(false);
            return Results.NoContent();
        }
        catch (InvalidOperationException error)
        {
            return Results.BadRequest(new { message = error.Message });
        }
    });

    app.MapDelete(prefix + "/{id}", async (
        string id,
        AgentDocumentStore documents) =>
    {
        try
        {
            await documents.DeleteAsync(kind, id).ConfigureAwait(false);
            return Results.NoContent();
        }
        catch (InvalidOperationException error)
        {
            return Results.BadRequest(new { message = error.Message });
        }
    });
}

private static DocumentResponse ToDocumentResponse(StoredAgentDocument document)
{
    return new DocumentResponse(
        document.Id,
        document.DisplayName,
        document.Enabled,
        document.BuiltIn,
        document.SizeBytes,
        document.UpdatedAt);
}
```

- [ ] **Step 7: Wire prompt service into chat**

Modify `src/agent/RoadProto.Agent.Host/Services/AgentChatService.cs`:

```csharp
private readonly AgentConfigurationStore configStore;
private readonly AgentSecretStore secretStore;
private readonly AgentPromptContextService promptContext;
```

Constructor becomes:

```csharp
public AgentChatService(
    SubgradeTemplateToolPlanner planner,
    OpenAiCompatibleChatClient modelClient,
    AgentConfigurationStore configStore,
    AgentSecretStore secretStore,
    AgentPromptContextService promptContext)
{
    this.planner = planner;
    this.modelClient = modelClient;
    this.configStore = configStore;
    this.secretStore = secretStore;
    this.promptContext = promptContext;
}
```

Replace profile lookup and model call with:

```csharp
var config = await configStore.LoadAsync(cancellationToken).ConfigureAwait(false);
var profile = FindProfile(config, request.ModelProfile);
if (profile == null)
{
    return new AgentChatResponse(
        "当前未配置模型。你仍可使用首个本地规则工具：创建路基模板。",
        null,
        false);
}

if (!string.Equals(profile.Provider, "OpenAICompatible", StringComparison.OrdinalIgnoreCase))
{
    return new AgentChatResponse(
        $"当前模型 Provider {profile.Provider} 暂不支持。你仍可使用本地规则工具：创建路基模板。",
        null,
        false);
}

var modelProfile = new ModelProfileOptions
{
    Name = profile.Name,
    Provider = profile.Provider,
    BaseUrl = profile.BaseUrl,
    Model = profile.Model,
    Temperature = profile.Temperature,
    TimeoutSeconds = profile.TimeoutSeconds
};
var apiKey = string.IsNullOrWhiteSpace(profile.SecretId)
    ? null
    : await secretStore.ReadAsync(profile.SecretId, cancellationToken).ConfigureAwait(false);
var reply = await modelClient.CompleteAsync(
    modelProfile,
    await promptContext.CreateSystemPromptAsync(cancellationToken).ConfigureAwait(false),
    message,
    apiKey,
    cancellationToken).ConfigureAwait(false);
```

Replace `FindProfile` with:

```csharp
private static StoredModelProfile? FindProfile(AgentRuntimeConfiguration config, string? requestedName)
{
    if (!string.IsNullOrWhiteSpace(requestedName))
    {
        var requestedProfile = config.ModelProfiles.FirstOrDefault(item =>
            string.Equals(item.Name, requestedName, StringComparison.OrdinalIgnoreCase));
        if (requestedProfile != null)
        {
            return requestedProfile;
        }
    }

    if (!string.IsNullOrWhiteSpace(config.DefaultModelProfile))
    {
        var defaultProfile = config.ModelProfiles.FirstOrDefault(item =>
            string.Equals(item.Name, config.DefaultModelProfile, StringComparison.OrdinalIgnoreCase));
        if (defaultProfile != null)
        {
            return defaultProfile;
        }
    }

    return config.ModelProfiles.FirstOrDefault();
}
```

Add `using RoadProto.Agent.Host.Admin;`.

- [ ] **Step 8: Register document and prompt services**

Modify `src/agent/RoadProto.Agent.Host/Program.cs`:

```csharp
builder.Services.AddSingleton<AgentDocumentStore>();
builder.Services.AddSingleton<AgentPromptContextService>();
```

Remove `MarkdownSkillRepository` from `AgentChatService` constructor registration assumptions; keep `MarkdownSkillRepository` registered because `AgentPromptContextService` uses it.

- [ ] **Step 9: Run tests**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
```

Expected: all Agent tests PASS.

- [ ] **Step 10: Commit Task 3**

Run:

```powershell
git add src\agent\RoadProto.Agent.Host\Admin src\agent\RoadProto.Agent.Host\Services src\agent\RoadProto.Agent.Host\Program.cs src\agent\RoadProto.Agent.Tests
git commit -m "feat: add agent admin document context"
```

---

### Task 4: `/admin` 静态页面入口

**Files:**
- Create: `src/agent/RoadProto.Agent.Host/wwwroot/admin/index.html`
- Create: `src/agent/RoadProto.Agent.Host/wwwroot/admin/admin.css`
- Create: `src/agent/RoadProto.Agent.Host/wwwroot/admin/admin.js`
- Modify: `src/agent/RoadProto.Agent.Host/Admin/AgentAdminEndpoints.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Program.cs`
- Modify: `src/agent/RoadProto.Agent.Tests/AgentAdminApiTests.cs`

- [ ] **Step 1: Add failing `/admin` static page test**

Append to `AgentAdminApiTests`:

```csharp
[Fact]
public async Task AdminPageReturnsHtmlShell()
{
    using var factory = CreateFactory();
    using var client = factory.CreateClient();

    var html = await client.GetStringAsync("/admin");

    Assert.Contains("RoadProto Agent 管理控制台", html);
    Assert.Contains("/admin/admin.js", html);
    Assert.Contains("/admin/admin.css", html);
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AdminPageReturnsHtmlShell
```

Expected: FAIL with 404.

- [ ] **Step 3: Create HTML shell**

Create `src/agent/RoadProto.Agent.Host/wwwroot/admin/index.html`:

```html
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>RoadProto Agent 管理控制台</title>
  <link rel="stylesheet" href="/admin/admin.css">
</head>
<body>
  <div class="app-shell">
    <aside class="sidebar">
      <div class="brand">RoadProto Agent</div>
      <button class="nav-item active" data-view="models">模型配置</button>
      <button class="nav-item" data-view="skills">Skill 文档</button>
      <button class="nav-item" data-view="knowledge">知识库</button>
      <button class="nav-item" data-view="status">连接测试</button>
    </aside>
    <main class="workspace">
      <header class="topbar">
        <div>
          <h1>RoadProto Agent 管理控制台</h1>
          <p id="statusText">正在读取后端状态...</p>
        </div>
        <button id="refreshButton" class="secondary-button">刷新</button>
      </header>

      <section id="modelsView" class="view active">
        <div class="section-header">
          <h2>模型配置</h2>
          <button id="newProfileButton" class="primary-button">新增 Profile</button>
        </div>
        <div class="two-column">
          <div id="profileList" class="list-panel"></div>
          <form id="profileForm" class="form-panel">
            <label>Profile 名称<input id="profileName" required pattern="[A-Za-z0-9_-]+"></label>
            <label>供应商
              <select id="profilePreset">
                <option value="dashscope">DashScope / 阿里百炼 / 千问</option>
                <option value="deepseek">DeepSeek</option>
                <option value="openai">OpenAI</option>
                <option value="custom">自定义 OpenAI-compatible</option>
              </select>
            </label>
            <label>Base URL<input id="profileBaseUrl" required></label>
            <label>模型名<input id="profileModel" required></label>
            <label>Temperature<input id="profileTemperature" type="number" step="0.1" min="0" max="2" value="0.2"></label>
            <label>超时秒数<input id="profileTimeout" type="number" min="1" max="300" value="60"></label>
            <label>API Key<input id="profileApiKey" type="password" autocomplete="off" placeholder="保存后不会回显完整 Key"></label>
            <label class="checkbox-row"><input id="profileDefault" type="checkbox">设为默认模型</label>
            <div class="button-row">
              <button class="primary-button" type="submit">保存</button>
              <button id="testProfileButton" class="secondary-button" type="button">测试连接</button>
              <button id="deleteProfileButton" class="danger-button" type="button">删除</button>
            </div>
          </form>
        </div>
      </section>

      <section id="skillsView" class="view">
        <div class="section-header"><h2>Skill 文档</h2><input id="skillUpload" type="file" accept=".md"></div>
        <div id="skillList" class="list-panel wide"></div>
      </section>

      <section id="knowledgeView" class="view">
        <div class="section-header"><h2>知识库</h2><input id="knowledgeUpload" type="file" accept=".md"></div>
        <div id="knowledgeList" class="list-panel wide"></div>
      </section>

      <section id="statusView" class="view">
        <h2>连接测试</h2>
        <div id="statusPanel" class="status-grid"></div>
      </section>
    </main>
  </div>
  <div id="toast" class="toast" hidden></div>
  <script src="/admin/admin.js"></script>
</body>
</html>
```

- [ ] **Step 4: Create CSS shell**

Create `src/agent/RoadProto.Agent.Host/wwwroot/admin/admin.css`:

```css
:root {
  color-scheme: light;
  --bg: #f4f6f8;
  --panel: #ffffff;
  --line: #d8dee6;
  --text: #1c2530;
  --muted: #687587;
  --accent: #1b6fd8;
  --danger: #b3261e;
}

* { box-sizing: border-box; }
body {
  margin: 0;
  font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
  background: var(--bg);
  color: var(--text);
}
.app-shell { display: flex; min-height: 100vh; }
.sidebar {
  width: 220px;
  background: #111820;
  color: #fff;
  padding: 20px 14px;
}
.brand { font-size: 18px; font-weight: 700; margin-bottom: 24px; }
.nav-item {
  display: block;
  width: 100%;
  border: 0;
  background: transparent;
  color: #c9d3df;
  text-align: left;
  padding: 10px 12px;
  border-radius: 6px;
  margin-bottom: 6px;
  cursor: pointer;
}
.nav-item.active, .nav-item:hover { background: #263241; color: #fff; }
.workspace { flex: 1; padding: 22px; min-width: 0; }
.topbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 18px;
}
h1 { margin: 0; font-size: 24px; }
h2 { margin: 0 0 14px; font-size: 18px; }
p { margin: 6px 0 0; color: var(--muted); }
.view { display: none; }
.view.active { display: block; }
.section-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
}
.two-column {
  display: grid;
  grid-template-columns: minmax(260px, 360px) minmax(420px, 1fr);
  gap: 16px;
}
.list-panel, .form-panel, .status-grid {
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 16px;
}
.wide { min-height: 360px; }
label { display: block; font-weight: 600; margin-bottom: 12px; }
input, select {
  display: block;
  width: 100%;
  height: 36px;
  margin-top: 6px;
  border: 1px solid var(--line);
  border-radius: 6px;
  padding: 0 10px;
  font: inherit;
}
.checkbox-row { display: flex; gap: 8px; align-items: center; }
.checkbox-row input { width: auto; height: auto; margin: 0; }
.button-row { display: flex; gap: 8px; flex-wrap: wrap; }
button {
  height: 36px;
  border-radius: 6px;
  border: 1px solid transparent;
  padding: 0 14px;
  cursor: pointer;
  font: inherit;
}
.primary-button { background: var(--accent); color: #fff; }
.secondary-button { background: #fff; color: var(--text); border-color: var(--line); }
.danger-button { background: #fff; color: var(--danger); border-color: #efb4af; }
.item {
  border: 1px solid var(--line);
  border-radius: 6px;
  padding: 10px;
  margin-bottom: 8px;
  cursor: pointer;
}
.item.active { border-color: var(--accent); background: #eef5ff; }
.item-title { font-weight: 700; }
.item-meta { color: var(--muted); font-size: 13px; margin-top: 4px; }
.toast {
  position: fixed;
  right: 20px;
  bottom: 20px;
  background: #111820;
  color: #fff;
  padding: 10px 14px;
  border-radius: 6px;
}
@media (max-width: 900px) {
  .app-shell { flex-direction: column; }
  .sidebar { width: 100%; display: flex; gap: 8px; overflow-x: auto; }
  .brand { margin: 0 12px 0 0; white-space: nowrap; }
  .nav-item { width: auto; white-space: nowrap; }
  .two-column { grid-template-columns: 1fr; }
}
```

- [ ] **Step 5: Create JavaScript placeholder**

Create `src/agent/RoadProto.Agent.Host/wwwroot/admin/admin.js`:

```javascript
const state = {
  profiles: [],
  selectedProfile: null
};

document.addEventListener("DOMContentLoaded", () => {
  document.querySelectorAll(".nav-item").forEach(button => {
    button.addEventListener("click", () => switchView(button.dataset.view));
  });
  document.getElementById("refreshButton").addEventListener("click", refreshAll);
  refreshAll();
});

function switchView(view) {
  document.querySelectorAll(".nav-item").forEach(item => {
    item.classList.toggle("active", item.dataset.view === view);
  });
  document.querySelectorAll(".view").forEach(panel => {
    panel.classList.toggle("active", panel.id === `${view}View`);
  });
}

async function refreshAll() {
  const status = await getJson("/api/admin/status");
  document.getElementById("statusText").textContent =
    `默认模型：${status.defaultModelProfile || "未配置"}，存储目录：${status.storageRoot}`;
  document.getElementById("statusPanel").innerHTML = `
    <div class="item"><div class="item-title">模型 Profile</div><div class="item-meta">${status.modelProfileCount}</div></div>
    <div class="item"><div class="item-title">启用 Skill</div><div class="item-meta">${status.enabledSkillCount}</div></div>
    <div class="item"><div class="item-title">启用知识库</div><div class="item-meta">${status.enabledKnowledgeCount}</div></div>`;
}

async function getJson(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }
  return await response.json();
}

function showToast(message) {
  const toast = document.getElementById("toast");
  toast.textContent = message;
  toast.hidden = false;
  setTimeout(() => toast.hidden = true, 2600);
}
```

- [ ] **Step 6: Map static page and files**

Modify `src/agent/RoadProto.Agent.Host/Admin/AgentAdminEndpoints.cs`:

```csharp
app.MapGet("/admin", async (HttpContext context, IWebHostEnvironment environment) =>
{
    var path = Path.Combine(environment.WebRootPath ?? string.Empty, "admin", "index.html");
    if (!File.Exists(path))
    {
        return Results.NotFound("RoadProto Agent 管理控制台文件不存在。");
    }

    context.Response.ContentType = "text/html; charset=utf-8";
    await context.Response.SendFileAsync(path).ConfigureAwait(false);
    return Results.Empty;
});
```

Modify `src/agent/RoadProto.Agent.Host/Program.cs` before mapping endpoints:

```csharp
app.UseStaticFiles();
```

- [ ] **Step 7: Run static page test**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AdminPageReturnsHtmlShell
```

Expected: PASS.

- [ ] **Step 8: Commit Task 4**

Run:

```powershell
git add src\agent\RoadProto.Agent.Host\wwwroot src\agent\RoadProto.Agent.Host\Admin\AgentAdminEndpoints.cs src\agent\RoadProto.Agent.Host\Program.cs src\agent\RoadProto.Agent.Tests\AgentAdminApiTests.cs
git commit -m "feat: add agent admin web shell"
```

---

### Task 5: 管理页面模型配置交互

**Files:**
- Modify: `src/agent/RoadProto.Agent.Host/wwwroot/admin/admin.js`
- Modify: `src/agent/RoadProto.Agent.Host/Admin/AgentAdminEndpoints.cs`
- Modify: `src/agent/RoadProto.Agent.Tests/AgentAdminApiTests.cs`

- [ ] **Step 1: Add API test for setting default profile**

Append to `AgentAdminApiTests`:

```csharp
[Fact]
public async Task CanChangeDefaultModelProfile()
{
    using var factory = CreateFactory();
    using var client = factory.CreateClient();
    await client.PostAsJsonAsync("/api/admin/model-profiles", new UpsertModelProfileRequest(
        "openai", "OpenAICompatible", "https://api.openai.com/v1", "gpt-4.1", 0.2, 60, "sk-openai", true));
    await client.PostAsJsonAsync("/api/admin/model-profiles", new UpsertModelProfileRequest(
        "deepseek", "OpenAICompatible", "https://api.deepseek.com", "deepseek-chat", 0.2, 60, "sk-deepseek", false));

    using var response = await client.PostAsync("/api/admin/model-profiles/deepseek/default", null);
    var status = await client.GetFromJsonAsync<AdminStatusResponse>("/api/admin/status");

    Assert.Equal(HttpStatusCode.OK, response.StatusCode);
    Assert.Equal("deepseek", status!.DefaultModelProfile);
}
```

- [ ] **Step 2: Ensure profile default test passes**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter CanChangeDefaultModelProfile
```

Expected: PASS. If it fails, fix `POST /api/admin/model-profiles/{name}/default` from Task 2 before moving on.

- [ ] **Step 3: Implement profile UI JavaScript**

Replace `admin.js` with this complete script:

```javascript
const state = {
  profiles: [],
  selectedProfile: null
};

const presets = {
  dashscope: {
    name: "dashscope-qwen",
    baseUrl: "https://dashscope.aliyuncs.com/compatible-mode/v1",
    model: "qwen-plus"
  },
  deepseek: {
    name: "deepseek",
    baseUrl: "https://api.deepseek.com",
    model: "deepseek-chat"
  },
  openai: {
    name: "openai",
    baseUrl: "https://api.openai.com/v1",
    model: "gpt-4.1"
  },
  custom: {
    name: "",
    baseUrl: "",
    model: ""
  }
};

document.addEventListener("DOMContentLoaded", () => {
  document.querySelectorAll(".nav-item").forEach(button => {
    button.addEventListener("click", () => switchView(button.dataset.view));
  });
  document.getElementById("refreshButton").addEventListener("click", refreshAll);
  document.getElementById("newProfileButton").addEventListener("click", () => selectProfile(null));
  document.getElementById("profilePreset").addEventListener("change", applyPreset);
  document.getElementById("profileForm").addEventListener("submit", saveProfile);
  document.getElementById("deleteProfileButton").addEventListener("click", deleteProfile);
  document.getElementById("testProfileButton").addEventListener("click", testProfile);
  refreshAll();
});

function switchView(view) {
  document.querySelectorAll(".nav-item").forEach(item => {
    item.classList.toggle("active", item.dataset.view === view);
  });
  document.querySelectorAll(".view").forEach(panel => {
    panel.classList.toggle("active", panel.id === `${view}View`);
  });
}

async function refreshAll() {
  await Promise.all([loadStatus(), loadProfiles()]);
}

async function loadStatus() {
  const status = await getJson("/api/admin/status");
  document.getElementById("statusText").textContent =
    `默认模型：${status.defaultModelProfile || "未配置"}，存储目录：${status.storageRoot}`;
  document.getElementById("statusPanel").innerHTML = `
    <div class="item"><div class="item-title">模型 Profile</div><div class="item-meta">${status.modelProfileCount}</div></div>
    <div class="item"><div class="item-title">启用 Skill</div><div class="item-meta">${status.enabledSkillCount}</div></div>
    <div class="item"><div class="item-title">启用知识库</div><div class="item-meta">${status.enabledKnowledgeCount}</div></div>`;
}

async function loadProfiles() {
  state.profiles = await getJson("/api/admin/model-profiles");
  renderProfiles();
  if (state.selectedProfile) {
    const stillExists = state.profiles.find(item => item.name === state.selectedProfile.name);
    selectProfile(stillExists || null);
  } else if (state.profiles.length > 0) {
    selectProfile(state.profiles[0]);
  } else {
    selectProfile(null);
  }
}

function renderProfiles() {
  const list = document.getElementById("profileList");
  if (state.profiles.length === 0) {
    list.innerHTML = `<div class="item"><div class="item-title">暂无 Profile</div><div class="item-meta">请新增一个模型配置。</div></div>`;
    return;
  }
  list.innerHTML = state.profiles.map(profile => `
    <div class="item ${state.selectedProfile?.name === profile.name ? "active" : ""}" data-profile="${escapeHtml(profile.name)}">
      <div class="item-title">${escapeHtml(profile.name)}${profile.isDefault ? " · 默认" : ""}</div>
      <div class="item-meta">${escapeHtml(profile.model)} · ${profile.hasApiKey ? profile.apiKeyMask : "未配置 Key"}</div>
    </div>`).join("");
  list.querySelectorAll("[data-profile]").forEach(item => {
    item.addEventListener("click", () => {
      const profile = state.profiles.find(candidate => candidate.name === item.dataset.profile);
      selectProfile(profile || null);
    });
  });
}

function selectProfile(profile) {
  state.selectedProfile = profile;
  document.getElementById("profileName").value = profile?.name || "";
  document.getElementById("profileBaseUrl").value = profile?.baseUrl || "";
  document.getElementById("profileModel").value = profile?.model || "";
  document.getElementById("profileTemperature").value = profile?.temperature ?? 0.2;
  document.getElementById("profileTimeout").value = profile?.timeoutSeconds ?? 60;
  document.getElementById("profileApiKey").value = "";
  document.getElementById("profileDefault").checked = profile?.isDefault || state.profiles.length === 0;
  renderProfiles();
}

function applyPreset() {
  const preset = presets[document.getElementById("profilePreset").value];
  if (!preset) return;
  if (preset.name) document.getElementById("profileName").value = preset.name;
  document.getElementById("profileBaseUrl").value = preset.baseUrl;
  document.getElementById("profileModel").value = preset.model;
}

async function saveProfile(event) {
  event.preventDefault();
  const payload = readProfileForm();
  const response = await fetch("/api/admin/model-profiles", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  });
  await ensureOk(response);
  showToast("模型配置已保存");
  await refreshAll();
}

async function deleteProfile() {
  const name = document.getElementById("profileName").value.trim();
  if (!name || !confirm(`确认删除模型 Profile：${name}？`)) return;
  const response = await fetch(`/api/admin/model-profiles/${encodeURIComponent(name)}`, { method: "DELETE" });
  await ensureOk(response);
  showToast("模型配置已删除");
  state.selectedProfile = null;
  await refreshAll();
}

async function testProfile() {
  const name = document.getElementById("profileName").value.trim();
  if (!name) {
    showToast("请先保存 Profile");
    return;
  }
  const response = await fetch(`/api/admin/model-profiles/${encodeURIComponent(name)}/test`, { method: "POST" });
  const text = await response.text();
  showToast(response.ok ? `连接测试完成：${text.slice(0, 80)}` : `连接测试失败：${text.slice(0, 80)}`);
}

function readProfileForm() {
  return {
    name: document.getElementById("profileName").value.trim(),
    provider: "OpenAICompatible",
    baseUrl: document.getElementById("profileBaseUrl").value.trim(),
    model: document.getElementById("profileModel").value.trim(),
    temperature: Number(document.getElementById("profileTemperature").value || "0.2"),
    timeoutSeconds: Number(document.getElementById("profileTimeout").value || "60"),
    apiKey: document.getElementById("profileApiKey").value.trim() || null,
    isDefault: document.getElementById("profileDefault").checked
  };
}

async function getJson(url) {
  const response = await fetch(url);
  await ensureOk(response);
  return await response.json();
}

async function ensureOk(response) {
  if (!response.ok) {
    let message = `${response.status} ${response.statusText}`;
    try {
      const body = await response.json();
      message = body.message || message;
    } catch {
      message = await response.text() || message;
    }
    throw new Error(message);
  }
}

function showToast(message) {
  const toast = document.getElementById("toast");
  toast.textContent = message;
  toast.hidden = false;
  setTimeout(() => toast.hidden = true, 2600);
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}
```

- [ ] **Step 4: Add test endpoint implementation**

Append to `MapAgentAdminEndpoints` in `AgentAdminEndpoints.cs`:

```csharp
app.MapPost("/api/admin/model-profiles/{name}/test", async (
    string name,
    AgentConfigurationStore store,
    AgentSecretStore secrets,
    OpenAiCompatibleChatClient client,
    HttpContext httpContext) =>
{
    var config = await store.LoadAsync(httpContext.RequestAborted).ConfigureAwait(false);
    var profile = config.ModelProfiles.FirstOrDefault(item =>
        string.Equals(item.Name, name, StringComparison.OrdinalIgnoreCase));
    if (profile == null)
    {
        return Results.NotFound("Profile 不存在。");
    }

    var apiKey = string.IsNullOrWhiteSpace(profile.SecretId)
        ? null
        : await secrets.ReadAsync(profile.SecretId, httpContext.RequestAborted).ConfigureAwait(false);
    var modelProfile = new ModelProfileOptions
    {
        Name = profile.Name,
        Provider = profile.Provider,
        BaseUrl = profile.BaseUrl,
        Model = profile.Model,
        Temperature = profile.Temperature,
        TimeoutSeconds = profile.TimeoutSeconds
    };
    var result = await client.TestConnectionAsync(modelProfile, apiKey, httpContext.RequestAborted)
        .ConfigureAwait(false);
    return Results.Ok(result);
});
```

- [ ] **Step 5: Build host**

Run:

```powershell
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
```

Expected: build succeeds with 0 errors.

- [ ] **Step 6: Commit Task 5**

Run:

```powershell
git add src\agent\RoadProto.Agent.Host\wwwroot\admin\admin.js src\agent\RoadProto.Agent.Host\Admin\AgentAdminEndpoints.cs src\agent\RoadProto.Agent.Tests\AgentAdminApiTests.cs
git commit -m "feat: wire agent admin model ui"
```

---

### Task 6: Skill 和知识库前端交互

**Files:**
- Modify: `src/agent/RoadProto.Agent.Host/wwwroot/admin/admin.js`
- Modify: `src/agent/RoadProto.Agent.Tests/AgentAdminApiTests.cs`

- [ ] **Step 1: Add API test for uploading skill through endpoint**

Append to `AgentAdminApiTests`:

```csharp
[Fact]
public async Task CanUploadSkillThroughAdminApi()
{
    using var factory = CreateFactory();
    using var client = factory.CreateClient();
    using var form = new MultipartFormDataContent();
    form.Add(new StringContent("# Skill\n\n创建规则"), "file", "road-skill.md");

    using var response = await client.PostAsync("/api/admin/skills/upload", form);
    var skills = await client.GetFromJsonAsync<List<DocumentResponse>>("/api/admin/skills");

    Assert.Equal(HttpStatusCode.OK, response.StatusCode);
    var skill = Assert.Single(skills!);
    Assert.Equal("road-skill.md", skill.DisplayName);
    Assert.True(skill.Enabled);
}
```

- [ ] **Step 2: Run upload endpoint test**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter CanUploadSkillThroughAdminApi
```

Expected: PASS. If it fails, fix the multipart endpoint before frontend work.

- [ ] **Step 3: Add document UI functions to JavaScript**

Append to `admin.js` and call from `refreshAll()`:

```javascript
async function refreshAll() {
  await Promise.all([loadStatus(), loadProfiles(), loadDocuments("skills"), loadDocuments("knowledge")]);
}
```

Register upload listeners in `DOMContentLoaded`:

```javascript
document.getElementById("skillUpload").addEventListener("change", event => uploadDocument("skills", event.target.files[0]));
document.getElementById("knowledgeUpload").addEventListener("change", event => uploadDocument("knowledge", event.target.files[0]));
```

Add document helpers:

```javascript
async function loadDocuments(kind) {
  const docs = await getJson(`/api/admin/${kind}`);
  const target = document.getElementById(kind === "skills" ? "skillList" : "knowledgeList");
  if (docs.length === 0) {
    target.innerHTML = `<div class="item"><div class="item-title">暂无 Markdown</div><div class="item-meta">上传 .md 文件后会显示在这里。</div></div>`;
    return;
  }

  target.innerHTML = docs.map(doc => `
    <div class="item">
      <div class="item-title">${escapeHtml(doc.displayName)}</div>
      <div class="item-meta">${doc.enabled ? "已启用" : "已禁用"} · ${formatBytes(doc.sizeBytes)} · ${doc.builtIn ? "内置" : "用户上传"}</div>
      <div class="button-row" style="margin-top:8px">
        <button class="secondary-button" data-action="toggle" data-kind="${kind}" data-id="${doc.id}" data-enabled="${!doc.enabled}">${doc.enabled ? "禁用" : "启用"}</button>
        ${doc.builtIn ? "" : `<button class="danger-button" data-action="delete" data-kind="${kind}" data-id="${doc.id}">删除</button>`}
      </div>
    </div>`).join("");

  target.querySelectorAll("[data-action='toggle']").forEach(button => {
    button.addEventListener("click", () => updateDocument(
      button.dataset.kind,
      button.dataset.id,
      button.dataset.enabled === "true"));
  });
  target.querySelectorAll("[data-action='delete']").forEach(button => {
    button.addEventListener("click", () => deleteDocument(button.dataset.kind, button.dataset.id));
  });
}

async function uploadDocument(kind, file) {
  if (!file) return;
  const form = new FormData();
  form.append("file", file);
  const response = await fetch(`/api/admin/${kind}/upload`, { method: "POST", body: form });
  await ensureOk(response);
  showToast("Markdown 已上传");
  await refreshAll();
}

async function updateDocument(kind, id, enabled) {
  const response = await fetch(`/api/admin/${kind}/${encodeURIComponent(id)}`, {
    method: "PATCH",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ enabled, displayName: null })
  });
  await ensureOk(response);
  showToast(enabled ? "文档已启用" : "文档已禁用");
  await refreshAll();
}

async function deleteDocument(kind, id) {
  if (!confirm("确认删除这个 Markdown 文档？")) return;
  const response = await fetch(`/api/admin/${kind}/${encodeURIComponent(id)}`, { method: "DELETE" });
  await ensureOk(response);
  showToast("文档已删除");
  await refreshAll();
}

function formatBytes(value) {
  if (value < 1024) return `${value} B`;
  if (value < 1024 * 1024) return `${Math.round(value / 1024)} KB`;
  return `${(value / 1024 / 1024).toFixed(1)} MB`;
}
```

- [ ] **Step 4: Run host build and tests**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
```

Expected: all tests pass and build succeeds.

- [ ] **Step 5: Commit Task 6**

Run:

```powershell
git add src\agent\RoadProto.Agent.Host\wwwroot\admin\admin.js src\agent\RoadProto.Agent.Tests\AgentAdminApiTests.cs
git commit -m "feat: manage agent markdown documents"
```

---

### Task 7: 文档、验证和浏览器手工点验

**Files:**
- Modify: `README.md`
- Modify: `docs/business/agent/设计软件原型Agent.md`
- Modify: `docs/modules/agent.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`

- [ ] **Step 1: Update README admin instructions**

In `README.md`, update the Agent sidecar section to include:

```markdown
启动后端后可打开本地管理控制台：

```text
http://127.0.0.1:17831/admin
```

管理控制台支持模型 Profile 配置、Windows 当前用户加密保存 API Key、Markdown skill 上传和 Markdown 知识库上传。API Key 保存在 `%LOCALAPPDATA%\RoadProto\Agent\secrets\`，不会写入仓库。
```
```

- [ ] **Step 2: Update Agent business and module docs**

In `docs/business/agent/设计软件原型Agent.md`, add:

```markdown
## 管理控制台

`RoadProto.Agent.Host` 提供独立浏览器管理控制台 `/admin`。该页面用于配置 OpenAI-compatible 模型 Profile、输入并加密保存 API Key、导入 Markdown skill 和 Markdown 知识库。管理控制台不直接执行 CAD 工具；所有 CAD 写入仍必须通过 WPF 确认卡片和 C++ 白名单工具网关。
```

In `docs/modules/agent.md`, add code landing point:

```markdown
- `src/agent/RoadProto.Agent.Host/Admin`：管理控制台配置、密钥、文档和 API。
- `src/agent/RoadProto.Agent.Host/wwwroot/admin`：独立浏览器管理页面。
```

- [ ] **Step 3: Update version log and tests README**

In `docs/dev/version_log.md`, under `v0.1.32_20260604_AgentPrototype`, add:

```markdown
- 新增 `/admin` 本地管理控制台，支持模型 Profile 配置、Windows 当前用户加密保存 API Key、Markdown skill 上传和 Markdown 知识库上传。
```

In `tests/README.md`, add:

```markdown
管理控制台测试覆盖配置读写、DPAPI 密钥保存、模型 Profile 管理 API、Markdown 上传与启用禁用、prompt 上下文组装、`/admin` 静态页面和 `/api/chat` 使用运行期默认模型配置。
```

- [ ] **Step 4: Run full automated verification**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
git diff --check
```

Expected:

- Agent tests pass.
- Agent Host Release build succeeds.
- WPF Release build succeeds.
- `git diff --check` reports no whitespace errors.

- [ ] **Step 5: Start backend for browser check**

Run:

```powershell
dotnet run --project src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj
```

Open:

```text
http://127.0.0.1:17831/admin
```

Expected:

- Page loads without 404 or connection refused.
- Left navigation shows `模型配置`、`Skill 文档`、`知识库`、`连接测试`.
- Adding a fake Profile with `https://api.openai.com/v1` and `gpt-4.1` saves successfully.
- Profile list shows `已配置 Key` with masked API Key.
- Uploading a `.md` file to Skill list succeeds.
- Uploading a `.txt` file fails with a user-visible message.

- [ ] **Step 6: Commit Task 7**

Run:

```powershell
git add README.md docs/business/agent/设计软件原型Agent.md docs/modules/agent.md docs/dev/version_log.md tests/README.md
git commit -m "docs: document agent admin console"
```

---

## Final Verification

Run these commands before claiming completion:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
git status --short
```

Expected:

- `RoadProto.Agent.Tests` passes.
- Agent Host Release build succeeds.
- WPF Release build succeeds.
- `git status --short` is clean after the final commit.

Manual browser check:

```powershell
dotnet run --project src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj
```

Open:

```text
http://127.0.0.1:17831/admin
```

Expected:

- Admin page loads.
- Model Profile can be saved.
- Skill Markdown can be uploaded.
- Knowledge Markdown can be uploaded.
- `/api/chat` still returns a normal response or a local tool confirmation.
