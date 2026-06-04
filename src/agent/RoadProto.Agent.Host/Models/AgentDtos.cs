namespace RoadProto.Agent.Host.Models;

public sealed record AgentChatRequest(
    string? Message,
    string? ModelProfile,
    IReadOnlyList<AgentChatMessage>? History);

public sealed record AgentChatMessage(string Role, string Content);

public sealed record AgentChatResponse(
    string Reply,
    AgentToolCall? ToolCall,
    bool RequiresConfirmation);

public sealed record AgentToolCall(
    string Tool,
    string RequestId,
    object Arguments,
    string ConfirmationTitle,
    string ConfirmationBody);

public sealed record SubgradeTemplateCreateArguments(
    string TemplateName,
    double DisplayScale,
    string RoadGrade,
    string RoadCenterlineHandle,
    AgentInsertionPoint InsertionPoint,
    string ComponentSource,
    IReadOnlyList<SubgradeComponentArgument> Components);

public sealed record AgentInsertionPoint(string Mode, double? X, double? Y, double Z);

public sealed record SubgradeComponentArgument(
    string Side,
    string Type,
    double Width,
    double Height,
    string SlopeMode,
    double FixedSlope,
    AgentColor? Color,
    IReadOnlyList<AgentStationValue> WideningTable,
    IReadOnlyList<AgentStationValue> VariableSlopeTable,
    AgentPavementLayer PavementLayer);

public sealed record AgentColor(int R, int G, int B);

public sealed record AgentStationValue(double Station, double Value);

public sealed record AgentPavementLayer(bool Linked, string Handle, string Name, double Thickness);
