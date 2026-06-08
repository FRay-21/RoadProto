# Agent Planner V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将当前硬编码 `SubgradeTemplateToolPlanner` 升级为可扩展的 `AgentPlanner`，并打通“默认/市政道路/默认基础上右侧增加行车道”的路基模板自然语言创建链路。

**Architecture:** 后端采用 `AgentPlanner` 总入口和具体工具 planner 分层。`tool skill` 作为工具规划契约和模型提示资料，核心可执行规则仍在本地代码中做确定性解析、默认值补齐和安全校验；C++ 白名单工具网关继续作为最终执行边界。首版只实现路基模板创建工具，不引入任意 CAD 命令执行。

**Tech Stack:** .NET 8 Agent Host、xUnit 后端测试、C++17 application/agent 工具协议、RoadProto core tests、WPF .NET Framework 4.8 请求文件桥接。

---

## 文件结构

- 新增 `src/agent/RoadProto.Agent.Host/Tools/AgentPlanner.cs`：Agent 规划总入口，负责调度具体工具 planner。
- 新增 `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateCreatePlanner.cs`：路基模板创建工具 planner，承载触发、别名、默认值和局部操作规则。
- 新增 `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateComponentOperationFactory.cs`：后端生成路基模板部件操作的辅助规则。
- 修改 `src/agent/RoadProto.Agent.Host/Models/AgentDtos.cs`：给 `SubgradeTemplateCreateArguments` 增加 `ComponentOperations`，并增加后端 DTO。
- 修改 `src/agent/RoadProto.Agent.Host/Services/AgentChatService.cs`：依赖 `AgentPlanner`，确认短句回看历史也通过 `AgentPlanner`。
- 修改 `src/agent/RoadProto.Agent.Host/Program.cs`：注册 `AgentPlanner` 和具体工具 planner。
- 修改 `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs`：改为或迁移到 `AgentPlannerTests`，覆盖新意图。
- 修改 `src/agent/RoadProto.Agent.Tests/AgentChatServiceTests.cs` 和 `src/agent/RoadProto.Agent.Tests/AgentChatApiTests.cs`：覆盖新 ToolCall 和确认回看。
- 修改 `src/application/agent/AgentToolRequest.h`：增加 C++ `AgentToolSubgradeComponentOperation` DTO。
- 修改 `src/application/agent/AgentToolRequest.cpp`：解析 `arguments.componentOperations`。
- 修改 `src/application/agent/SubgradeTemplateToolMapper.cpp`：在默认模板基础上应用 `AddComponent` 操作。
- 修改 `tests/core_tests.cpp`：增加工具协议解析和 mapper 行为测试。
- 修改 `docs/agent/skills/cross_section/subgrade_template_create.md`：按 tool skill 结构重写触发、别名、默认值、局部修改和示例映射。
- 修改 `docs/agent/tool_protocol.md`：补充 `componentOperations` 协议。
- 修改 `docs/architecture/agent_code_structure.md`、`docs/agent/overview.md`、`docs/business/agent/设计软件原型Agent.md`、`docs/modules/agent.md`、`docs/reuse/agent_tool_gateway.md`、`tests/README.md`、`docs/dev/version_log.md`：同步 Agent Planner V1 说明。

---

### Task 1: 后端 Planner 命名和入口升级

**Files:**
- Create: `src/agent/RoadProto.Agent.Host/Tools/AgentPlanner.cs`
- Create: `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateCreatePlanner.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Program.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Services/AgentChatService.cs`
- Modify: `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs`

- [ ] **Step 1: 写失败测试，证明 `AgentPlanner` 是新入口**

将 `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs` 中的类重命名为 `AgentPlannerTests`，并把首个测试改为：

```csharp
[Fact]
public void PlansSecondClassSubgradeTemplateThroughAgentPlanner()
{
    var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

    var result = planner.TryPlan("帮我创建一个二级公路路基模板，比例1:20，名字叫K1路基模板", out var guidance);

    Assert.Null(guidance);
    Assert.NotNull(result);
    Assert.Equal("cross_section.subgrade_template.create", result.Tool);
    var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
    Assert.Equal("K1路基模板", args.TemplateName);
    Assert.Equal(20, args.DisplayScale);
    Assert.Equal("SecondClass", args.RoadGrade);
    Assert.Equal("DefaultByRoadGrade", args.ComponentSource);
    Assert.Empty(args.Components);
    Assert.Empty(args.ComponentOperations);
    Assert.Equal("PickInCad", args.InsertionPoint.Mode);
}
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AgentPlannerTests
```

