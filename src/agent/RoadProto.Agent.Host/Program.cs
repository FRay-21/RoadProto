using Microsoft.Extensions.Options;
using RoadProto.Agent.Host.Admin;
using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Providers;
using RoadProto.Agent.Host.Services;
using RoadProto.Agent.Host.Skills;
using RoadProto.Agent.Host.Tools;

var builder = WebApplication.CreateBuilder(args);

builder.Services.Configure<RoadProtoAgentOptions>(
    builder.Configuration.GetSection("RoadProtoAgent"));
builder.Services.AddSingleton<AgentLocalPaths>();
builder.Services.AddSingleton(serviceProvider => new AgentConfigurationStore(
    serviceProvider.GetRequiredService<AgentLocalPaths>(),
    serviceProvider.GetRequiredService<IOptions<RoadProtoAgentOptions>>().Value));
builder.Services.AddSingleton<AgentSecretStore>();
builder.Services.AddSingleton<AgentDocumentStore>();
builder.Services.AddSingleton<SubgradeTemplateCreatePlanner>();
builder.Services.AddSingleton<AgentPlanner>();
builder.Services.AddSingleton<OpenAiCompatibleChatClient>();
builder.Services.AddSingleton(serviceProvider =>
{
    var root = FindRepositoryRoot(AppContext.BaseDirectory);
    return new MarkdownSkillRepository(root);
});
builder.Services.AddSingleton<AgentPromptContextService>();
builder.Services.AddSingleton<AgentChatService>();

var app = builder.Build();

app.Use(async (context, next) =>
{
    if (string.Equals(context.Request.Path.Value, "/admin", StringComparison.OrdinalIgnoreCase))
    {
        context.Response.Redirect("/admin/");
        return;
    }

    await next().ConfigureAwait(false);
});

app.UseDefaultFiles();
app.UseStaticFiles();

app.MapGet("/health", () => Results.Ok(new { status = "ok" }));

app.MapPost("/api/chat", async (
    AgentChatRequest? request,
    AgentChatService service,
    HttpContext httpContext) =>
{
    if (request == null || string.IsNullOrWhiteSpace(request.Message))
    {
        return Results.BadRequest(new AgentChatResponse(
            "请输入要发送给 RoadProto Agent 的消息。",
            null,
            false));
    }

    var validatedRequest = request with { Message = request.Message.Trim() };
    var response = await service.ReplyAsync(
        validatedRequest,
        httpContext.RequestAborted).ConfigureAwait(false);
    return Results.Ok(response);
});

app.MapAgentAdminEndpoints();

app.Run("http://127.0.0.1:17831");

static string FindRepositoryRoot(string start)
{
    var current = new DirectoryInfo(start);
    while (current != null)
    {
        if (Directory.Exists(Path.Combine(current.FullName, "docs", "agent")))
        {
            return current.FullName;
        }

        current = current.Parent;
    }

    return Directory.GetCurrentDirectory();
}

public partial class Program
{
}
