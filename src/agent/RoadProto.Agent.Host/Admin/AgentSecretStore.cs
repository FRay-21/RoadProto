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
        if (!OperatingSystem.IsWindows())
        {
            throw new PlatformNotSupportedException("AgentSecretStore requires Windows CurrentUser DPAPI.");
        }

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
        if (!OperatingSystem.IsWindows())
        {
            throw new PlatformNotSupportedException("AgentSecretStore requires Windows CurrentUser DPAPI.");
        }

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
