using System.Text.RegularExpressions;

namespace RoadProto.Agent.Host.Admin;

public static class AgentAdminEndpoints
{
    private static readonly Regex ProfileNamePattern = new(
        "^[A-Za-z0-9_-]+$",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    public static WebApplication MapAgentAdminEndpoints(this WebApplication app)
    {
        app.MapGet("/api/admin/status", async (
            AgentLocalPaths paths,
            AgentConfigurationStore store,
            CancellationToken cancellationToken) =>
        {
            var config = await store.LoadAsync(cancellationToken).ConfigureAwait(false);
            return Results.Ok(new AdminStatusResponse(
                paths.Root,
                config.DefaultModelProfile,
                config.ModelProfiles.Count,
                config.Skills.Count(item => item.Enabled),
                config.Knowledge.Count(item => item.Enabled)));
        });

        app.MapGet("/api/admin/model-profiles", async (
            AgentConfigurationStore store,
            CancellationToken cancellationToken) =>
        {
            var config = await store.LoadAsync(cancellationToken).ConfigureAwait(false);
            return Results.Ok(config.ModelProfiles
                .Select(profile => ToResponse(profile, config.DefaultModelProfile))
                .ToList());
        });

        app.MapPost("/api/admin/model-profiles", async (
            UpsertModelProfileRequest? request,
            AgentConfigurationStore store,
            AgentSecretStore secrets,
            CancellationToken cancellationToken) =>
        {
            if (request == null)
            {
                return Results.BadRequest("模型 Profile 请求不能为空。");
            }

            var validationError = ValidateUpsertRequest(request);
            if (validationError != null)
            {
                return Results.BadRequest(validationError);
            }

            var config = await store.LoadAsync(cancellationToken).ConfigureAwait(false);
            var name = request.Name.Trim();
            var existing = config.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, name, StringComparison.OrdinalIgnoreCase));
            var profile = existing ?? new StoredModelProfile { Name = name };

            profile.Provider = string.IsNullOrWhiteSpace(request.Provider)
                ? "OpenAICompatible"
                : request.Provider.Trim();
            profile.BaseUrl = request.BaseUrl.Trim();
            profile.Model = request.Model.Trim();
            profile.Temperature = request.Temperature;
            profile.TimeoutSeconds = request.TimeoutSeconds <= 0 ? 60 : request.TimeoutSeconds;

            if (!string.IsNullOrWhiteSpace(request.ApiKey))
            {
                profile.SecretId = AgentSecretStore.CreateSecretId(name);
                profile.ApiKeyMask = AgentSecretStore.Mask(request.ApiKey);
                await secrets.SaveAsync(profile.SecretId, request.ApiKey, cancellationToken).ConfigureAwait(false);
            }

            if (existing == null)
            {
                config.ModelProfiles.Add(profile);
            }

            if (request.IsDefault || !DefaultProfileExists(config))
            {
                config.DefaultModelProfile = profile.Name;
            }

            await store.SaveAsync(config, cancellationToken).ConfigureAwait(false);
            return Results.Ok(ToResponse(profile, config.DefaultModelProfile));
        });

        app.MapDelete("/api/admin/model-profiles/{name}", async (
            string name,
            AgentConfigurationStore store,
            AgentSecretStore secrets,
            CancellationToken cancellationToken) =>
        {
            if (!IsValidProfileName(name))
            {
                return Results.BadRequest("模型 Profile 名称只能包含字母、数字、下划线和连字符。");
            }

            var config = await store.LoadAsync(cancellationToken).ConfigureAwait(false);
            var profile = config.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, name, StringComparison.OrdinalIgnoreCase));
            if (profile == null)
            {
                return Results.NotFound();
            }

            config.ModelProfiles.Remove(profile);
            if (!string.IsNullOrWhiteSpace(profile.SecretId))
            {
                await secrets.DeleteAsync(profile.SecretId).ConfigureAwait(false);
            }

            if (string.Equals(config.DefaultModelProfile, profile.Name, StringComparison.OrdinalIgnoreCase)
                || !DefaultProfileExists(config))
            {
                config.DefaultModelProfile = config.ModelProfiles.FirstOrDefault()?.Name ?? string.Empty;
            }

