namespace RoadProto.Agent.Host.Skills;

public sealed class MarkdownSkillRepository
{
    private readonly string repositoryRoot;

    public MarkdownSkillRepository(string repositoryRoot)
    {
        this.repositoryRoot = repositoryRoot;
    }

    public string ReadSubgradeTemplateCreateSkill()
    {
        var path = Path.Combine(
            repositoryRoot,
            "docs",
            "agent",
            "skills",
            "cross_section",
            "subgrade_template_create.md");
        return File.Exists(path) ? File.ReadAllText(path, System.Text.Encoding.UTF8) : string.Empty;
    }
}
