using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public sealed class AgentChatRequest
{
    public string Message { get; set; } = string.Empty;

    public string? ModelProfile { get; set; }

    public List<AgentChatMessage> History { get; set; } = new();
}

public sealed class AgentChatMessage
{
    public string Role { get; set; } = string.Empty;

    public string Content { get; set; } = string.Empty;
}

public sealed class AgentChatResponse
{
    public string Reply { get; set; } = string.Empty;

    public AgentToolCall? ToolCall { get; set; }

    public bool RequiresConfirmation { get; set; }
}

public sealed class AgentToolCall
{
    public string Tool { get; set; } = string.Empty;

    public string RequestId { get; set; } = string.Empty;

    public object? Arguments { get; set; }

    public string ConfirmationTitle { get; set; } = string.Empty;

    public string ConfirmationBody { get; set; } = string.Empty;
}
