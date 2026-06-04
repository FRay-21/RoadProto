using System.Text;
using Microsoft.AspNetCore.Http;
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Providers;
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
        var repositoryRoot = Path.Combine(root, "repo");
        var builtInSkillPath = Path.Combine(
            repositoryRoot,
            "docs",
            "agent",
            "skills",
            "cross_section",
            "subgrade_template_create.md");
        Directory.CreateDirectory(Path.GetDirectoryName(builtInSkillPath)!);
        await File.WriteAllTextAsync(builtInSkillPath, "内置路基模板 skill 内容", Encoding.UTF8);

        var paths = new AgentLocalPaths(Path.Combine(root, "local"));
        var configStore = new AgentConfigurationStore(paths, new RoadProtoAgentOptions());
        var documentStore = new AgentDocumentStore(paths, configStore);
        await documentStore.UploadMarkdownAsync(
            AgentDocumentKind.Skill,
            CreateFormFile("用户横断面.md", "用户 skill 内容"));
        await documentStore.UploadMarkdownAsync(
            AgentDocumentKind.Knowledge,
            CreateFormFile("道路知识.md", "知识库内容"));

        var service = new AgentPromptContextService(
            new MarkdownSkillRepository(repositoryRoot),
            documentStore);

        var prompt = await service.BuildSystemPromptAsync();

        Assert.Contains("你是 RoadProto 设计软件原型 Agent", prompt);
        Assert.Contains("## 内置 Skill", prompt);
        Assert.Contains("内置路基模板 skill 内容", prompt);
        Assert.Contains("## 用户 Skill: 用户横断面.md", prompt);
        Assert.Contains("用户 skill 内容", prompt);
        Assert.Contains("## 知识库: 道路知识.md", prompt);
        Assert.Contains("知识库内容", prompt);
    }

    private static IFormFile CreateFormFile(string fileName, string content)
    {
        var bytes = Encoding.UTF8.GetBytes(content);
        return new FormFile(new MemoryStream(bytes), 0, bytes.Length, "file", fileName);
    }
}
