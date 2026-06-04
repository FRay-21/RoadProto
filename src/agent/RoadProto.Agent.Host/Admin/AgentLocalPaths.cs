namespace RoadProto.Agent.Host.Admin;

public sealed class AgentLocalPaths
{
    private const string ProjectLocalRootDirectory = ".roadproto-agent";
    private const string StorageRootEnvironmentVariable = "ROADPROTO_AGENT_STORAGE_ROOT";

    private static readonly string LegacyAppDataRoot = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "RoadProto",
        "Agent");

    private readonly bool migrateLegacyAppData;

    public AgentLocalPaths()
        : this(ResolveDefaultRoot(out var shouldMigrateLegacyAppData), shouldMigrateLegacyAppData)
    {
    }

    public AgentLocalPaths(string root)
        : this(root, migrateLegacyAppData: false)
    {
    }

    private AgentLocalPaths(string root, bool migrateLegacyAppData)
    {
        if (string.IsNullOrWhiteSpace(root))
        {
            throw new ArgumentException("Agent local storage root cannot be empty.", nameof(root));
        }

        Root = Path.GetFullPath(root);
        this.migrateLegacyAppData = migrateLegacyAppData;
    }

    public string Root { get; }
    public string ConfigPath => Path.Combine(Root, "config.json");
    public string SecretsDirectory => Path.Combine(Root, "secrets");
    public string SkillsDirectory => Path.Combine(Root, "skills");
    public string KnowledgeDirectory => Path.Combine(Root, "knowledge");

    public void Ensure()
    {
        if (migrateLegacyAppData)
        {
            MigrateLegacyAppDataRootIfNeeded();
        }

        Directory.CreateDirectory(Root);
        Directory.CreateDirectory(SecretsDirectory);
        Directory.CreateDirectory(SkillsDirectory);
        Directory.CreateDirectory(KnowledgeDirectory);
    }

    private static string ResolveDefaultRoot(out bool shouldMigrateLegacyAppData)
    {
        shouldMigrateLegacyAppData = false;
        var overrideRoot = Environment.GetEnvironmentVariable(StorageRootEnvironmentVariable);
        if (!string.IsNullOrWhiteSpace(overrideRoot))
        {
            return overrideRoot.Trim();
        }

        var repositoryRoot = FindRepositoryRoot(AppContext.BaseDirectory)
            ?? FindRepositoryRoot(Directory.GetCurrentDirectory());
        if (!string.IsNullOrWhiteSpace(repositoryRoot))
        {
            shouldMigrateLegacyAppData = true;
            return Path.Combine(repositoryRoot, ProjectLocalRootDirectory);
        }

        return LegacyAppDataRoot;
    }

    private static string? FindRepositoryRoot(string start)
    {
        var current = new DirectoryInfo(start);
        while (current != null)
        {
            if (Directory.Exists(Path.Combine(current.FullName, "docs", "agent"))
                && Directory.Exists(Path.Combine(current.FullName, "src", "agent")))
            {
                return current.FullName;
            }

            current = current.Parent;
        }

        return null;
    }

    private void MigrateLegacyAppDataRootIfNeeded()
    {
        var targetRoot = Path.GetFullPath(Root);
        var legacyRoot = Path.GetFullPath(LegacyAppDataRoot);
        var comparison = OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;

        if (string.Equals(targetRoot, legacyRoot, comparison)
            || !Directory.Exists(legacyRoot)
            || HasLocalData(targetRoot))
        {
            return;
        }

        Directory.CreateDirectory(targetRoot);
        CopyDirectory(legacyRoot, targetRoot);
    }

    private static bool HasLocalData(string root)
    {
        if (File.Exists(Path.Combine(root, "config.json")))
        {
            return true;
        }

        return new[] { "secrets", "skills", "knowledge" }
            .Select(name => Path.Combine(root, name))
            .Any(path => Directory.Exists(path) && Directory.EnumerateFileSystemEntries(path).Any());
    }

    private static void CopyDirectory(string sourceRoot, string targetRoot)
    {
        foreach (var directory in Directory.EnumerateDirectories(sourceRoot, "*", SearchOption.AllDirectories))
        {
            var relative = Path.GetRelativePath(sourceRoot, directory);
            Directory.CreateDirectory(Path.Combine(targetRoot, relative));
        }

        foreach (var file in Directory.EnumerateFiles(sourceRoot, "*", SearchOption.AllDirectories))
        {
            var relative = Path.GetRelativePath(sourceRoot, file);
            var target = Path.Combine(targetRoot, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            if (!File.Exists(target))
            {
                File.Copy(file, target);
            }
        }
    }
}
