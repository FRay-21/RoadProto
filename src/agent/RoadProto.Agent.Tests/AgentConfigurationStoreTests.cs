using System.Text.Json;
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
    public async Task StoreReplacesExistingConfigWithReadableJson()
    {
        var store = new AgentConfigurationStore(new AgentLocalPaths(root), new RoadProtoAgentOptions());
        await store.SaveAsync(new AgentRuntimeConfiguration
        {
            DefaultModelProfile = "old",
            ModelProfiles =
            [
                new StoredModelProfile
                {
                    Name = "old",
                    BaseUrl = "https://old.example/v1",
                    Model = "old-model"
                }
            ]
        });

        await store.SaveAsync(new AgentRuntimeConfiguration
        {
            DefaultModelProfile = "new",
            ModelProfiles =
            [
                new StoredModelProfile
                {
                    Name = "new",
                    BaseUrl = "https://new.example/v1",
                    Model = "new-model"
                }
            ]
        });

        var json = await File.ReadAllTextAsync(Path.Combine(root, "config.json"));
        var parsed = JsonSerializer.Deserialize<AgentRuntimeConfiguration>(
            json,
            new JsonSerializerOptions(JsonSerializerDefaults.Web));

        Assert.NotNull(parsed);
        Assert.Equal("new", parsed.DefaultModelProfile);
        Assert.Equal("new-model", parsed.ModelProfiles.Single().Model);
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
    public async Task StoreCreatesDistinctBackupsForRepeatedCorruptedConfigs()
    {
        Directory.CreateDirectory(root);
        var configPath = Path.Combine(root, "config.json");
        var store = new AgentConfigurationStore(new AgentLocalPaths(root), new RoadProtoAgentOptions());

        await File.WriteAllTextAsync(configPath, "{ invalid json 1");
        await store.LoadAsync();
        await File.WriteAllTextAsync(configPath, "{ invalid json 2");
        await store.LoadAsync();

        Assert.Equal(2, Directory.GetFiles(root, "config.corrupt.*.json").Length);
    }

    [Fact]
    public void StoredAgentDocumentUpdatedAtDefaultsToUnset()
    {
        var document = new StoredAgentDocument();

        Assert.Equal(default, document.UpdatedAt);
    }

    [Theory]
    [InlineData("../bad")]
    [InlineData("bad:name")]
    [InlineData("")]
    public async Task SecretStoreRejectsInvalidSecretIds(string secretId)
    {
        var secrets = new AgentSecretStore(new AgentLocalPaths(root));

        await Assert.ThrowsAsync<InvalidOperationException>(
            () => secrets.DeleteAsync(secretId));
    }

    [Fact]
    public async Task SecretStoreEncryptsAndMasksApiKey()
    {
        if (!OperatingSystem.IsWindows())
        {
            return;
        }

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