Expected: FAIL，原因是 `AgentPlanner`、`ComponentOperations` 或新构造函数不存在。

- [ ] **Step 3: 实现最小 Planner 入口**

新增 `src/agent/RoadProto.Agent.Host/Tools/AgentPlanner.cs`：

```csharp
using RoadProto.Agent.Host.Models;

namespace RoadProto.Agent.Host.Tools;

public sealed class AgentPlanner
{
    private readonly SubgradeTemplateCreatePlanner subgradeTemplateCreatePlanner;

    public AgentPlanner(SubgradeTemplateCreatePlanner subgradeTemplateCreatePlanner)
    {
        this.subgradeTemplateCreatePlanner = subgradeTemplateCreatePlanner;
    }

    public AgentToolCall? TryPlan(string message, out string? guidance)
    {
        return subgradeTemplateCreatePlanner.TryPlan(message, out guidance);
    }
}
```

将原 `SubgradeTemplateToolPlanner` 内容迁移到 `SubgradeTemplateCreatePlanner`，类名改为：

```csharp
public sealed class SubgradeTemplateCreatePlanner
```

在 `Program.cs` 中替换注册：

```csharp
builder.Services.AddSingleton<SubgradeTemplateCreatePlanner>();
builder.Services.AddSingleton<AgentPlanner>();
```

在 `AgentChatService.cs` 中把字段和构造函数参数从 `SubgradeTemplateToolPlanner` 替换为 `AgentPlanner`，调用保持：

```csharp
var toolCall = planner.TryPlan(message, out var guidance);
```

- [ ] **Step 4: 运行测试确认通过**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AgentPlannerTests
```

Expected: PASS。

- [ ] **Step 5: 提交**

```powershell
git add src\agent\RoadProto.Agent.Host\Tools src\agent\RoadProto.Agent.Host\Program.cs src\agent\RoadProto.Agent.Host\Services\AgentChatService.cs src\agent\RoadProto.Agent.Tests\SubgradeTemplateToolPlannerTests.cs
git commit -m "refactor: introduce agent planner entrypoint"
```

---

### Task 2: 市政道路和缺省参数识别

**Files:**
- Modify: `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateCreatePlanner.cs`
- Modify: `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs`

- [ ] **Step 1: 写失败测试，覆盖市政道路和泛称模板**

追加测试：

```csharp
[Theory]
[InlineData("创建市政道路路基模板")]
[InlineData("创建城市道路路基模板")]
[InlineData("帮我生成一个市政道路的模板")]
public void PlansMunicipalRoadAsUrbanArterial(string message)
{
    var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

    var result = planner.TryPlan(message, out var guidance);

    Assert.Null(guidance);
    Assert.NotNull(result);
    var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
    Assert.Equal("UrbanArterial", args.RoadGrade);
    Assert.Equal("默认路基模板", args.TemplateName);
    Assert.Equal(10, args.DisplayScale);
    Assert.Equal("DefaultByRoadGrade", args.ComponentSource);
}
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter PlansMunicipalRoadAsUrbanArterial
```

Expected: FAIL，当前会回退 `Expressway` 或不识别“市政道路的模板”。

- [ ] **Step 3: 实现触发和等级别名**

在 `SubgradeTemplateCreatePlanner` 中把创建触发拆成方法：

```csharp
private static bool IsCreateIntent(string message)
{
    return message.Contains("创建", StringComparison.Ordinal)
        || message.Contains("新建", StringComparison.Ordinal)
        || message.Contains("生成", StringComparison.Ordinal)
        || message.Contains("做一个", StringComparison.Ordinal)
        || message.Contains("来一个", StringComparison.Ordinal);
}

