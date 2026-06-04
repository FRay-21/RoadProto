using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace RoadProto.Agent.Host.Admin;

public sealed class AgentSecretStore
{
    private static readonly Regex SecretIdPattern = new(
        "^[a-z0-9][a-z0-9_-]{0,63}$",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private readonly AgentLocalPaths paths;

    public AgentSecretStore(AgentLocalPaths paths)
    {
        this.paths = paths;
    }

    public async Task SaveAsync(string secretId, string apiKey, CancellationToken cancellationToken = default)
    {
        var path = GetSecretPath(secretId);
        if (!OperatingSystem.IsWindows())
        {
            throw new PlatformNotSupportedException("AgentSecretStore requires Windows CurrentUser DPAPI.");
        }

        paths.Ensure();
        var protectedBytes = ProtectedData.Protect(
            Encoding.UTF8.GetBytes(apiKey),
            optionalEntropy: null,
            DataProtectionScope.CurrentUser);
        await File.WriteAllBytesAsync(path, protectedBytes, cancellationToken).ConfigureAwait(false);
    }

    public async Task<string?> ReadAsync(string secretId, CancellationToken cancellationToken = default)
    {
        var path = GetSecretPath(secretId);
        if (!OperatingSystem.IsWindows())
        {
            throw new PlatformNotSupportedException("AgentSecretStore requires Windows CurrentUser DPAPI.");
        }

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
            .Select(ch => char.IsLetterOrDigit(ch) || ch == '_' || ch == '-' ? ch : '-')
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
        if (string.IsNullOrWhiteSpace(secretId) || !SecretIdPattern.IsMatch(secretId))
        {
            throw new InvalidOperationException("SecretId 必须是 1 到 64 位的小写字母、数字、下划线或连字符 slug。");
        }

        var secretsRoot = Path.GetFullPath(paths.SecretsDirectory);
        var comparison = OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        var rootPrefix = secretsRoot.EndsWith(Path.DirectorySeparatorChar)
            ? secretsRoot
            : secretsRoot + Path.DirectorySeparatorChar;
        var fullPath = Path.GetFullPath(Path.Combine(secretsRoot, secretId + ".bin"));
        if (!fullPath.StartsWith(rootPrefix, comparison))
        {
            throw new InvalidOperationException("SecretId 不能解析到密钥目录之外。");
        }

        return fullPath;
    }
}
