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
                    $"config.corrupt.{DateTimeOffset.UtcNow:yyyyMMddHHmmssfffffff}.{Guid.NewGuid():N}.json");
                File.Move(paths.ConfigPath, backup, overwrite: false);
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
        string? tempPath = null;
        try
        {
            paths.Ensure();
            tempPath = Path.Combine(paths.Root, $"config.{Guid.NewGuid():N}.tmp");
            await using (var stream = new FileStream(
                tempPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None,
                bufferSize: 8192,
                FileOptions.WriteThrough))
            {
                await JsonSerializer.SerializeAsync(stream, config, JsonOptions, cancellationToken).ConfigureAwait(false);
                await stream.FlushAsync(cancellationToken).ConfigureAwait(false);
                stream.Flush(flushToDisk: true);
            }

            CommitTempConfig(tempPath);
            tempPath = null;
        }
        finally
        {
            if (tempPath != null && File.Exists(tempPath))
            {
                File.Delete(tempPath);
            }

            gate.Release();
        }
    }

    private void CommitTempConfig(string tempPath)
    {
        for (var attempt = 0; attempt < 2; attempt++)
        {
            if (File.Exists(paths.ConfigPath))
            {
                try
                {
                    File.Replace(tempPath, paths.ConfigPath, destinationBackupFileName: null, ignoreMetadataErrors: true);
                    return;
                }
                catch (FileNotFoundException)
                {
                    continue;
                }
            }

            try
            {
                File.Move(tempPath, paths.ConfigPath);
                return;
            }
            catch (IOException) when (File.Exists(paths.ConfigPath))
            {
            }
        }

        File.Replace(tempPath, paths.ConfigPath, destinationBackupFileName: null, ignoreMetadataErrors: true);
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