private static bool MentionsSubgradeTemplateConcept(string message)
{
    if (message.Contains("路基模板", StringComparison.Ordinal))
    {
        return true;
    }

    return message.Contains("模板", StringComparison.Ordinal)
        && (message.Contains("市政道路", StringComparison.Ordinal)
            || message.Contains("城市道路", StringComparison.Ordinal)
            || message.Contains("城市主干", StringComparison.Ordinal)
            || message.Contains("城市次干", StringComparison.Ordinal)
            || message.Contains("城市支路", StringComparison.Ordinal)
            || message.Contains("城市快速", StringComparison.Ordinal));
}
```

替换原先仅检查 `路基模板` 和 `创建/新建/生成` 的判断。扩展 `DetectRoadGrade`：

```csharp
if (message.Contains("市政道路", StringComparison.Ordinal)
    || message.Contains("城市道路", StringComparison.Ordinal)
    || message.Contains("市政路", StringComparison.Ordinal)
    || message.Contains("城区道路", StringComparison.Ordinal))
{
    return "UrbanArterial";
}
```

该规则放在 `城市快速/城市主干/城市次干/城市支路` 精确判断之后，避免覆盖用户明确道路等级。

- [ ] **Step 4: 运行测试确认通过**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter PlansMunicipalRoadAsUrbanArterial
```

Expected: PASS。

- [ ] **Step 5: 提交**

```powershell
git add src\agent\RoadProto.Agent.Host\Tools\SubgradeTemplateCreatePlanner.cs src\agent\RoadProto.Agent.Tests\SubgradeTemplateToolPlannerTests.cs
git commit -m "feat: map municipal road template intent"
```

---

### Task 3: 后端部件操作计划

**Files:**
- Modify: `src/agent/RoadProto.Agent.Host/Models/AgentDtos.cs`
- Create: `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateComponentOperationFactory.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateCreatePlanner.cs`
- Modify: `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs`

- [ ] **Step 1: 写失败测试，覆盖右侧增加行车道**

追加测试：

```csharp
[Theory]
[InlineData("我想创建一个市政道路的路基模板，并基于默认参数上，最右侧增加一个行车道部件")]
[InlineData("创建城市道路模板，在右边再加一条车道")]
[InlineData("生成市政路模板，右侧外侧增加机动车道")]
public void PlansRightTravelLaneOperationOnDefaultUrbanTemplate(string message)
{
    var planner = new AgentPlanner(new SubgradeTemplateCreatePlanner());

    var result = planner.TryPlan(message, out var guidance);

    Assert.Null(guidance);
    Assert.NotNull(result);
    var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
    Assert.Equal("UrbanArterial", args.RoadGrade);
    Assert.Equal("DefaultByRoadGrade", args.ComponentSource);
    var operation = Assert.Single(args.ComponentOperations);
    Assert.Equal("AddComponent", operation.Action);
    Assert.Equal("Right", operation.Side);
    Assert.Equal("TravelLane", operation.Type);
    Assert.Equal("OutermostMotorLane", operation.Position);
    Assert.Null(operation.Width);
    Assert.Contains("右侧机动车道组外侧新增 1 个行车道", result.ConfirmationBody);
}
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter PlansRightTravelLaneOperationOnDefaultUrbanTemplate
```

Expected: FAIL，当前遇到“行车道/车道”会返回 guidance，不生成 ToolCall。

- [ ] **Step 3: 增加后端 DTO**

在 `AgentDtos.cs` 中把 `SubgradeTemplateCreateArguments` 改为：

```csharp
public sealed record SubgradeTemplateCreateArguments(
    string TemplateName,
    double DisplayScale,
    string RoadGrade,
    string RoadCenterlineHandle,
    AgentInsertionPoint InsertionPoint,
    string ComponentSource,
    IReadOnlyList<SubgradeComponentArgument> Components,
    IReadOnlyList<SubgradeComponentOperationArgument> ComponentOperations);
```

新增：

```csharp
public sealed record SubgradeComponentOperationArgument(
    string Action,
    string Side,
    string Type,
    string Position,
    double? Width);
```

更新已有构造调用，默认传 `Array.Empty<SubgradeComponentOperationArgument>()`。

- [ ] **Step 4: 实现操作工厂**

新增 `SubgradeTemplateComponentOperationFactory.cs`：

