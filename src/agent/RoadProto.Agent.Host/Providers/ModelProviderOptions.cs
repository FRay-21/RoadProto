namespace RoadProto.Agent.Host.Providers;

public sealed class RoadProtoAgentOptions
{
    public List<ModelProfileOptions> ModelProfiles { get; set; } = new();
}

public sealed class ModelProfileOptions
{
    public string Name { get; set; } = string.Empty;
    public string Provider { get; set; } = "OpenAICompatible";
    public string BaseUrl { get; set; } = string.Empty;
    public string ApiKeyEnvironmentVariable { get; set; } = string.Empty;
    public string Model { get; set; } = string.Empty;
    public double Temperature { get; set; } = 0.2;
    public int TimeoutSeconds { get; set; } = 60;
    public bool EnableStreaming { get; set; }
}
