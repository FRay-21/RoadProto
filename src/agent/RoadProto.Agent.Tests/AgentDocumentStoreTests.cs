using System.Text;
using Microsoft.AspNetCore.Http;
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Providers;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class AgentDocumentStoreTests : IDisposable
{
    private readonly string root = Path.Combine(Path.GetTempPath(), "RoadProtoAgentDocumentTests", Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        if (Directory.Exists(root))
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [Fact]
    public async Task UploadMarkdownSkillCreatesSafeDocument()
    {
        var paths = new AgentLocalPaths(root);
        var store = CreateStore(paths);
        var file = CreateFormFile("../dangerous skill.md", "# 用户 Skill\n请优先解释横断面参数。");

        var document = await store.UploadMarkdownAsync(AgentDocumentKind.Skill, file);

        var config = await CreateConfigStore(paths).LoadAsync();
        var stored = Assert.Single(config.Skills);
        Assert.Equal(document.Id, stored.Id);
        Assert.Equal("../dangerous skill.md", stored.DisplayName);
        Assert.True(stored.Enabled);
        Assert.False(stored.BuiltIn);
        Assert.EndsWith(".md", stored.FileName, StringComparison.OrdinalIgnoreCase);
        Assert.NotEqual("../dangerous skill.md", stored.FileName);
        Assert.DoesNotContain("..", stored.FileName);
        Assert.DoesNotContain(Path.DirectorySeparatorChar, stored.FileName);

        var fullPath = Path.Combine(paths.SkillsDirectory, stored.FileName);
        Assert.True(File.Exists(fullPath));
        Assert.Equal("# 用户 Skill\n请优先解释横断面参数。", await File.ReadAllTextAsync(fullPath, Encoding.UTF8));

        var enabled = await store.ReadEnabledDocumentsAsync(AgentDocumentKind.Skill, 10_000);
        var item = Assert.Single(enabled);
        Assert.Equal("../dangerous skill.md", item.DisplayName);
        Assert.Contains("横断面参数", item.Content);
    }

    [Fact]
    public async Task UploadRejectsNonMarkdown()
    {
        var store = CreateStore(new AgentLocalPaths(root));
        var file = CreateFormFile("notes.txt", "not markdown");

        await Assert.ThrowsAsync<InvalidOperationException>(
            () => store.UploadMarkdownAsync(AgentDocumentKind.Skill, file));
    }

    [Fact]
    public async Task CanDisableAndDeleteKnowledgeDocument()
    {
        var paths = new AgentLocalPaths(root);
        var store = CreateStore(paths);
        var document = await store.UploadMarkdownAsync(
            AgentDocumentKind.Knowledge,
            CreateFormFile("设计规范.md", "知识库内容"));
        var fullPath = Path.Combine(paths.KnowledgeDirectory, document.FileName);

        var updated = await store.UpdateAsync(
            AgentDocumentKind.Knowledge,
            document.Id,
            new UpdateDocumentRequest(false, "禁用后的知识"));

        Assert.False(updated.Enabled);
        Assert.Equal("禁用后的知识", updated.DisplayName);
        Assert.Empty(await store.ReadEnabledDocumentsAsync(AgentDocumentKind.Knowledge, 10_000));
        Assert.True(File.Exists(fullPath));

        var deleted = await store.DeleteAsync(AgentDocumentKind.Knowledge, document.Id);

        Assert.True(deleted);
        Assert.Empty(await store.ListAsync(AgentDocumentKind.Knowledge));
        Assert.False(File.Exists(fullPath));
    }

    private static AgentDocumentStore CreateStore(AgentLocalPaths paths)
    {
        return new AgentDocumentStore(paths, CreateConfigStore(paths));
    }

    private static AgentConfigurationStore CreateConfigStore(AgentLocalPaths paths)
    {
        return new AgentConfigurationStore(paths, new RoadProtoAgentOptions());
    }

    private static IFormFile CreateFormFile(string fileName, string content)
    {
        var bytes = Encoding.UTF8.GetBytes(content);
        return new FormFile(new MemoryStream(bytes), 0, bytes.Length, "file", fileName);
    }
}
