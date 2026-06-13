using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Reflection;
using System.Threading.Tasks;

namespace RoadProto.Terrain.UI.Services;

public static class AgentBackendProcess
{
    private static readonly object SyncRoot = new();
    private static readonly Uri HealthUri = new("http://127.0.0.1:17831/health");
    private static Process? ownedProcess;
    private static Task? startupTask;

    public static bool HasOwnedProcess
    {
        get
        {
            lock (SyncRoot)
            {
                return ownedProcess != null && !ownedProcess.HasExited;
            }
        }
    }

    public static Task StartOrAttachAsync()
    {
        lock (SyncRoot)
        {
            if (startupTask == null || startupTask.IsCompleted)
            {
                startupTask = StartOrAttachCoreAsync();
            }

            return startupTask;
        }
    }

    public static void StopOwnedProcess()
    {
        Process? process;
        lock (SyncRoot)
        {
            process = ownedProcess;
            ownedProcess = null;
            startupTask = null;
        }

        if (process == null)
        {
            return;
        }

        try
        {
            if (!process.HasExited)
            {
                process.Kill();
                process.WaitForExit(3000);
            }
        }
        catch
        {
        }
        finally
        {
            process.Dispose();
        }
    }

    private static async Task StartOrAttachCoreAsync()
    {
        if (await IsHealthyAsync().ConfigureAwait(false))
        {
            return;
        }

        Process process;
        lock (SyncRoot)
        {
            if (ownedProcess != null && !ownedProcess.HasExited)
            {
                return;
            }

            var startInfo = CreateStartInfo();
            process = Process.Start(startInfo)
                ?? throw new InvalidOperationException("未能启动 RoadProto Agent 后端进程。");
            ownedProcess = process;
        }

        try
        {
            await WaitUntilHealthyAsync().ConfigureAwait(false);
        }
        catch
        {
            StopOwnedProcess();
            throw;
        }
    }

    private static ProcessStartInfo CreateStartInfo()
    {
        var launchInfo = ResolveLaunchInfo();
        return new ProcessStartInfo
        {
            FileName = launchInfo.FileName,
            Arguments = launchInfo.Arguments,
            WorkingDirectory = launchInfo.WorkingDirectory,
            UseShellExecute = false,
            CreateNoWindow = true,
            WindowStyle = ProcessWindowStyle.Hidden,
        };
    }

    private static AgentHostLaunchInfo ResolveLaunchInfo()
    {
        foreach (var root in EnumerateProbeRoots())
        {
            foreach (var configuration in EnumerateConfigurations(root))
            {
                var hostDirectory = Path.Combine(root, "artifacts", "agent", configuration, "net8.0");
                var hostExe = Path.Combine(hostDirectory, "RoadProto.Agent.Host.exe");
                if (File.Exists(hostExe))
                {
                    return new AgentHostLaunchInfo(hostExe, string.Empty, hostDirectory);
                }

                var hostDll = Path.Combine(hostDirectory, "RoadProto.Agent.Host.dll");
                if (File.Exists(hostDll))
                {
                    return new AgentHostLaunchInfo("dotnet", Quote(hostDll), hostDirectory);
                }
            }
        }

        throw new FileNotFoundException("找不到 RoadProto.Agent.Host.exe，请先构建 Agent Host。");
    }

    private static IEnumerable<string> EnumerateProbeRoots()
    {
        var assemblyDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        var current = new DirectoryInfo(assemblyDirectory ?? AppDomain.CurrentDomain.BaseDirectory);
        while (current != null)
        {
            if (Directory.Exists(Path.Combine(current.FullName, "artifacts", "agent")) ||
                File.Exists(Path.Combine(current.FullName, "RoadProto.sln")))
            {
                yield return current.FullName;
            }

            current = current.Parent;
        }
    }

    private static IEnumerable<string> EnumerateConfigurations(string root)
    {
        var assemblyDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location) ?? string.Empty;
        var preferred = assemblyDirectory
            .Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
            .FirstOrDefault(part =>
                string.Equals(part, "Debug", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(part, "Release", StringComparison.OrdinalIgnoreCase));

        if (!string.IsNullOrWhiteSpace(preferred))
        {
            yield return preferred!;
        }

        yield return "Release";
        yield return "Debug";
    }

    private static async Task WaitUntilHealthyAsync()
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
        while (DateTime.UtcNow < deadline)
        {
            if (await IsHealthyAsync().ConfigureAwait(false))
            {
                return;
            }

            await Task.Delay(250).ConfigureAwait(false);
        }

        throw new TimeoutException("RoadProto Agent 后端启动超时。");
    }

    private static async Task<bool> IsHealthyAsync()
    {
        try
        {
            using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(1) };
            using var response = await client.GetAsync(HealthUri).ConfigureAwait(false);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    private static string Quote(string value)
    {
        return $"\"{value}\"";
    }

    private sealed class AgentHostLaunchInfo
    {
        public AgentHostLaunchInfo(string fileName, string arguments, string workingDirectory)
        {
            FileName = fileName;
            Arguments = arguments;
            WorkingDirectory = workingDirectory;
        }

        public string FileName { get; }

        public string Arguments { get; }

        public string WorkingDirectory { get; }
    }
}