            await store.SaveAsync(config, cancellationToken).ConfigureAwait(false);
            return Results.NoContent();
        });

        app.MapPost("/api/admin/model-profiles/{name}/default", async (
            string name,
            AgentConfigurationStore store,
            CancellationToken cancellationToken) =>
        {
            if (!IsValidProfileName(name))
            {
                return Results.BadRequest("模型 Profile 名称只能包含字母、数字、下划线和连字符。");
            }

            var config = await store.LoadAsync(cancellationToken).ConfigureAwait(false);
            var profile = config.ModelProfiles.FirstOrDefault(item =>
                string.Equals(item.Name, name, StringComparison.OrdinalIgnoreCase));
            if (profile == null)
            {
                return Results.NotFound();
            }

            config.DefaultModelProfile = profile.Name;
            await store.SaveAsync(config, cancellationToken).ConfigureAwait(false);
            return Results.Ok(ToResponse(profile, config.DefaultModelProfile));
        });

        MapDocumentEndpoints(app, "/api/admin/skills", AgentDocumentKind.Skill);
        MapDocumentEndpoints(app, "/api/admin/knowledge", AgentDocumentKind.Knowledge);

        return app;
    }

    private static void MapDocumentEndpoints(
        WebApplication app,
        string routePrefix,
        AgentDocumentKind kind)
    {
        app.MapGet(routePrefix, async (
            AgentDocumentStore documents,
            CancellationToken cancellationToken) =>
        {
            var list = await documents.ListAsync(kind, cancellationToken).ConfigureAwait(false);
            return Results.Ok(list.Select(ToResponse).ToList());
        });

        app.MapPost(routePrefix + "/upload", async (
            IFormFile file,
            AgentDocumentStore documents,
            CancellationToken cancellationToken) =>
        {
            try
            {
                var document = await documents.UploadMarkdownAsync(kind, file, cancellationToken).ConfigureAwait(false);
                return Results.Ok(ToResponse(document));
            }
            catch (InvalidOperationException ex)
            {
                return Results.BadRequest(ex.Message);
            }
        }).DisableAntiforgery();

        app.MapPatch(routePrefix + "/{id}", async (
            string id,
            UpdateDocumentRequest? request,
            AgentDocumentStore documents,
            CancellationToken cancellationToken) =>
        {
            if (request == null)
            {
                return Results.BadRequest("文档更新请求不能为空。");
            }

            try
            {
                var document = await documents.UpdateAsync(kind, id, request, cancellationToken).ConfigureAwait(false);
                return Results.Ok(ToResponse(document));
            }
            catch (KeyNotFoundException)
            {
                return Results.NotFound();
            }
        });

        app.MapDelete(routePrefix + "/{id}", async (
            string id,
            AgentDocumentStore documents,
            CancellationToken cancellationToken) =>
        {
            try
            {
                var deleted = await documents.DeleteAsync(kind, id, cancellationToken).ConfigureAwait(false);
                return deleted ? Results.NoContent() : Results.NotFound();
            }
            catch (InvalidOperationException ex)
            {
                return Results.BadRequest(ex.Message);
            }
        });
    }

    private static string? ValidateUpsertRequest(UpsertModelProfileRequest request)
    {
        if (!IsValidProfileName(request.Name))
        {
            return "模型 Profile 名称不能为空，且只能包含字母、数字、下划线和连字符。";
        }

        if (string.IsNullOrWhiteSpace(request.BaseUrl)
            || !Uri.TryCreate(request.BaseUrl.Trim(), UriKind.Absolute, out _))
        {
            return "模型 BaseUrl 必须是绝对 URI。";
        }

        if (string.IsNullOrWhiteSpace(request.Model))
        {
            return "模型名称不能为空。";
        }

        return null;
    }

    private static bool IsValidProfileName(string? name)
    {
        return !string.IsNullOrWhiteSpace(name)
            && ProfileNamePattern.IsMatch(name.Trim());
    }

    private static bool DefaultProfileExists(AgentRuntimeConfiguration config)
    {
        return !string.IsNullOrWhiteSpace(config.DefaultModelProfile)
            && config.ModelProfiles.Any(item =>
                string.Equals(item.Name, config.DefaultModelProfile, StringComparison.OrdinalIgnoreCase));
    }

    private static ModelProfileResponse ToResponse(StoredModelProfile profile, string defaultProfile)
    {
        return new ModelProfileResponse(
            profile.Name,
            profile.Provider,
            profile.BaseUrl,
            profile.Model,
            profile.Temperature,
            profile.TimeoutSeconds,
            profile.HasApiKey,
            profile.ApiKeyMask,
            string.Equals(profile.Name, defaultProfile, StringComparison.OrdinalIgnoreCase));
    }

    private static DocumentResponse ToResponse(StoredAgentDocument document)
    {
        return new DocumentResponse(
            document.Id,
            document.DisplayName,
            document.Enabled,
            document.BuiltIn,
            document.SizeBytes,
            document.UpdatedAt);
    }
}