```csharp
using RoadProto.Agent.Host.Models;

namespace RoadProto.Agent.Host.Tools;

public static class SubgradeTemplateComponentOperationFactory
{
    public static IReadOnlyList<SubgradeComponentOperationArgument> DetectOperations(string message)
    {
        if (!MentionsRightSide(message) || !MentionsTravelLane(message) || !MentionsAdd(message))
        {
            return Array.Empty<SubgradeComponentOperationArgument>();
        }

        return new[]
        {
            new SubgradeComponentOperationArgument(
                "AddComponent",
                "Right",
                "TravelLane",
                "OutermostMotorLane",
                null)
        };
    }

    private static bool MentionsRightSide(string message)
    {
        return message.Contains("最右", StringComparison.Ordinal)
            || message.Contains("右侧", StringComparison.Ordinal)
            || message.Contains("右边", StringComparison.Ordinal);
    }

    private static bool MentionsTravelLane(string message)
    {
        return message.Contains("行车道", StringComparison.Ordinal)
            || message.Contains("车道", StringComparison.Ordinal)
            || message.Contains("机动车道", StringComparison.Ordinal);
    }

    private static bool MentionsAdd(string message)
    {
        return message.Contains("增加", StringComparison.Ordinal)
            || message.Contains("新增", StringComparison.Ordinal)
            || message.Contains("加一", StringComparison.Ordinal)
            || message.Contains("再加", StringComparison.Ordinal)
            || message.Contains("添加", StringComparison.Ordinal);
    }
}
```

- [ ] **Step 5: 接入 planner**

在 `SubgradeTemplateCreatePlanner.TryPlan` 中先获取：

```csharp
var componentOperations = SubgradeTemplateComponentOperationFactory.DetectOperations(message);
```

把原 `DescribesExplicitComponents` 拒绝逻辑改为：仅当识别到具体部件词且 `componentOperations.Count == 0` 时才返回 guidance。构造参数时传入 `componentOperations`。

确认文案按操作补充：

```csharp
var operationSummary = componentOperations.Count > 0
    ? "，并在右侧机动车道组外侧新增 1 个行车道，宽度按道路等级默认值补齐"
    : string.Empty;
```

- [ ] **Step 6: 运行后端测试确认通过**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter AgentPlannerTests
```

Expected: PASS。

- [ ] **Step 7: 提交**

```powershell
git add src\agent\RoadProto.Agent.Host\Models\AgentDtos.cs src\agent\RoadProto.Agent.Host\Tools src\agent\RoadProto.Agent.Tests\SubgradeTemplateToolPlannerTests.cs
git commit -m "feat: plan subgrade component operations"
```

---

### Task 4: C++ 工具协议解析 componentOperations

**Files:**
- Modify: `src/application/agent/AgentToolRequest.h`
- Modify: `src/application/agent/AgentToolRequest.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: 写失败测试，解析 componentOperations**

在 `tests/core_tests.cpp` 的 Agent 工具请求测试区追加：

```cpp
void agentToolRequestParsesSubgradeComponentOperations()
{
    using roadproto::application::agent::parseAgentToolRequestJson;

    const std::string json =
        "{"
        "\"tool\":\"cross_section.subgrade_template.create\","
        "\"arguments\":{"
        "\"componentSource\":\"DefaultByRoadGrade\","
        "\"componentOperations\":[{"
        "\"action\":\"AddComponent\","
        "\"side\":\"Right\","
        "\"type\":\"TravelLane\","
        "\"position\":\"OutermostMotorLane\""
        "}]"
        "}"
        "}";

    std::wstring error;
    const auto request = parseAgentToolRequestJson(json, error);
    CHECK(error.empty());
    CHECK(request.succeeded);
    CHECK(request.arguments.componentOperations.size() == 1);
    const auto& operation = request.arguments.componentOperations.front();
    CHECK(operation.action == L"AddComponent");
    CHECK(operation.side == L"Right");
    CHECK(operation.type == L"TravelLane");
    CHECK(operation.position == L"OutermostMotorLane");
    CHECK(!operation.hasWidth);
}
```

把该测试函数加入 `main()` 测试调用列表。

- [ ] **Step 2: 构建并运行 core tests，确认失败**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL 或编译失败，原因是 `componentOperations` DTO 不存在。

