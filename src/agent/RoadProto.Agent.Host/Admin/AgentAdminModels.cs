namespace RoadProto.Agent.Host.Admin;

public sealed class AgentRuntimeConfiguration
{
    public string DefaultModelProfile { get; set; } = string.Empty;
    public List<StoredModelProfile> ModelProfiles { get; set; } = new();
    public List<StoredAgentDocument> Skills { get; set; } = new();
    public List<StoredAgentDocument> Knowledge { get; set; } = new();
}

public sealed class StoredModelProfile
{
    public string Name { get; set; } = string.Empty;
    public string Provider { get; set; } = "OpenAICompatible";
    public string BaseUrl { get; set; } = string.Empty;
    public string Model { get; set; } = string.Empty;
    public double Temperature { get; set; } = 0.2;
    public int TimeoutSeconds { get; set; } = 60;
    public string SecretId { get; set; } = string.Empty;
    public string ApiKeyMask { get; set; } = string.Empty;
    public bool HasApiKey => !string.IsNullOrWhiteSpace(SecretId);
}

public sealed class StoredAgentDocument
{
    public string Id { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;
    public string FileName { get; set; } = string.Empty;
    public bool Enabled { get; set; } = true;
    public bool BuiltIn { get; set; }
    public long SizeBytes { get; set; }
    public DateTimeOffset UpdatedAt { get; set; } = DateTimeOffset.UtcNow;
}

public sealed record AdminStatusResponse(
    string StorageRoot,
    string DefaultModelProfile,
    int ModelProfileCount,
    int EnabledSkillCount,
    int EnabledKnowledgeCount);

public sealed record UpsertModelProfileRequest(
    string Name,
    string Provider,
    string BaseUrl,
    string Model,
    double Temperature,
    int TimeoutSeconds,
    string? ApiKey,
    bool IsDefault);

public sealed record ModelProfileResponse(
    string Name,
    string Provider,
    string BaseUrl,
    string Model,
    double Temperature,
    int TimeoutSeconds,
    bool HasApiKey,
    string ApiKeyMask,
    bool IsDefault);

public sealed record DocumentResponse(
    string Id,
    string DisplayName,
    bool Enabled,
    bool BuiltIn,
    long SizeBytes,
    DateTimeOffset UpdatedAt);

public sealed record UpdateDocumentRequest(bool Enabled, string? DisplayName);
