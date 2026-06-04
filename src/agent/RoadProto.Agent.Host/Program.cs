using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Tools;

var builder = WebApplication.CreateBuilder(args);
builder.Services.AddSingleton<SubgradeTemplateToolPlanner>();

var app = builder.Build();

app.MapGet("/health", () => Results.Ok(new { status = "ok" }));

app.MapPost("/api/chat", (AgentChatRequest request, SubgradeTemplateToolPlanner planner) =>
{
    var toolCall = planner.TryPlan(request.Message);
    if (toolCall != null)
    {
        return Results.Ok(new AgentChatResponse(
            "我识别到你要创建路基模板。请确认工具调用参数。",
            toolCall,
            true));
    }

    return Results.Ok(new AgentChatResponse(
        "我可以回答 RoadProto 操作问题，也可以在确认后调用受控工具。当前首个工具是创建路基模板。",
        null,
        false));
});

app.Run("http://127.0.0.1:17831");