- [ ] **Step 3: 增加 C++ DTO**

在 `AgentToolRequest.h` 中新增：

```cpp
struct AgentToolSubgradeComponentOperation {
    std::wstring action;
    std::wstring side;
    std::wstring type;
    std::wstring position;
    double width = 0.0;
    bool hasWidth = false;
};
```

并在 `AgentSubgradeTemplateCreateArguments` 中新增：

```cpp
std::vector<AgentToolSubgradeComponentOperation> componentOperations;
```

- [ ] **Step 4: 解析 JSON**

在 `AgentToolRequest.cpp` 中仿照 `components` 解析 `arguments.componentOperations`，每个操作读取：

```cpp
readRequiredString(&item, L"action", path + L".action", operation.action, errorMessage)
readRequiredString(&item, L"side", path + L".side", operation.side, errorMessage)
readRequiredString(&item, L"type", path + L".type", operation.type, errorMessage)
readRequiredString(&item, L"position", path + L".position", operation.position, errorMessage)
```

`width` 为可选 number；如果存在则设置 `hasWidth = true`。

- [ ] **Step 5: 运行 core tests 确认通过新增解析测试**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: PASS。

- [ ] **Step 6: 提交**

```powershell
git add src\application\agent\AgentToolRequest.h src\application\agent\AgentToolRequest.cpp tests\core_tests.cpp
git commit -m "feat: parse agent component operations"
```

---

### Task 5: C++ 默认模板上应用新增行车道

**Files:**
- Modify: `src/application/agent/SubgradeTemplateToolMapper.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: 写失败测试，默认城市主干路右侧新增行车道**

在 `tests/core_tests.cpp` 追加：

```cpp
void agentSubgradeToolAppliesRightTravelLaneOperation()
{
    using roadproto::application::agent::AgentSubgradeTemplateCreateArguments;
    using roadproto::application::agent::AgentToolSubgradeComponentOperation;
    using roadproto::application::agent::buildSubgradeTemplateToolData;
    using roadproto::domain::cross_section::RoadGrade;
    using roadproto::domain::cross_section::SubgradeComponentType;
    using roadproto::domain::cross_section::SubgradeSide;

    AgentSubgradeTemplateCreateArguments arguments;
    arguments.templateName = L"市政道路模板";
    arguments.displayScale = 10.0;
    arguments.roadGrade = L"UrbanArterial";
    arguments.componentSource = L"DefaultByRoadGrade";

    AgentToolSubgradeComponentOperation operation;
    operation.action = L"AddComponent";
    operation.side = L"Right";
    operation.type = L"TravelLane";
    operation.position = L"OutermostMotorLane";
    arguments.componentOperations.push_back(operation);

    std::wstring error;
    const auto result = buildSubgradeTemplateToolData(arguments, error);
    CHECK(result.succeeded);
    CHECK(error.empty());
    CHECK(result.data.properties.roadGrade == RoadGrade::UrbanArterial);

    int rightLaneCount = 0;
    for (const auto& component : result.data.components) {
        if (component.side == SubgradeSide::Right && component.type == SubgradeComponentType::TravelLane) {
            ++rightLaneCount;
            CHECK(std::fabs(component.width - 3.5) < 1.0e-9);
        }
    }
    CHECK(rightLaneCount == 3);
}
```

加入测试调用列表。

- [ ] **Step 2: 构建并运行 core tests，确认失败**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL，当前 mapper 不处理 `componentOperations`。

- [ ] **Step 3: 实现操作校验和应用**

在 `SubgradeTemplateToolMapper.cpp` 中新增：

```cpp
bool isSubgradeComponentOperationActionCode(const std::wstring& code)
{
    return code == L"AddComponent";
}

bool isSubgradeComponentOperationPositionCode(const std::wstring& code)
{
    return code == L"OutermostMotorLane";
}
```

新增 `applyAddComponentOperation`，规则：

- `action` 必须为 `AddComponent`。
- `side` 必须为 `Left` 或 `Right`。
- `type` 必须为已支持部件类型。
- `position` 首版只支持 `OutermostMotorLane`。
- 当 `width` 缺省时，在同侧同类型部件中查找最后一个宽度作为默认值；对城市主干路行车道应得到 `3.5`。
- 插入位置为同侧最后一个 `TravelLane` 之后。
- 找不到同侧 `TravelLane` 时返回错误，不猜测工程宽度。

核心插入逻辑：

```cpp
auto insertPosition = data.components.end();
double width = operation.hasWidth ? operation.width : 0.0;
bool hasDefaultWidth = operation.hasWidth;

