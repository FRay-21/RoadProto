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
