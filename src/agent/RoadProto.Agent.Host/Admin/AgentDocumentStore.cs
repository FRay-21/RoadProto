using System.Text;
using System.Text.RegularExpressions;
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

    private static readonly Regex SafeFileNamePattern = new(
        "^[a-z0-9][a-z0-9_-]{0,80}\\.md$",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private readonly AgentLocalPaths paths;
    private readonly AgentConfigurationStore configurations;

    public AgentDocumentStore(AgentLocalPaths paths, AgentConfigurationStore configurations)
    {
        this.paths = paths;
        this.configurations = configurations;
    }

    public async Task<StoredAgentDocument> UploadMarkdownAsync(
        AgentDocumentKind kind,
        IFormFile file,
        CancellationToken cancellationToken = default)
    {
        ValidateUpload(file);
        paths.Ensure();

        var displayName = file.FileName;
        var id = CreateDocumentId(file.FileName);
        var fileName = id + ".md";
        var directory = GetDirectory(kind);
        var path = GetDocumentPath(directory, fileName);

        await using (var input = file.OpenReadStream())
        using (var reader = new StreamReader(input, Encoding.UTF8, detectEncodingFromByteOrderMarks: true, leaveOpen: false))
        {
            var text = await reader.ReadToEndAsync(cancellationToken).ConfigureAwait(false);
            await File.WriteAllTextAsync(path, text, Encoding.UTF8, cancellationToken).ConfigureAwait(false);
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

        var config = await configurations.LoadAsync(cancellationToken).ConfigureAwait(false);
        GetDocuments(config, kind).Add(document);
        await configurations.SaveAsync(config, cancellationToken).ConfigureAwait(false);
        return document;
    }

    public async Task<List<StoredAgentDocument>> ListAsync(
        AgentDocumentKind kind,
        CancellationToken cancellationToken = default)
    {
        var config = await configurations.LoadAsync(cancellationToken).ConfigureAwait(false);
        return GetDocuments(config, kind)
            .Select(Clone)
            .ToList();
    }

    public async Task<StoredAgentDocument> UpdateAsync(
        AgentDocumentKind kind,
        string id,
        UpdateDocumentRequest request,
        CancellationToken cancellationToken = default)
    {
        var config = await configurations.LoadAsync(cancellationToken).ConfigureAwait(false);
        var document = FindDocument(config, kind, id);
        if (document == null)
        {
            throw new KeyNotFoundException("未找到指定文档。");
        }

        document.Enabled = request.Enabled;
        if (!string.IsNullOrWhiteSpace(request.DisplayName))
        {
            document.DisplayName = request.DisplayName.Trim();
        }

        document.UpdatedAt = DateTimeOffset.UtcNow;
        await configurations.SaveAsync(config, cancellationToken).ConfigureAwait(false);
        return Clone(document);
    }

    public async Task<bool> DeleteAsync(
        AgentDocumentKind kind,
        string id,
        CancellationToken cancellationToken = default)
    {
        var config = await configurations.LoadAsync(cancellationToken).ConfigureAwait(false);
        var documents = GetDocuments(config, kind);
        var document = documents.FirstOrDefault(item =>
            string.Equals(item.Id, id, StringComparison.OrdinalIgnoreCase));
        if (document == null)
        {
            return false;
        }

        if (document.BuiltIn)
        {
            throw new InvalidOperationException("内置文档不能删除。");
        }

        documents.Remove(document);
        await configurations.SaveAsync(config, cancellationToken).ConfigureAwait(false);

        var path = GetDocumentPath(GetDirectory(kind), document.FileName);
        if (File.Exists(path))
        {
            File.Delete(path);
        }

        return true;
    }

    public async Task<IReadOnlyList<(string DisplayName, string Content)>> ReadEnabledDocumentsAsync(
        AgentDocumentKind kind,
        int maxChars,
        CancellationToken cancellationToken = default)
    {
        if (maxChars <= 0)
        {
            return [];
        }

        var remaining = maxChars;
        var result = new List<(string DisplayName, string Content)>();
        var config = await configurations.LoadAsync(cancellationToken).ConfigureAwait(false);
        foreach (var document in GetDocuments(config, kind).Where(item => item.Enabled))
        {
            if (!IsSafeFileName(document.FileName))
            {
                continue;
            }

            var path = GetDocumentPath(GetDirectory(kind), document.FileName);
            if (!File.Exists(path))
            {
                continue;
            }

            var content = await File.ReadAllTextAsync(path, Encoding.UTF8, cancellationToken).ConfigureAwait(false);
            if (string.IsNullOrWhiteSpace(content))
            {
                continue;
            }

            if (content.Length > remaining)
            {
                content = content[..remaining];
            }

            result.Add((document.DisplayName, content));
            remaining -= content.Length;
            if (remaining <= 0)
            {
                break;
            }
        }

        return result;
    }

    private static void ValidateUpload(IFormFile file)
    {
        if (file == null)
        {
            throw new InvalidOperationException("Markdown 文件不能为空。");
        }

        if (!string.Equals(Path.GetExtension(file.FileName), ".md", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("只支持上传 .md Markdown 文件。");
        }

        if (file.Length <= 0)
        {
            throw new InvalidOperationException("Markdown 文件不能为空。");
        }

        if (file.Length > MaxMarkdownBytes)
        {
            throw new InvalidOperationException("Markdown 文件不能超过 2MB。");
        }
    }

    private static string CreateDocumentId(string originalFileName)
    {
        var baseName = Path.GetFileNameWithoutExtension(originalFileName);
        var slugChars = baseName
            .Trim()
            .ToLowerInvariant()
            .Select(ch => (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ? ch : '-')
            .ToArray();
        var slug = Regex.Replace(new string(slugChars), "-+", "-").Trim('-');
        if (string.IsNullOrWhiteSpace(slug))
        {
            slug = "document";
        }

        if (slug.Length > 40)
        {
            slug = slug[..40].Trim('-');
        }

        var guid = Guid.NewGuid().ToString("N")[..8];
        return $"{slug}-{guid}";
    }

    private static List<StoredAgentDocument> GetDocuments(AgentRuntimeConfiguration config, AgentDocumentKind kind)
    {
        return kind switch
        {
            AgentDocumentKind.Skill => config.Skills,
            AgentDocumentKind.Knowledge => config.Knowledge,
            _ => throw new ArgumentOutOfRangeException(nameof(kind), kind, null)
        };
    }

    private static StoredAgentDocument? FindDocument(
        AgentRuntimeConfiguration config,
        AgentDocumentKind kind,
        string id)
    {
        return GetDocuments(config, kind).FirstOrDefault(item =>
            string.Equals(item.Id, id, StringComparison.OrdinalIgnoreCase));
    }

    private string GetDirectory(AgentDocumentKind kind)
    {
        return kind switch
        {
            AgentDocumentKind.Skill => paths.SkillsDirectory,
            AgentDocumentKind.Knowledge => paths.KnowledgeDirectory,
            _ => throw new ArgumentOutOfRangeException(nameof(kind), kind, null)
        };
    }

    private static string GetDocumentPath(string directory, string fileName)
    {
        if (!IsSafeFileName(fileName))
        {
            throw new InvalidOperationException("文档文件名不安全。");
        }

        var root = Path.GetFullPath(directory);
        var comparison = OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        var rootPrefix = root.EndsWith(Path.DirectorySeparatorChar) ? root : root + Path.DirectorySeparatorChar;
        var fullPath = Path.GetFullPath(Path.Combine(root, fileName));
        if (!fullPath.StartsWith(rootPrefix, comparison))
        {
            throw new InvalidOperationException("文档路径不能解析到存储目录之外。");
        }

        return fullPath;
    }

    private static bool IsSafeFileName(string fileName)
    {
        return !string.IsNullOrWhiteSpace(fileName)
            && Path.GetFileName(fileName) == fileName
            && SafeFileNamePattern.IsMatch(fileName);
    }

    private static StoredAgentDocument Clone(StoredAgentDocument document)
    {
        return new StoredAgentDocument
        {
            Id = document.Id,
            DisplayName = document.DisplayName,
            FileName = document.FileName,
            Enabled = document.Enabled,
            BuiltIn = document.BuiltIn,
            SizeBytes = document.SizeBytes,
            UpdatedAt = document.UpdatedAt
        };
    }
}