for (auto it = data.components.begin(); it != data.components.end(); ++it) {
    if (it->side == side && it->type == type) {
        insertPosition = std::next(it);
        if (!operation.hasWidth) {
            width = it->width;
            hasDefaultWidth = true;
        }
    }
}

if (!hasDefaultWidth) {
    errorMessage = L"Cannot infer default width for component operation.";
    return false;
}

SubgradeTemplateComponent component;
component.side = side;
component.type = type;
component.width = width;
component.color = SubgradeTemplateDefaults::defaultColorFor(type);
data.components.insert(insertPosition, component);
return true;
```

在 `buildSubgradeTemplateToolData` 中创建默认模板后遍历 `arguments.componentOperations` 并应用。

- [ ] **Step 4: 运行 core tests 确认通过**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: PASS。

- [ ] **Step 5: 提交**

```powershell
git add src\application\agent\SubgradeTemplateToolMapper.cpp tests\core_tests.cpp
git commit -m "feat: apply subgrade component operations"
```

---

### Task 6: 聊天 API、WPF 请求文件和确认文案验证

**Files:**
- Modify: `src/agent/RoadProto.Agent.Tests/AgentChatApiTests.cs`
- Modify: `src/agent/RoadProto.Agent.Tests/AgentChatServiceTests.cs`
- Modify: `tests/RoadProtoManagedBridgeTests/Program.cs` if existing WPF bridge tests need explicit coverage

- [ ] **Step 1: 写失败测试，API 返回 componentOperations**

在 `AgentChatApiTests.cs` 追加：

```csharp
[Fact]
public async Task ChatReturnsToolCallForMunicipalRoadWithRightLaneOperation()
{
    using var app = new RoadProtoAgentTestApplication();
    using var client = app.CreateClient();

    using var response = await client.PostAsJsonAsync(
        "/api/chat",
        new AgentChatRequest(
            "我想创建一个市政道路的路基模板，并基于默认参数上，最右侧增加一个行车道部件",
            null,
            Array.Empty<AgentChatMessage>()));

    response.EnsureSuccessStatusCode();
    var body = await response.Content.ReadFromJsonAsync<AgentChatResponse>();
    Assert.NotNull(body);
    Assert.True(body.RequiresConfirmation);
    Assert.NotNull(body.ToolCall);
    Assert.Contains("右侧机动车道组外侧新增 1 个行车道", body.ToolCall.ConfirmationBody);
}
```

- [ ] **Step 2: 运行测试确认失败或覆盖缺口**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj --filter ChatReturnsToolCallForMunicipalRoadWithRightLaneOperation
```

Expected: 在前面任务完成后 PASS；如果失败，修正 API 序列化或测试 helper。

- [ ] **Step 3: 检查 WPF 请求文件序列化**

确认 `AgentToolExecutionFile.NormalizeJsonValue` 会把 `componentOperations` 写入 `arguments`。如果已有动态 object 可自然序列化，不改 WPF；若测试发现丢字段，则在 WPF bridge DTO 中增加强类型或字典保持字段。

- [ ] **Step 4: 运行后端全量测试**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
```

Expected: PASS。

- [ ] **Step 5: 提交**

```powershell
git add src\agent\RoadProto.Agent.Tests tests\RoadProtoManagedBridgeTests
git commit -m "test: cover agent planner chat flow"
```

---

### Task 7: Tool Skill 和协议文档同步

**Files:**
- Modify: `docs/agent/skills/cross_section/subgrade_template_create.md`
- Modify: `docs/agent/tool_protocol.md`
- Modify: `docs/agent/skill_authoring_rules.md`

- [ ] **Step 1: 重写路基模板 tool skill**

把 `docs/agent/skills/cross_section/subgrade_template_create.md` 调整为以下结构：

```markdown
# 创建路基模板 Tool Skill

