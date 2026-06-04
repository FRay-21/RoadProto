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