## Tool 身份

- Tool ID：`cross_section.subgrade_template.create`
- 能力：创建 `DnSubgradeTemplateEntity` 路基模板实体。
- 执行方式：Agent Planner 生成 ToolCall，WPF 确认后由 C++ 白名单工具网关执行。

## 触发语义

- 创建、新建、生成、做一个、来一个。
- 路基模板、道路模板，且上下文出现市政道路、城市道路、城市主干路、城市次干路、城市支路或城市快速路。

## 别名归一化

- `市政道路`、`城市道路`、`市政路`、`城区道路`：默认 `UrbanArterial`。
- `城市快速路`：`UrbanExpressway`。
- `城市主干路`：`UrbanArterial`。
- `城市次干路`：`UrbanSubArterial`。
- `城市支路`：`UrbanBranch`。
- `车道`、`机动车道`、`行车道`：`TravelLane`。

## 默认值规则

- 未给模板名：`默认路基模板`。
- 未给显示比例：`10`。
- 未给插入点：`PickInCad`。
- 用户说“默认参数基础上”：先使用 `DefaultByRoadGrade`，再应用 `componentOperations`。
- 新增行车道未给宽度：使用同侧同类型默认部件宽度。

## 局部修改规则

- “最右侧增加一个行车道”“右边再加一条车道”“右侧外侧增加机动车道”：
  - `action = AddComponent`
  - `side = Right`
  - `type = TravelLane`
  - `position = OutermostMotorLane`
  - `width = null`

## 追问规则

- 用户描述具体部件，但无法判断操作、侧别或类型时追问。
- 用户给出非法显示比例时拒绝并提示允许值。
- 用户要求绑定路面结构层但没有可信 handle 时追问。

## 示例映射

用户：`我想创建一个市政道路的模板，并基于默认参数上，最右侧增加一个行车道部件`

Planner 输出：

```json
{
  "tool": "cross_section.subgrade_template.create",
  "arguments": {
    "templateName": "默认路基模板",
    "displayScale": 10,
    "roadGrade": "UrbanArterial",
    "componentSource": "DefaultByRoadGrade",
    "componentOperations": [
      {
        "action": "AddComponent",
        "side": "Right",
        "type": "TravelLane",
        "position": "OutermostMotorLane",
        "width": null
      }
    ]
  }
}
```
```

- [ ] **Step 2: 更新工具协议**

在 `docs/agent/tool_protocol.md` 的请求示例和字段说明中增加：

```json
"componentOperations": [
  {
    "action": "AddComponent",
    "side": "Right",
    "type": "TravelLane",
    "position": "OutermostMotorLane",
    "width": null
  }
]
```

记录首版只支持 `AddComponent` 和 `OutermostMotorLane`。

- [ ] **Step 3: 更新 skill 写作规则**

在 `docs/agent/skill_authoring_rules.md` 中补充 tool skill 必须包含：

- Tool 身份。
- 别名归一化。
- 默认值规则。
- 局部操作规则。
- 示例用户表达到结构化计划映射。

- [ ] **Step 4: 提交**

```powershell
git add docs\agent\skills\cross_section\subgrade_template_create.md docs\agent\tool_protocol.md docs\agent\skill_authoring_rules.md
git commit -m "docs: define subgrade template tool skill rules"
```

---

### Task 8: 架构、业务和测试文档同步

**Files:**
- Modify: `docs/architecture/agent_code_structure.md`
- Modify: `docs/agent/overview.md`
- Modify: `docs/business/agent/设计软件原型Agent.md`
- Modify: `docs/modules/agent.md`
- Modify: `docs/reuse/agent_tool_gateway.md`
- Modify: `tests/README.md`
- Modify: `docs/dev/version_log.md`

- [ ] **Step 1: 更新架构文档**

在 `docs/architecture/agent_code_structure.md` 中把“本地规则 planner”改为 “Agent Planner”，说明：

- `AgentPlanner` 是后端工具规划总入口。
- 具体工具 planner 位于 `src/agent/RoadProto.Agent.Host/Tools/`。
- tool skill 是规划契约和模型提示资料。
- 可执行规则必须通过本地校验后才能生成 ToolCall。

- [ ] **Step 2: 更新业务文档**

在 `docs/business/agent/设计软件原型Agent.md` 中说明：

- 用户可使用“市政道路”“城市道路”等自然表达创建路基模板。
- 用户没有给参数时使用默认值。
- 用户说“默认基础上右侧增加行车道”时，Agent Planner 生成 `componentOperations`，确认后由 C++ 在默认模板基础上应用。

- [ ] **Step 3: 更新测试文档和版本记录**

在 `tests/README.md` 增加 Agent Planner 测试覆盖说明。
在 `docs/dev/version_log.md` 增加未发布条目：

```markdown
## Unreleased

- Agent Planner V1：将路基模板工具规划从单一硬编码 planner 升级为 `AgentPlanner` 总入口，支持市政道路默认城市主干路、缺省参数补齐和默认基础上右侧新增行车道的 `componentOperations`。
```

- [ ] **Step 4: 提交**

```powershell
git add docs\architecture\agent_code_structure.md docs\agent\overview.md docs\business\agent\设计软件原型Agent.md docs\modules\agent.md docs\reuse\agent_tool_gateway.md tests\README.md docs\dev\version_log.md
git commit -m "docs: document agent planner v1"
```

---

### Task 9: 验证、构建和主目录同步

**Files:**
- Verify: `src/agent/RoadProto.Agent.Tests/RoadProto.Agent.Tests.csproj`
- Verify: `tests/RoadProtoCoreTests.vcxproj`
- Verify: `src/ui/wpf/RoadProto.Terrain.UI/RoadProto.Terrain.UI.csproj`
- Sync to main: `F:\0_GPT_道路设计原型功能项目`

- [ ] **Step 1: 运行后端测试**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
```

Expected: PASS，包含 Agent Planner、chat API、配置、skill/知识库相关测试。

- [ ] **Step 2: 构建并运行核心测试**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: 构建成功，测试程序退出码为 0。

- [ ] **Step 3: 构建 Agent Host**

Run:

```powershell
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
```

Expected: Build succeeded。

- [ ] **Step 4: 构建 WPF 插件**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: Build succeeded。

- [ ] **Step 5: 手工 API 烟测**

启动 worktree 下 Agent Host 后调用：

```powershell
$body = @{
  message = '我想创建一个市政道路的路基模板，并基于默认参数上，最右侧增加一个行车道部件'
  modelProfile = $null
  history = @()
} | ConvertTo-Json -Depth 20
Invoke-RestMethod -Uri 'http://127.0.0.1:17831/api/chat' -Method Post -ContentType 'application/json; charset=utf-8' -Body $body
```

Expected:

- `requiresConfirmation = true`
- `toolCall.tool = cross_section.subgrade_template.create`
- `toolCall.arguments.roadGrade = UrbanArterial`
- `toolCall.arguments.componentOperations[0].type = TravelLane`
- `toolCall.arguments.componentOperations[0].side = Right`

- [ ] **Step 6: 同步主项目目录**

按 AGENTS.md 的 Worktree 主目录同步规则，把正式代码和文档同步回：

```text
F:\0_GPT_道路设计原型功能项目
```

不同步 `.git/`、`.vs/`、`.worktrees/`、`bin/`、`obj/`、`.roadproto-agent/` 和临时缓存。若本次没有生成新的 ARX/WPF 构建产物，不复制 `artifacts/`。

- [ ] **Step 7: 最终提交**

```powershell
git status --short
git log --oneline -5
```

Expected: worktree 没有未提交源码或文档改动。若同步主目录后主目录出现对应可见副本改动，明确告知用户这是主目录同步副本。

---

## 自查

- 覆盖用户目标：默认创建、市政道路默认、缺参数默认补齐、右侧新增行车道均有任务和测试。
- 分层边界：后端不依赖 ObjectARX；C++ application 解析和映射协议；ObjectARX 网关继续白名单执行；WPF 只确认和写请求文件。
- 安全边界：模型和 skill 不直接执行 CAD；ToolCall 仍需 WPF 确认和 C++ 校验。
- 范围控制：首版 `componentOperations` 只实现 `AddComponent + OutermostMotorLane`，不一次性扩展删除、改宽、左右对称等操作。
