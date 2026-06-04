# RoadProto Agent Prototype Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 RoadProto 中搭建可维护的本地设计软件原型 Agent 框架，并实现自动化创建路基模板的首个原子函数。

**Architecture:** 采用 `.NET 8` 本地 Agent sidecar、AutoCAD WPF `PaletteSet` 右侧面板、C++ ObjectARX 白名单工具网关三层架构。Agent 后端负责模型 Provider、skill 文档读取、意图分析和工具 JSON 规划；WPF 只做交互和确认；C++ 只执行固定 schema 的白名单工具，并复用现有 `SubgradeTemplateCreateService`、`SubgradeTemplateDefaults` 和 `SubgradeTemplateRules`。

**Tech Stack:** C++17、ObjectARX 2021、AutoCAD .NET API、WPF .NET Framework 4.8、.NET 8 Minimal API、OpenAI-compatible HTTP API、Windows PowerShell UTF-8。

---

## 文件结构总览

### 新增文档

- Create: `docs/agent/overview.md`：说明 RoadProto Agent 总体架构、边界和首版范围。
- Create: `docs/agent/tool_protocol.md`：定义工具请求 JSON、结果 JSON、确认规则和错误码。
- Create: `docs/agent/skill_authoring_rules.md`：定义 RoadProto Agent skill 写法。
- Create: `docs/agent/skills/cross_section/subgrade_template_create.md`：首个路基模板创建 skill。
- Create: `docs/business/agent/设计软件原型Agent.md`：Agent 用户命令业务文档。
- Create: `docs/reuse/agent_tool_gateway.md`：C++ 工具网关复用说明。
- Modify: `docs/modules/module_index.md`：加入 `AI_AGENT` 模块。
- Modify: `docs/dev/version_log.md`：记录本次 Agent 原型版本。
- Modify: `README.md`：加入 Agent 首版入口和运行说明。

### 新增 C++ application 层

- Create: `src/application/agent/AgentJsonValue.h`
- Create: `src/application/agent/AgentJsonValue.cpp`
- Create: `src/application/agent/AgentToolRequest.h`
- Create: `src/application/agent/AgentToolRequest.cpp`
- Create: `src/application/agent/SubgradeTemplateToolMapper.h`
- Create: `src/application/agent/SubgradeTemplateToolMapper.cpp`

职责：解析受控工具 JSON，把 `cross_section.subgrade_template.create` 参数转换为不依赖 ObjectARX 的 `SubgradeTemplateData` 和插入点模型。

### 新增 C++ module 与 cad_adapter 层

- Create: `src/modules/agent/AgentModule.h`
- Create: `src/modules/agent/AgentModule.cpp`
- Create: `src/app/startup/AgentStartupRegistration.h`
- Create: `src/app/startup/AgentStartupRegistration.cpp`
- Modify: `src/app/startup/Startup.cpp`
- Create: `src/cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.h`
- Create: `src/cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/RoadProtoCoreTests.vcxproj`

职责：注册 `AI_AGENT` 模块和 `RD_AI_EXECUTE_TOOL_FILE` 命令；命令读取 JSON 文件、调用 application 层转换、必要时提示用户点取插入点、创建 `DnSubgradeTemplateEntity`。

### 新增 Agent 后端

- Create: `src/agent/RoadProto.Agent.Host/RoadProto.Agent.Host.csproj`
- Create: `src/agent/RoadProto.Agent.Host/Program.cs`
- Create: `src/agent/RoadProto.Agent.Host/appsettings.example.json`
- Create: `src/agent/RoadProto.Agent.Host/Models/AgentDtos.cs`
- Create: `src/agent/RoadProto.Agent.Host/Skills/MarkdownSkillRepository.cs`
- Create: `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateToolPlanner.cs`
- Create: `src/agent/RoadProto.Agent.Host/Providers/ModelProviderOptions.cs`
- Create: `src/agent/RoadProto.Agent.Host/Providers/OpenAiCompatibleChatClient.cs`
- Create: `src/agent/RoadProto.Agent.Host/Services/AgentChatService.cs`
- Create: `src/agent/RoadProto.Agent.Tests/RoadProto.Agent.Tests.csproj`
- Create: `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs`
- Modify: `RoadProto.sln`

职责：提供本地 HTTP API，读取 Agent skill，支持 OpenAI-compatible Provider 配置，并把用户自然语言规划为工具调用或普通回答。

### 修改 WPF 托管插件

- Create: `src/ui/wpf/RoadProto.Terrain.UI/AgentAssistantControl.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AgentAssistantControl.xaml.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AgentAssistantCommands.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AgentToolExecutionFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/AgentDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Services/AgentBackendClient.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadProto.Terrain.UI.csproj`

职责：新增右侧 AI 面板，调用本地后端，展示确认卡片，并把工具 JSON 交给 `RD_AI_EXECUTE_TOOL_FILE`。

---

## Task 1: 文档基线与 Agent skill

**Files:**
- Create: `docs/agent/overview.md`
- Create: `docs/agent/tool_protocol.md`
- Create: `docs/agent/skill_authoring_rules.md`
- Create: `docs/agent/skills/cross_section/subgrade_template_create.md`
- Create: `docs/business/agent/设计软件原型Agent.md`
- Create: `docs/reuse/agent_tool_gateway.md`
- Modify: `docs/modules/module_index.md`
- Modify: `README.md`
- Modify: `docs/dev/version_log.md`

- [ ] **Step 1: 新增 Agent 总览文档**

Create `docs/agent/overview.md` with this content:

```markdown
# RoadProto Agent 总览

## 目标

RoadProto Agent 是 CAD 内嵌设计软件原型助手。首版提供右侧 WPF 对话面板、本地 Agent 后端、模型 Provider 配置、Agent skill 文档读取、受控工具调用和首个自动化工具：创建路基模板。

## 边界

- WPF 只负责输入、展示、确认和结果反馈。
- Agent 后端不依赖 ObjectARX，不读写 CAD 对象。
- C++ 工具网关只执行白名单工具。
- domain/application 继续承载业务规则。
- cad_adapter/objectarx 继续承载 AutoCAD API 调用。

## 首版命令

- `RD_AI_ASSISTANT_OPEN`：打开右侧 Agent 面板。
- `RD_AI_EXECUTE_TOOL_FILE`：执行 Agent 工具 JSON 文件。

## 首版工具

- `cross_section.subgrade_template.create`：自动化创建 `DnSubgradeTemplateEntity` 路基模板实体。

## 不做范围

- 不执行任意 AutoCAD 命令字符串。
- 不把 API Key 写入仓库。
- 不让 Agent 凭空绑定路面结构层模板 handle。
- 不在 WPF 中写 CAD 核心业务逻辑。
```

- [ ] **Step 2: 新增工具协议文档**

Create `docs/agent/tool_protocol.md`:

```markdown
# Agent 工具协议

## 请求文件

工具请求文件使用 UTF-8 JSON。顶层结构：

```json
{
  "tool": "cross_section.subgrade_template.create",
  "requestId": "uuid",
  "arguments": {},
  "resultPath": "%TEMP%/RoadProtoAgentToolResult_<pid>_<requestId>.json"
}
```

## 结果文件

成功：

```json
{
  "requestId": "uuid",
  "tool": "cross_section.subgrade_template.create",
  "succeeded": true,
  "entityHandle": "AB12",
  "entityType": "DnSubgradeTemplateEntity",
  "message": "已创建路基模板实体。"
}
```

失败：

```json
{
  "requestId": "uuid",
  "tool": "cross_section.subgrade_template.create",
  "succeeded": false,
  "errorCode": "InvalidDisplayScale",
  "message": "路基模板显示比例必须为 1、10、20、50 或 100。"
}
```

## 安全规则

- C++ 只接受白名单工具名。
- WPF 必须在执行前展示确认卡片。
- `RD_AI_EXECUTE_TOOL_FILE` 只读取文件，不接收模型生成的任意命令。
- 失败时必须返回 `errorCode` 和用户可读 `message`。
```

- [ ] **Step 3: 新增 Agent skill 写作规则**

Create `docs/agent/skill_authoring_rules.md`:

```markdown
# Agent Skill 写作规则

## 用途

RoadProto Agent skill 是给本地 Agent 后端读取的 markdown 能力说明。它帮助模型判断何时调用工具、如何补参数、什么时候追问、什么时候拒绝自动执行。

## 文件位置

工具 skill 放在：

```text
docs/agent/skills/<module>/<tool_name>.md
```

## 必备内容

每份 skill 必须包含：

- 工具 ID。
- 所属模块。
- 对应业务文档。
- 触发语义。
- 必要参数。
- 可选参数。
- 默认值规则。
- 缺参追问规则。
- 执行前确认文案。
- 禁止事项。
- 示例用户表达。
- 示例工具 JSON。

## 写作原则

- 用明确规则，不写开放式猜测。
- 缺少工程参数时优先追问。
- 只能引用已存在或用户明确提供的 CAD handle。
- 不允许凭空绑定实体。
```

- [ ] **Step 4: 新增路基模板创建 skill**

Create `docs/agent/skills/cross_section/subgrade_template_create.md`:

```markdown
# 创建路基模板 Agent Skill

## 工具

- ID：`cross_section.subgrade_template.create`
- 模块：`CROSS_SECTION`
- 业务文档：`docs/business/cross_section/路基模板_创建.md`
- 风险：创建 CAD 自定义实体

## 何时触发

用户要求创建、新建、生成路基模板时触发。例如：

- 创建一个二级公路路基模板。
- 帮我生成城市主干道路基模板，比例 1:20。
- 新建一个左右各两条 3.5 米行车道的路基模板。

## 何时不要触发

- 用户只是询问路基模板是什么。
- 用户询问如何手动创建路基模板。
- 用户要求创建边坡模板、道路模型、路面结构层模板或工程量表。

## 参数规则

- `templateName` 缺省为 `默认路基模板`。
- `displayScale` 缺省为 `10`，只允许 `1`、`10`、`20`、`50`、`100`。
- `roadGrade` 缺省为 `Expressway`。
- `insertionPoint.mode` 缺省为 `PickInCad`。
- 用户未明确给出部件列表时，使用 `DefaultByRoadGrade`。
- 用户明确给出完整部件时，使用 `ExplicitComponents`。
- 用户只给出部分部件且无法判断左右侧、宽度或类型时，先追问。
- 不允许凭空生成 `pavementLayer.handle`。

## 执行前确认

确认卡片必须展示：

- 模板名称。
- 道路等级。
- 显示比例。
- 部件来源。
- 插入点方式。
- 需要创建 `DnSubgradeTemplateEntity`。

## 示例工具 JSON

```json
{
  "tool": "cross_section.subgrade_template.create",
  "requestId": "agent-demo-001",
  "arguments": {
    "templateName": "K1 路基模板",
    "displayScale": 20,
    "roadGrade": "SecondClass",
    "roadCenterlineHandle": "",
    "insertionPoint": {
      "mode": "PickInCad",
      "x": null,
      "y": null,
      "z": 0
    },
    "componentSource": "DefaultByRoadGrade",
    "components": []
  },
  "resultPath": ""
}
```
```

- [ ] **Step 5: 新增业务和复用文档**

Create `docs/business/agent/设计软件原型Agent.md`:

```markdown
# 设计软件原型 Agent

## 功能范围

本功能提供 RoadProto 内嵌 AI 对话入口。用户可通过右侧 Agent 面板进行问答、软件操作咨询和受控工具调用。首版支持自动化创建路基模板。

## 命令

- `RD_AI_ASSISTANT_OPEN`：打开右侧 Agent 面板。
- `RD_AI_EXECUTE_TOOL_FILE`：执行 Agent 工具 JSON 文件。

## 边界

- Agent 面板不直接读写 CAD 实体。
- 后端服务不依赖 ObjectARX。
- C++ 只执行白名单工具。
- 首版工具只创建 `DnSubgradeTemplateEntity`。

## 首版流程

1. 用户运行 `RD_AI_ASSISTANT_OPEN`。
2. 系统打开右侧 WPF Agent 面板。
3. 用户输入创建路基模板需求。
4. 后端读取 skill 文档并规划工具调用。
5. WPF 展示确认卡片。
6. 用户确认后，C++ 执行 `cross_section.subgrade_template.create`。
7. 系统创建路基模板实体并返回 handle。
```

Create `docs/reuse/agent_tool_gateway.md`:

```markdown
# Agent 工具网关复用说明

## 能力

Agent 工具网关提供从 Agent 工具 JSON 到 C++ application/cad_adapter 的受控执行能力。

## 复用边界

- 工具请求解析放在 `src/application/agent`。
- AutoCAD 选择、点取、实体创建放在 `src/cad_adapter/objectarx/agent`。
- 新工具必须注册到白名单。
- 新工具必须有业务文档和 Agent skill 文档。

## 新增工具流程

1. 编写 Agent skill。
2. 定义工具 JSON schema。
3. 在 application 层增加参数转换和校验。
4. 在 cad_adapter 层增加执行器。
5. 在 `AI_AGENT` 模块注册命令或工具。
6. 增加核心测试和 AutoCAD 手工验证。
```

- [ ] **Step 6: 更新索引文档**

Modify `docs/modules/module_index.md` by adding this row to the table:

```markdown
| 设计软件原型 Agent | `AI_AGENT` | `RD_AI_` | 右侧 Agent 面板、工具调用网关和首个自动化路基模板工具规划实现 | `docs/business/agent/设计软件原型Agent.md` |
```

Modify `README.md` in the command list section by adding:

```markdown
- 设计软件原型 Agent：命令 `RD_AI_ASSISTANT_OPEN`
  - 打开 AutoCAD 右侧 WPF Agent 面板。
  - 首版支持通过受控工具调用自动创建路基模板。
  - 业务文档：`docs/business/agent/设计软件原型Agent.md`
```

Modify `docs/dev/version_log.md` by adding a new top entry:

```markdown
## v0.1.32_20260604_AgentPrototype

- 新增设计软件原型 Agent 设计与实现入口。
- 新增右侧 WPF Agent 面板规划、`.NET 8` 本地 Agent sidecar、C++ 白名单工具网关和 RoadProto Agent skill 文档体系。
- 首个自动化原子函数为 `cross_section.subgrade_template.create`，用于自动创建 `DnSubgradeTemplateEntity` 路基模板实体。
- 是否可作为稳定测试版本：否。该版本为 Agent 原型开发版本，需要完成 ARX、WPF 和后端联调后再标记稳定。
```

- [ ] **Step 7: 提交文档基线**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add docs README.md
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "docs: add agent architecture and skill docs"
```

Expected: commit succeeds with only docs changes.

---

## Task 2: C++ application 层工具请求模型与 JSON 解析

**Files:**
- Create: `src/application/agent/AgentJsonValue.h`
- Create: `src/application/agent/AgentJsonValue.cpp`
- Create: `src/application/agent/AgentToolRequest.h`
- Create: `src/application/agent/AgentToolRequest.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: 写失败测试，覆盖 JSON 解析和工具名校验**

Modify `tests/core_tests.cpp` near other helper tests by adding:

```cpp
#include "application/agent/AgentToolRequest.h"
```

Add this test function before `main()`:

```cpp
void agentToolRequestParsesSubgradeTemplateCreateJson()
{
    using roadproto::application::agent::parseAgentToolRequestJson;

    const std::string json =
        "{"
        "\"tool\":\"cross_section.subgrade_template.create\","
        "\"requestId\":\"agent-001\","
        "\"resultPath\":\"C:/Temp/result.json\","
        "\"arguments\":{"
        "\"templateName\":\"K1\","
        "\"displayScale\":20,"
        "\"roadGrade\":\"SecondClass\","
        "\"insertionPoint\":{\"mode\":\"Explicit\",\"x\":10,\"y\":20,\"z\":0},"
        "\"componentSource\":\"DefaultByRoadGrade\","
        "\"components\":[]"
        "}"
        "}";

    std::wstring error;
    const auto request = parseAgentToolRequestJson(json, error);
    CHECK(error.empty());
    CHECK(request.succeeded);
    CHECK(request.tool == L"cross_section.subgrade_template.create");
    CHECK(request.requestId == L"agent-001");
    CHECK(request.resultPath == L"C:/Temp/result.json");
    CHECK(request.arguments.templateName == L"K1");
    CHECK(std::fabs(request.arguments.displayScale - 20.0) < 1.0e-9);
    CHECK(request.arguments.roadGrade == L"SecondClass");
    CHECK(request.arguments.insertionPoint.mode == L"Explicit");
    CHECK(std::fabs(request.arguments.insertionPoint.x - 10.0) < 1.0e-9);
    CHECK(std::fabs(request.arguments.insertionPoint.y - 20.0) < 1.0e-9);
}

void agentToolRequestRejectsUnknownTool()
{
    using roadproto::application::agent::parseAgentToolRequestJson;

    const std::string json =
        "{"
        "\"tool\":\"acad.command.free_text\","
        "\"requestId\":\"agent-002\","
        "\"arguments\":{}"
        "}";

    std::wstring error;
    const auto request = parseAgentToolRequestJson(json, error);
    CHECK(!request.succeeded);
    CHECK(error.find(L"Unsupported agent tool") != std::wstring::npos);
}
```

Call both functions in `main()`:

```cpp
agentToolRequestParsesSubgradeTemplateCreateJson();
agentToolRequestRejectsUnknownTool();
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `application/agent/AgentToolRequest.h` does not exist.

- [ ] **Step 3: 新增 JSON value 类型**

Create `src/application/agent/AgentJsonValue.h`:

```cpp
#pragma once

#include <map>
#include <string>
#include <vector>

namespace roadproto::application::agent {

class AgentJsonValue {
public:
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Object,
        Array
    };

    Type type = Type::Null;
    bool booleanValue = false;
    double numberValue = 0.0;
    std::wstring stringValue;
    std::map<std::wstring, AgentJsonValue> objectValue;
    std::vector<AgentJsonValue> arrayValue;

    bool isObject() const;
    bool isArray() const;
    const AgentJsonValue* find(const std::wstring& key) const;
};

bool parseAgentJson(const std::string& text, AgentJsonValue& value, std::wstring& errorMessage);

} // namespace roadproto::application::agent
```

Create `src/application/agent/AgentJsonValue.cpp` with a small deterministic parser. It must support object, array, string, number, boolean and null. Use `std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>` for UTF-8 strings. Include these public methods exactly:

```cpp
#include "application/agent/AgentJsonValue.h"

#include <codecvt>
#include <cwctype>
#include <locale>
#include <cstdlib>
#include <sstream>

namespace roadproto::application::agent {

bool AgentJsonValue::isObject() const
{
    return type == Type::Object;
}

bool AgentJsonValue::isArray() const
{
    return type == Type::Array;
}

const AgentJsonValue* AgentJsonValue::find(const std::wstring& key) const
{
    if (type != Type::Object) {
        return nullptr;
    }
    const auto item = objectValue.find(key);
    return item == objectValue.end() ? nullptr : &item->second;
}

namespace {

class Parser {
public:
    explicit Parser(std::string text)
        : text_(std::move(text))
    {
    }

    bool parse(AgentJsonValue& value, std::wstring& error)
    {
        skipWhitespace();
        if (!parseValue(value, error)) {
            return false;
        }
        skipWhitespace();
        if (position_ != text_.size()) {
            error = L"Unexpected trailing JSON content.";
            return false;
        }
        return true;
    }

private:
    bool parseValue(AgentJsonValue& value, std::wstring& error);
    bool parseObject(AgentJsonValue& value, std::wstring& error);
    bool parseArray(AgentJsonValue& value, std::wstring& error);
    bool parseString(std::wstring& value, std::wstring& error);
    bool parseNumber(AgentJsonValue& value, std::wstring& error);
    bool consume(char expected);
    void skipWhitespace();

    std::string text_;
    std::size_t position_ = 0;
};

}

bool parseAgentJson(const std::string& text, AgentJsonValue& value, std::wstring& errorMessage)
{
    Parser parser(text);
    return parser.parse(value, errorMessage);
}

} // namespace roadproto::application::agent
```

Implement the private parser methods in the same file with this code:

```cpp
bool Parser::parseValue(AgentJsonValue& value, std::wstring& error)
{
    skipWhitespace();
    if (position_ >= text_.size()) {
        error = L"Unexpected end of JSON.";
        return false;
    }

    const auto ch = text_[position_];
    if (ch == '{') {
        return parseObject(value, error);
    }
    if (ch == '[') {
        return parseArray(value, error);
    }
    if (ch == '"') {
        value.type = AgentJsonValue::Type::String;
        return parseString(value.stringValue, error);
    }
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        return parseNumber(value, error);
    }
    if (text_.compare(position_, 4, "true") == 0) {
        position_ += 4;
        value.type = AgentJsonValue::Type::Boolean;
        value.booleanValue = true;
        return true;
    }
    if (text_.compare(position_, 5, "false") == 0) {
        position_ += 5;
        value.type = AgentJsonValue::Type::Boolean;
        value.booleanValue = false;
        return true;
    }
    if (text_.compare(position_, 4, "null") == 0) {
        position_ += 4;
        value.type = AgentJsonValue::Type::Null;
        return true;
    }

    error = L"Unexpected JSON token.";
    return false;
}

bool Parser::parseObject(AgentJsonValue& value, std::wstring& error)
{
    if (!consume('{')) {
        error = L"Expected JSON object.";
        return false;
    }

    value.type = AgentJsonValue::Type::Object;
    skipWhitespace();
    if (consume('}')) {
        return true;
    }

    while (position_ < text_.size()) {
        std::wstring key;
        if (!parseString(key, error)) {
            return false;
        }
        skipWhitespace();
        if (!consume(':')) {
            error = L"Expected ':' after JSON object key.";
            return false;
        }

        AgentJsonValue child;
        if (!parseValue(child, error)) {
            return false;
        }
        value.objectValue[key] = child;

        skipWhitespace();
        if (consume('}')) {
            return true;
        }
        if (!consume(',')) {
            error = L"Expected ',' or '}' in JSON object.";
            return false;
        }
        skipWhitespace();
    }

    error = L"Unterminated JSON object.";
    return false;
}

bool Parser::parseArray(AgentJsonValue& value, std::wstring& error)
{
    if (!consume('[')) {
        error = L"Expected JSON array.";
        return false;
    }

    value.type = AgentJsonValue::Type::Array;
    skipWhitespace();
    if (consume(']')) {
        return true;
    }

    while (position_ < text_.size()) {
        AgentJsonValue child;
        if (!parseValue(child, error)) {
            return false;
        }
        value.arrayValue.push_back(child);

        skipWhitespace();
        if (consume(']')) {
            return true;
        }
        if (!consume(',')) {
            error = L"Expected ',' or ']' in JSON array.";
            return false;
        }
        skipWhitespace();
    }

    error = L"Unterminated JSON array.";
    return false;
}

bool Parser::parseString(std::wstring& value, std::wstring& error)
{
    if (!consume('"')) {
        error = L"Expected JSON string.";
        return false;
    }

    std::string utf8;
    while (position_ < text_.size()) {
        const auto ch = text_[position_++];
        if (ch == '"') {
            try {
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                value = converter.from_bytes(utf8);
                return true;
            } catch (...) {
                error = L"Invalid UTF-8 in JSON string.";
                return false;
            }
        }
        if (ch == '\\') {
            if (position_ >= text_.size()) {
                error = L"Invalid JSON string escape.";
                return false;
            }
            const auto escaped = text_[position_++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                utf8.push_back(escaped);
                break;
            case 'b':
                utf8.push_back('\b');
                break;
            case 'f':
                utf8.push_back('\f');
                break;
            case 'n':
                utf8.push_back('\n');
                break;
            case 'r':
                utf8.push_back('\r');
                break;
            case 't':
                utf8.push_back('\t');
                break;
            default:
                error = L"Unsupported JSON string escape.";
                return false;
            }
            continue;
        }
        utf8.push_back(ch);
    }

    error = L"Unterminated JSON string.";
    return false;
}

bool Parser::parseNumber(AgentJsonValue& value, std::wstring& error)
{
    const auto start = position_;
    if (text_[position_] == '-') {
        ++position_;
    }
    while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
        ++position_;
    }
    if (position_ < text_.size() && text_[position_] == '.') {
        ++position_;
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
            ++position_;
        }
    }
    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
        ++position_;
        if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
            ++position_;
        }
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
            ++position_;
        }
    }

    const auto numberText = text_.substr(start, position_ - start);
    char* end = nullptr;
    const auto parsed = std::strtod(numberText.c_str(), &end);
    if (end == numberText.c_str() || *end != '\0') {
        error = L"Invalid JSON number.";
        return false;
    }

    value.type = AgentJsonValue::Type::Number;
    value.numberValue = parsed;
    return true;
}

bool Parser::consume(char expected)
{
    skipWhitespace();
    if (position_ < text_.size() && text_[position_] == expected) {
        ++position_;
        return true;
    }
    return false;
}

void Parser::skipWhitespace()
{
    while (position_ < text_.size()) {
        const auto ch = text_[position_];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            return;
        }
        ++position_;
    }
}
```

This parser intentionally does not support `\uXXXX` escapes in the first version. Agent-generated RoadProto tool files are written by .NET `System.Text.Json` with UTF-8 text, so Chinese strings are emitted directly and covered by the UTF-8 conversion path.

- [ ] **Step 4: 新增工具请求模型**

Create `src/application/agent/AgentToolRequest.h`:

```cpp
#pragma once

#include "application/agent/AgentJsonValue.h"

#include <string>
#include <vector>

namespace roadproto::application::agent {

struct AgentToolPoint {
    std::wstring mode = L"PickInCad";
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool hasX = false;
    bool hasY = false;
    bool hasZ = false;
};

struct AgentToolStationValue {
    double station = 0.0;
    double value = 0.0;
};

struct AgentToolColor {
    int r = -1;
    int g = -1;
    int b = -1;
};

struct AgentToolPavementLayer {
    bool linked = false;
    std::wstring handle;
    std::wstring name;
    double thickness = 0.0;
};

struct AgentToolSubgradeComponent {
    std::wstring side;
    std::wstring type;
    double width = 0.0;
    bool hasWidth = false;
    double height = 0.0;
    std::wstring slopeMode = L"Fixed";
    double fixedSlope = 0.0;
    AgentToolColor color;
    std::vector<AgentToolStationValue> wideningTable;
    std::vector<AgentToolStationValue> variableSlopeTable;
    AgentToolPavementLayer pavementLayer;
};

struct AgentSubgradeTemplateCreateArguments {
    std::wstring templateName = L"默认路基模板";
    double displayScale = 10.0;
    std::wstring roadGrade = L"Expressway";
    std::wstring roadCenterlineHandle;
    AgentToolPoint insertionPoint;
    std::wstring componentSource = L"DefaultByRoadGrade";
    std::vector<AgentToolSubgradeComponent> components;
};

struct AgentToolRequest {
    bool succeeded = false;
    std::wstring tool;
    std::wstring requestId;
    std::wstring resultPath;
    AgentSubgradeTemplateCreateArguments arguments;
};

AgentToolRequest parseAgentToolRequestJson(const std::string& text, std::wstring& errorMessage);

} // namespace roadproto::application::agent
```

Create `src/application/agent/AgentToolRequest.cpp`. Implement:

```cpp
#include "application/agent/AgentToolRequest.h"

#include <cmath>

namespace roadproto::application::agent {
namespace {

std::wstring stringOrDefault(const AgentJsonValue* value, const std::wstring& fallback)
{
    return value != nullptr && value->type == AgentJsonValue::Type::String
        ? value->stringValue
        : fallback;
}

double numberOrDefault(const AgentJsonValue* value, double fallback)
{
    return value != nullptr && value->type == AgentJsonValue::Type::Number
        ? value->numberValue
        : fallback;
}

bool boolOrDefault(const AgentJsonValue* value, bool fallback)
{
    return value != nullptr && value->type == AgentJsonValue::Type::Boolean
        ? value->booleanValue
        : fallback;
}

}

AgentToolRequest parseAgentToolRequestJson(const std::string& text, std::wstring& errorMessage)
{
    AgentJsonValue root;
    if (!parseAgentJson(text, root, errorMessage)) {
        return {};
    }
    if (!root.isObject()) {
        errorMessage = L"Agent tool request root must be an object.";
        return {};
    }

    AgentToolRequest request;
    request.tool = stringOrDefault(root.find(L"tool"), L"");
    if (request.tool != L"cross_section.subgrade_template.create") {
        errorMessage = L"Unsupported agent tool: " + request.tool;
        return {};
    }

    request.requestId = stringOrDefault(root.find(L"requestId"), L"");
    request.resultPath = stringOrDefault(root.find(L"resultPath"), L"");

    const auto* arguments = root.find(L"arguments");
    if (arguments == nullptr || !arguments->isObject()) {
        errorMessage = L"Agent tool request arguments must be an object.";
        return {};
    }

    request.arguments.templateName = stringOrDefault(arguments->find(L"templateName"), L"默认路基模板");
    request.arguments.displayScale = numberOrDefault(arguments->find(L"displayScale"), 10.0);
    request.arguments.roadGrade = stringOrDefault(arguments->find(L"roadGrade"), L"Expressway");
    request.arguments.roadCenterlineHandle = stringOrDefault(arguments->find(L"roadCenterlineHandle"), L"");
    request.arguments.componentSource = stringOrDefault(arguments->find(L"componentSource"), L"DefaultByRoadGrade");

    const auto* insertionPoint = arguments->find(L"insertionPoint");
    if (insertionPoint != nullptr && insertionPoint->isObject()) {
        request.arguments.insertionPoint.mode = stringOrDefault(insertionPoint->find(L"mode"), L"PickInCad");
        if (const auto* x = insertionPoint->find(L"x"); x != nullptr && x->type == AgentJsonValue::Type::Number) {
            request.arguments.insertionPoint.x = x->numberValue;
            request.arguments.insertionPoint.hasX = true;
        }
        if (const auto* y = insertionPoint->find(L"y"); y != nullptr && y->type == AgentJsonValue::Type::Number) {
            request.arguments.insertionPoint.y = y->numberValue;
            request.arguments.insertionPoint.hasY = true;
        }
        if (const auto* z = insertionPoint->find(L"z"); z != nullptr && z->type == AgentJsonValue::Type::Number) {
            request.arguments.insertionPoint.z = z->numberValue;
            request.arguments.insertionPoint.hasZ = true;
        }
    }

    const auto* components = arguments->find(L"components");
    if (components != nullptr && components->isArray()) {
        for (const auto& item : components->arrayValue) {
            if (!item.isObject()) {
                errorMessage = L"Agent subgrade component must be an object.";
                return {};
            }
            AgentToolSubgradeComponent component;
            component.side = stringOrDefault(item.find(L"side"), L"Right");
            component.type = stringOrDefault(item.find(L"type"), L"TravelLane");
            if (const auto* width = item.find(L"width"); width != nullptr && width->type == AgentJsonValue::Type::Number) {
                component.width = width->numberValue;
                component.hasWidth = true;
            }
            component.height = numberOrDefault(item.find(L"height"), 0.0);
            component.slopeMode = stringOrDefault(item.find(L"slopeMode"), L"Fixed");
            component.fixedSlope = numberOrDefault(item.find(L"fixedSlope"), 0.0);
            request.arguments.components.push_back(component);
        }
    }

    request.succeeded = true;
    errorMessage.clear();
    return request;
}

} // namespace roadproto::application::agent
```

Extend this file in Task 3 for colors, station tables and pavement layer parsing.

- [ ] **Step 5: 加入 vcxproj**

Modify `src/app/RoadProtoArx.vcxproj` and `tests/RoadProtoCoreTests.vcxproj` by adding:

```xml
<ClCompile Include="..\src\application\agent\AgentJsonValue.cpp" />
<ClCompile Include="..\src\application\agent\AgentToolRequest.cpp" />
```

For `src/app/RoadProtoArx.vcxproj`, the relative paths are:

```xml
<ClCompile Include="..\application\agent\AgentJsonValue.cpp" />
<ClCompile Include="..\application\agent\AgentToolRequest.cpp" />
```

- [ ] **Step 6: 运行测试确认通过**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: build succeeds and `RoadProtoCoreTests.exe` reports zero failures.

- [ ] **Step 7: 提交**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/application/agent src/app/RoadProtoArx.vcxproj tests/RoadProtoCoreTests.vcxproj tests/core_tests.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add agent tool request parser"
```

---

## Task 3: C++ 路基模板工具参数映射

**Files:**
- Create: `src/application/agent/SubgradeTemplateToolMapper.h`
- Create: `src/application/agent/SubgradeTemplateToolMapper.cpp`
- Modify: `src/application/agent/AgentToolRequest.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: 写失败测试，覆盖默认道路等级和显式部件**

Add include:

```cpp
#include "application/agent/SubgradeTemplateToolMapper.h"
```

Add tests:

```cpp
void agentSubgradeToolUsesDefaultComponentsForRoadGrade()
{
    using roadproto::application::agent::AgentSubgradeTemplateCreateArguments;
    using roadproto::application::agent::buildSubgradeTemplateToolData;
    using roadproto::domain::cross_section::RoadGrade;

    AgentSubgradeTemplateCreateArguments arguments;
    arguments.templateName = L"K1 路基模板";
    arguments.displayScale = 20.0;
    arguments.roadGrade = L"SecondClass";
    arguments.componentSource = L"DefaultByRoadGrade";

    std::wstring error;
    const auto result = buildSubgradeTemplateToolData(arguments, error);
    CHECK(result.succeeded);
    CHECK(result.data.properties.name == L"K1 路基模板");
    CHECK(std::fabs(result.data.properties.displayScale - 20.0) < 1.0e-9);
    CHECK(result.data.properties.roadGrade == RoadGrade::SecondClass);
    CHECK(result.data.components.size() == 6);
}

void agentSubgradeToolMapsExplicitComponents()
{
    using roadproto::application::agent::AgentSubgradeTemplateCreateArguments;
    using roadproto::application::agent::AgentToolSubgradeComponent;
    using roadproto::application::agent::buildSubgradeTemplateToolData;
    using roadproto::domain::cross_section::SubgradeComponentType;
    using roadproto::domain::cross_section::SubgradeSide;

    AgentSubgradeTemplateCreateArguments arguments;
    arguments.templateName = L"显式模板";
    arguments.displayScale = 10.0;
    arguments.roadGrade = L"UrbanArterial";
    arguments.componentSource = L"ExplicitComponents";

    AgentToolSubgradeComponent leftLane;
    leftLane.side = L"Left";
    leftLane.type = L"TravelLane";
    leftLane.width = 3.5;
    leftLane.hasWidth = true;
    leftLane.fixedSlope = -0.02;
    arguments.components.push_back(leftLane);

    AgentToolSubgradeComponent rightLane = leftLane;
    rightLane.side = L"Right";
    arguments.components.push_back(rightLane);

    std::wstring error;
    const auto result = buildSubgradeTemplateToolData(arguments, error);
    CHECK(result.succeeded);
    CHECK(result.data.components.size() == 2);
    CHECK(result.data.components[0].side == SubgradeSide::Left);
    CHECK(result.data.components[0].type == SubgradeComponentType::TravelLane);
    CHECK(std::fabs(result.data.components[0].width - 3.5) < 1.0e-9);
    CHECK(std::fabs(result.data.components[0].fixedSlope + 0.02) < 1.0e-9);
    CHECK(result.data.components[1].side == SubgradeSide::Right);
}

void agentSubgradeToolRejectsInvalidExplicitWidth()
{
    using roadproto::application::agent::AgentSubgradeTemplateCreateArguments;
    using roadproto::application::agent::AgentToolSubgradeComponent;
    using roadproto::application::agent::buildSubgradeTemplateToolData;

    AgentSubgradeTemplateCreateArguments arguments;
    arguments.componentSource = L"ExplicitComponents";
    AgentToolSubgradeComponent lane;
    lane.side = L"Left";
    lane.type = L"TravelLane";
    lane.width = -3.5;
    lane.hasWidth = true;
    arguments.components.push_back(lane);

    std::wstring error;
    const auto result = buildSubgradeTemplateToolData(arguments, error);
    CHECK(!result.succeeded);
    CHECK(error.find(L"width") != std::wstring::npos);
}
```

Call these tests in `main()`.

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `SubgradeTemplateToolMapper.h` does not exist.

- [ ] **Step 3: 新增 mapper 头文件**

Create `src/application/agent/SubgradeTemplateToolMapper.h`:

```cpp
#pragma once

#include "application/agent/AgentToolRequest.h"
#include "domain/cross_section/SubgradeTemplateModel.h"

#include <string>

namespace roadproto::application::agent {

struct SubgradeTemplateToolDataResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::cross_section::SubgradeTemplateData data;
};

SubgradeTemplateToolDataResult buildSubgradeTemplateToolData(
    const AgentSubgradeTemplateCreateArguments& arguments,
    std::wstring& errorMessage);

} // namespace roadproto::application::agent
```

- [ ] **Step 4: 实现 mapper**

Create `src/application/agent/SubgradeTemplateToolMapper.cpp`:

```cpp
#include "application/agent/SubgradeTemplateToolMapper.h"

#include <cmath>

namespace roadproto::application::agent {
namespace {

using namespace roadproto::domain::cross_section;

bool isFinite(double value)
{
    return std::isfinite(value);
}

SubgradeTemplateRgbColor mapColorOrDefault(
    const AgentToolColor& color,
    SubgradeComponentType type)
{
    if (color.r >= 0 && color.g >= 0 && color.b >= 0) {
        return {
            std::max(0, std::min(255, color.r)),
            std::max(0, std::min(255, color.g)),
            std::max(0, std::min(255, color.b))};
    }
    return SubgradeTemplateDefaults::defaultColorFor(type);
}

SubgradeTemplateComponent mapComponent(const AgentToolSubgradeComponent& input)
{
    SubgradeTemplateComponent component;
    component.side = subgradeSideFromCode(input.side, SubgradeSide::Right);
    component.type = subgradeComponentTypeFromCode(input.type, SubgradeComponentType::TravelLane);
    component.width = input.hasWidth ? input.width : 0.0;
    component.height = input.height;
    component.slopeMode = subgradeSlopeModeFromCode(input.slopeMode, SubgradeSlopeMode::Fixed);
    component.fixedSlope = input.fixedSlope;
    component.color = mapColorOrDefault(input.color, component.type);
    for (const auto& row : input.wideningTable) {
        component.wideningTable.push_back({row.station, row.value});
    }
    for (const auto& row : input.variableSlopeTable) {
        component.variableSlopeTable.push_back({row.station, row.value});
    }
    component.pavementLayerLinked = input.pavementLayer.linked;
    component.pavementLayerHandle = input.pavementLayer.handle;
    component.pavementLayerName = input.pavementLayer.name;
    component.pavementLayerThickness = input.pavementLayer.thickness;
    return component;
}

}

SubgradeTemplateToolDataResult buildSubgradeTemplateToolData(
    const AgentSubgradeTemplateCreateArguments& arguments,
    std::wstring& errorMessage)
{
    SubgradeTemplateToolDataResult result;
    const auto grade = roadGradeFromCode(arguments.roadGrade, RoadGrade::Expressway);

    if (arguments.componentSource == L"ExplicitComponents") {
        result.data.properties.roadGrade = grade;
        result.data.properties.name = arguments.templateName.empty() ? L"默认路基模板" : arguments.templateName;
        result.data.properties.displayScale = arguments.displayScale;
        result.data.roadCenterlineHandle = arguments.roadCenterlineHandle;
        for (const auto& component : arguments.components) {
            result.data.components.push_back(mapComponent(component));
        }
    } else {
        result.data = SubgradeTemplateDefaults::create(grade);
        result.data.properties.name = arguments.templateName.empty() ? L"默认路基模板" : arguments.templateName;
        result.data.properties.displayScale = arguments.displayScale;
        result.data.roadCenterlineHandle = arguments.roadCenterlineHandle;
    }

    if (!SubgradeTemplateRules::normalize(result.data, errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    result.succeeded = true;
    errorMessage.clear();
    return result;
}

} // namespace roadproto::application::agent
```

- [ ] **Step 5: 扩展 JSON 请求解析完整字段**

Modify `src/application/agent/AgentToolRequest.cpp` component parsing block to read:

```cpp
if (const auto* color = item.find(L"color"); color != nullptr && color->isObject()) {
    component.color.r = static_cast<int>(numberOrDefault(color->find(L"r"), -1));
    component.color.g = static_cast<int>(numberOrDefault(color->find(L"g"), -1));
    component.color.b = static_cast<int>(numberOrDefault(color->find(L"b"), -1));
}
if (const auto* pavementLayer = item.find(L"pavementLayer"); pavementLayer != nullptr && pavementLayer->isObject()) {
    component.pavementLayer.linked = boolOrDefault(pavementLayer->find(L"linked"), false);
    component.pavementLayer.handle = stringOrDefault(pavementLayer->find(L"handle"), L"");
    component.pavementLayer.name = stringOrDefault(pavementLayer->find(L"name"), L"");
    component.pavementLayer.thickness = numberOrDefault(pavementLayer->find(L"thickness"), 0.0);
}
```

Add helper function for station tables:

```cpp
std::vector<AgentToolStationValue> stationTableOrEmpty(const AgentJsonValue* value, std::wstring& errorMessage)
{
    std::vector<AgentToolStationValue> rows;
    if (value == nullptr) {
        return rows;
    }
    if (!value->isArray()) {
        errorMessage = L"Agent station table must be an array.";
        return rows;
    }
    for (const auto& item : value->arrayValue) {
        if (!item.isObject()) {
            errorMessage = L"Agent station table row must be an object.";
            return {};
        }
        rows.push_back({
            numberOrDefault(item.find(L"station"), 0.0),
            numberOrDefault(item.find(L"value"), 0.0)});
    }
    return rows;
}
```

Use it:

```cpp
component.wideningTable = stationTableOrEmpty(item.find(L"wideningTable"), errorMessage);
if (!errorMessage.empty()) {
    return {};
}
component.variableSlopeTable = stationTableOrEmpty(item.find(L"variableSlopeTable"), errorMessage);
if (!errorMessage.empty()) {
    return {};
}
```

- [ ] **Step 6: 加入 vcxproj 并运行测试**

Add compile entries:

For `src/app/RoadProtoArx.vcxproj`:

```xml
<ClCompile Include="..\application\agent\SubgradeTemplateToolMapper.cpp" />
```

For `tests/RoadProtoCoreTests.vcxproj`:

```xml
<ClCompile Include="..\src\application\agent\SubgradeTemplateToolMapper.cpp" />
```

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: zero failures.

- [ ] **Step 7: 提交**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/application/agent src/app/RoadProtoArx.vcxproj tests/RoadProtoCoreTests.vcxproj tests/core_tests.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: map agent subgrade template tool data"
```

---

## Task 4: C++ AI_AGENT 模块与工具网关命令

**Files:**
- Create: `src/modules/agent/AgentModule.h`
- Create: `src/modules/agent/AgentModule.cpp`
- Create: `src/app/startup/AgentStartupRegistration.h`
- Create: `src/app/startup/AgentStartupRegistration.cpp`
- Create: `src/cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.h`
- Create: `src/cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.cpp`
- Modify: `src/app/startup/Startup.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: 写失败测试，覆盖命令元数据**

Add include:

```cpp
#include "modules/agent/AgentModule.h"
```

Add test:

```cpp
void agentModuleRegistersToolGatewayCommand()
{
    roadproto::core::CommandRegistry commands;
    auto module = roadproto::modules::agent::createAgentModule();
    module.registerCommands(commands);

    const auto command = commands.find(L"RD_AI_EXECUTE_TOOL_FILE");
    CHECK(command != nullptr);
    CHECK(command->moduleCode == L"AI_AGENT");
    CHECK(command->businessDocPath == L"docs/business/agent/设计软件原型Agent.md");
    CHECK(command->ribbonAttachable == false);
}
```

Call it in `main()`.

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `modules/agent/AgentModule.h` does not exist.

- [ ] **Step 3: 新增 ObjectARX 命令头文件**

Create `src/cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.h`:

```cpp
#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::agent {

core::CommandProcedure agentExecuteToolFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::agent
```

- [ ] **Step 4: 新增 AI_AGENT 模块**

Create `src/modules/agent/AgentModule.h`:

```cpp
#pragma once

#include "core/module/ModuleRegistry.h"

namespace roadproto::modules::agent {

core::ModuleDefinition createAgentModule();

} // namespace roadproto::modules::agent
```

Create `src/modules/agent/AgentModule.cpp`:

```cpp
#include "modules/agent/AgentModule.h"

#include "cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::agent {
namespace {

void registerAgentCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_AI_EXECUTE_TOOL_FILE",
        L"执行 Agent 工具文件",
        L"AI_AGENT",
        L"Executes a whitelisted RoadProto Agent tool request file.",
        cad_adapter::objectarx::agent::agentExecuteToolFileCommandProcedure(),
        true,
        true,
        L"docs/business/agent/设计软件原型Agent.md",
        false});
}

void registerAgentRibbon(ui::RibbonModel&)
{
}

} // namespace

core::ModuleDefinition createAgentModule()
{
    return core::ModuleDefinition{
        L"Agent",
        L"AI_AGENT",
        L"RoadProto design software prototype Agent module.",
        []() { return true; },
        []() { return true; },
        &registerAgentCommands,
        &registerAgentRibbon,
        L"docs/business/agent/设计软件原型Agent.md"};
}

} // namespace roadproto::modules::agent
```

- [ ] **Step 5: 新增启动注册**

Create `src/app/startup/AgentStartupRegistration.h`:

```cpp
#pragma once

#include "core/module/ModuleRegistry.h"

namespace roadproto::app {

void registerAgentModuleForStartup(core::ModuleRegistry& moduleRegistry);

} // namespace roadproto::app
```

Create `src/app/startup/AgentStartupRegistration.cpp`:

```cpp
#include "app/startup/AgentStartupRegistration.h"

#include "modules/agent/AgentModule.h"

namespace roadproto::app {

void registerAgentModuleForStartup(core::ModuleRegistry& moduleRegistry)
{
    moduleRegistry.registerModule(modules::agent::createAgentModule());
}

} // namespace roadproto::app
```

Modify `src/app/startup/Startup.cpp`:

```cpp
#include "app/startup/AgentStartupRegistration.h"
```

In `registerBuiltInModules`, add after drawing quantity registration:

```cpp
registerAgentModuleForStartup(moduleRegistry);
```

- [ ] **Step 6: 新增 ObjectARX 工具命令测试桩与真实执行**

Create `src/cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.cpp`:

```cpp
#include "cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/agent/AgentToolRequest.h"
#include "application/agent/SubgradeTemplateToolMapper.h"
#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <fstream>
#include <sstream>
#endif

namespace roadproto::cad_adapter::objectarx::agent {
namespace {

#ifndef ROADPROTO_TEST_BUILD

std::string readBinaryFile(const std::wstring& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool promptInsertionPoint(AcGePoint3d& insertionPoint)
{
    ads_point raw;
    if (acedGetPoint(nullptr, L"\n请选择 Agent 路基模板插入位置: ", raw) != RTNORM) {
        return false;
    }
    insertionPoint = AcGePoint3d(raw[0], raw[1], raw[2]);
    return true;
}

bool appendEntityToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr) {
        return false;
    }
    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        return false;
    }
    AcDbBlockTableRecord* modelSpace = nullptr;
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForWrite);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return false;
    }
    const auto appendStatus = modelSpace->appendAcDbEntity(entityId, entity);
    modelSpace->close();
    return appendStatus == Acad::eOk;
}

std::wstring entityHandleText(AcDbEntity* entity)
{
    AcDbHandle handle;
    entity->getAcDbHandle(handle);
    ACHAR handleText[32] = {};
    handle.getIntoAsciiBuffer(handleText);
    return handleText;
}

void runAgentExecuteToolFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto Agent tool request file: ", pathBuffer) != RTNORM) {
        return;
    }

    std::wstring error;
    const auto request = application::agent::parseAgentToolRequestJson(readBinaryFile(pathBuffer), error);
    if (!request.succeeded) {
        editor.writeError(error.empty() ? L"Agent 工具请求解析失败。" : error);
        return;
    }

    const auto dataResult = application::agent::buildSubgradeTemplateToolData(request.arguments, error);
    if (!dataResult.succeeded) {
        editor.writeError(error.empty() ? L"Agent 路基模板参数无效。" : error);
        return;
    }

    AcGePoint3d insertionPoint(0.0, 0.0, 0.0);
    if (request.arguments.insertionPoint.mode == L"Explicit"
        && request.arguments.insertionPoint.hasX
        && request.arguments.insertionPoint.hasY) {
        insertionPoint = AcGePoint3d(
            request.arguments.insertionPoint.x,
            request.arguments.insertionPoint.y,
            request.arguments.insertionPoint.hasZ ? request.arguments.insertionPoint.z : 0.0);
    } else if (!promptInsertionPoint(insertionPoint)) {
        editor.writeWarning(L"Agent 路基模板创建已取消。");
        return;
    }

    auto* entity = new cross_section::DnSubgradeTemplateEntity();
    entity->setTemplateData(dataResult.data);
    entity->setInsertionPoint(insertionPoint);

    AcDbObjectId entityId;
    if (!appendEntityToModelSpace(entity, entityId)) {
        delete entity;
        editor.writeError(L"Agent 插入 DnSubgradeTemplateEntity 失败。");
        return;
    }

    const auto handle = entityHandleText(entity);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"Agent 已创建路基模板实体，handle: " + handle);
}

#else

void runAgentExecuteToolFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure agentExecuteToolFileCommandProcedure()
{
    return &runAgentExecuteToolFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::agent
```

After this compiles, add result file writing in Task 8 so WPF can display structured results.

- [ ] **Step 7: 加入 vcxproj**

Modify `src/app/RoadProtoArx.vcxproj`:

```xml
<ClCompile Include="startup\AgentStartupRegistration.cpp" />
<ClCompile Include="..\modules\agent\AgentModule.cpp" />
<ClCompile Include="..\cad_adapter\objectarx\agent\ObjectArxAgentToolCommand.cpp" />
```

Modify `tests/RoadProtoCoreTests.vcxproj`:

```xml
<ClCompile Include="..\src\app\startup\AgentStartupRegistration.cpp" />
<ClCompile Include="..\src\modules\agent\AgentModule.cpp" />
<ClCompile Include="..\src\cad_adapter\objectarx\agent\ObjectArxAgentToolCommand.cpp" />
```

- [ ] **Step 8: 运行核心测试**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: zero failures.

- [ ] **Step 9: 提交**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/modules/agent src/app/startup src/cad_adapter/objectarx/agent src/app/RoadProtoArx.vcxproj tests/RoadProtoCoreTests.vcxproj tests/core_tests.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add agent tool gateway command"
```

---

## Task 5: .NET 8 Agent 后端骨架

**Files:**
- Create: `src/agent/RoadProto.Agent.Host/RoadProto.Agent.Host.csproj`
- Create: `src/agent/RoadProto.Agent.Host/Program.cs`
- Create: `src/agent/RoadProto.Agent.Host/appsettings.example.json`
- Create: `src/agent/RoadProto.Agent.Host/Models/AgentDtos.cs`
- Create: `src/agent/RoadProto.Agent.Host/Skills/MarkdownSkillRepository.cs`
- Create: `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateToolPlanner.cs`
- Create: `src/agent/RoadProto.Agent.Tests/RoadProto.Agent.Tests.csproj`
- Create: `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs`
- Modify: `RoadProto.sln`

- [ ] **Step 1: 创建项目文件**

Create `src/agent/RoadProto.Agent.Host/RoadProto.Agent.Host.csproj`:

```xml
<Project Sdk="Microsoft.NET.Sdk.Web">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <AssemblyName>RoadProto.Agent.Host</AssemblyName>
    <RootNamespace>RoadProto.Agent.Host</RootNamespace>
    <OutputPath>$(MSBuildProjectDirectory)\..\..\..\artifacts\agent\$(Configuration)\</OutputPath>
  </PropertyGroup>
</Project>
```

Create `src/agent/RoadProto.Agent.Tests/RoadProto.Agent.Tests.csproj`:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <IsPackable>false</IsPackable>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="..\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj" />
    <PackageReference Include="Microsoft.NET.Test.Sdk" Version="17.10.0" />
    <PackageReference Include="xunit" Version="2.8.1" />
    <PackageReference Include="xunit.runner.visualstudio" Version="2.8.1" />
  </ItemGroup>
</Project>
```

Run:

```powershell
dotnet sln RoadProto.sln add src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj
dotnet sln RoadProto.sln add src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
```

Expected: both projects are added to `RoadProto.sln`.

- [ ] **Step 2: 新增 DTO**

Create `src/agent/RoadProto.Agent.Host/Models/AgentDtos.cs`:

```csharp
namespace RoadProto.Agent.Host.Models;

public sealed record AgentChatRequest(
    string Message,
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
```

- [ ] **Step 3: 新增 skill repository**

Create `src/agent/RoadProto.Agent.Host/Skills/MarkdownSkillRepository.cs`:

```csharp
namespace RoadProto.Agent.Host.Skills;

public sealed class MarkdownSkillRepository
{
    private readonly string repositoryRoot;

    public MarkdownSkillRepository(string repositoryRoot)
    {
        this.repositoryRoot = repositoryRoot;
    }

    public string ReadSubgradeTemplateCreateSkill()
    {
        var path = Path.Combine(
            repositoryRoot,
            "docs",
            "agent",
            "skills",
            "cross_section",
            "subgrade_template_create.md");
        return File.Exists(path) ? File.ReadAllText(path) : string.Empty;
    }
}
```

- [ ] **Step 4: 写后端 planner 测试**

Create `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs`:

```csharp
using RoadProto.Agent.Host.Tools;
using Xunit;

namespace RoadProto.Agent.Tests;

public sealed class SubgradeTemplateToolPlannerTests
{
    [Fact]
    public void PlansSecondClassSubgradeTemplate()
    {
        var planner = new SubgradeTemplateToolPlanner();

        var result = planner.TryPlan("帮我创建一个二级公路路基模板，比例1:20，名字叫K1路基模板");

        Assert.NotNull(result);
        Assert.Equal("cross_section.subgrade_template.create", result.Tool);
        var args = Assert.IsType<SubgradeTemplateCreateArguments>(result.Arguments);
        Assert.Equal("K1路基模板", args.TemplateName);
        Assert.Equal(20, args.DisplayScale);
        Assert.Equal("SecondClass", args.RoadGrade);
        Assert.Equal("DefaultByRoadGrade", args.ComponentSource);
        Assert.Empty(args.Components);
        Assert.Equal("PickInCad", args.InsertionPoint.Mode);
    }

    [Fact]
    public void DoesNotPlanForQuestionOnly()
    {
        var planner = new SubgradeTemplateToolPlanner();

        var result = planner.TryPlan("路基模板是什么？");

        Assert.Null(result);
    }
}
```

- [ ] **Step 5: 实现 planner**

Create `src/agent/RoadProto.Agent.Host/Tools/SubgradeTemplateToolPlanner.cs`:

```csharp
using System.Text.RegularExpressions;
using RoadProto.Agent.Host.Models;

namespace RoadProto.Agent.Host.Tools;

public sealed class SubgradeTemplateToolPlanner
{
    public AgentToolCall? TryPlan(string message)
    {
        if (!message.Contains("路基模板", StringComparison.Ordinal)) {
            return null;
        }
        if (message.Contains("是什么", StringComparison.Ordinal)
            || message.Contains("怎么", StringComparison.Ordinal)
            || message.Contains("如何", StringComparison.Ordinal)) {
            return null;
        }
        if (!message.Contains("创建", StringComparison.Ordinal)
            && !message.Contains("新建", StringComparison.Ordinal)
            && !message.Contains("生成", StringComparison.Ordinal)) {
            return null;
        }

        var roadGrade = DetectRoadGrade(message);
        var displayScale = DetectDisplayScale(message);
        var templateName = DetectTemplateName(message);

        var arguments = new SubgradeTemplateCreateArguments(
            templateName,
            displayScale,
            roadGrade,
            string.Empty,
            new AgentInsertionPoint("PickInCad", null, null, 0),
            "DefaultByRoadGrade",
            Array.Empty<SubgradeComponentArgument>());

        return new AgentToolCall(
            "cross_section.subgrade_template.create",
            Guid.NewGuid().ToString("N"),
            arguments,
            "创建路基模板",
            $"将创建 {templateName}，道路等级 {roadGrade}，显示比例 1:{displayScale}，确认后需要在 CAD 中点取插入点。");
    }

    private static string DetectRoadGrade(string message)
    {
        if (message.Contains("一级", StringComparison.Ordinal)) return "FirstClass";
        if (message.Contains("二级", StringComparison.Ordinal)) return "SecondClass";
        if (message.Contains("三级", StringComparison.Ordinal)) return "ThirdClass";
        if (message.Contains("四级", StringComparison.Ordinal)) return "FourthClass";
        if (message.Contains("城市快速", StringComparison.Ordinal)) return "UrbanExpressway";
        if (message.Contains("城市主干", StringComparison.Ordinal)) return "UrbanArterial";
        if (message.Contains("城市次干", StringComparison.Ordinal)) return "UrbanSubArterial";
        if (message.Contains("城市支路", StringComparison.Ordinal)) return "UrbanBranch";
        return "Expressway";
    }

    private static double DetectDisplayScale(string message)
    {
        var match = Regex.Match(message, @"1\s*[:：]\s*(1|10|20|50|100)");
        return match.Success ? double.Parse(match.Groups[1].Value) : 10;
    }

    private static string DetectTemplateName(string message)
    {
        var match = Regex.Match(message, @"名字叫(?<name>[\u4e00-\u9fa5A-Za-z0-9_ -]+)");
        return match.Success ? match.Groups["name"].Value.Trim() : "默认路基模板";
    }
}
```

- [ ] **Step 6: 新增 Minimal API**

Create `src/agent/RoadProto.Agent.Host/Program.cs`:

```csharp
using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Tools;

var builder = WebApplication.CreateBuilder(args);
builder.Services.AddSingleton<SubgradeTemplateToolPlanner>();

var app = builder.Build();

app.MapGet("/health", () => Results.Ok(new { status = "ok" }));

app.MapPost("/api/chat", (AgentChatRequest request, SubgradeTemplateToolPlanner planner) =>
{
    var toolCall = planner.TryPlan(request.Message);
    if (toolCall != null) {
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
```

Create `src/agent/RoadProto.Agent.Host/appsettings.example.json`:

```json
{
  "RoadProtoAgent": {
    "ModelProfiles": [
      {
        "Name": "openai",
        "Provider": "OpenAICompatible",
        "BaseUrl": "https://api.openai.com/v1",
        "ApiKeyEnvironmentVariable": "OPENAI_API_KEY",
        "Model": "gpt-4.1",
        "Temperature": 0.2,
        "TimeoutSeconds": 60,
        "EnableStreaming": false
      },
      {
        "Name": "deepseek",
        "Provider": "OpenAICompatible",
        "BaseUrl": "https://api.deepseek.com",
        "ApiKeyEnvironmentVariable": "DEEPSEEK_API_KEY",
        "Model": "deepseek-chat",
        "Temperature": 0.2,
        "TimeoutSeconds": 60,
        "EnableStreaming": false
      },
      {
        "Name": "dashscope-qwen",
        "Provider": "OpenAICompatible",
        "BaseUrl": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "ApiKeyEnvironmentVariable": "DASHSCOPE_API_KEY",
        "Model": "qwen-plus",
        "Temperature": 0.2,
        "TimeoutSeconds": 60,
        "EnableStreaming": false
      }
    ]
  }
}
```

- [ ] **Step 7: 运行后端测试和构建**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
```

Expected: tests pass, host builds to `artifacts\agent\Release\`.

- [ ] **Step 8: 提交**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add RoadProto.sln src/agent
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add local agent backend skeleton"
```

---

## Task 6: WPF 右侧 Agent Palette 面板

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AgentAssistantControl.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AgentAssistantControl.xaml.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AgentAssistantCommands.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AgentToolExecutionFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/AgentDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Services/AgentBackendClient.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadProto.Terrain.UI.csproj`

- [ ] **Step 1: 新增 DTO**

Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/AgentDtos.cs`:

```csharp
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
```

- [ ] **Step 2: 新增后端客户端**

Create `src/ui/wpf/RoadProto.Terrain.UI/Services/AgentBackendClient.cs`:

```csharp
using System.Net.Http;
using System.Net.Http.Json;
using System.Threading.Tasks;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI.Services;

public sealed class AgentBackendClient
{
    private readonly HttpClient client = new()
    {
        BaseAddress = new System.Uri("http://127.0.0.1:17831")
    };

    public async Task<bool> IsHealthyAsync()
    {
        try
        {
            using var response = await client.GetAsync("/health").ConfigureAwait(false);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<AgentChatResponse?> SendAsync(AgentChatRequest request)
    {
        using var response = await client.PostAsJsonAsync("/api/chat", request).ConfigureAwait(false);
        if (!response.IsSuccessStatusCode)
        {
            return new AgentChatResponse { Reply = $"Agent 后端返回错误：{response.StatusCode}" };
        }

        return await response.Content.ReadFromJsonAsync<AgentChatResponse>().ConfigureAwait(false);
    }
}
```

- [ ] **Step 3: 新增工具文件写入**

Create `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AgentToolExecutionFile.cs`:

```csharp
using System;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI.AutoCad;

public static class AgentToolExecutionFile
{
    public static string WriteToolRequest(AgentToolCall toolCall)
    {
        var requestPath = Path.Combine(
            Path.GetTempPath(),
            $"RoadProtoAgentToolRequest_{Process.GetCurrentProcess().Id}_{toolCall.RequestId}.json");
        var resultPath = Path.Combine(
            Path.GetTempPath(),
            $"RoadProtoAgentToolResult_{Process.GetCurrentProcess().Id}_{toolCall.RequestId}.json");

        var payload = new
        {
            tool = toolCall.Tool,
            requestId = toolCall.RequestId,
            arguments = toolCall.Arguments,
            resultPath
        };

        File.WriteAllText(
            requestPath,
            JsonSerializer.Serialize(payload, new JsonSerializerOptions { WriteIndented = true }));
        return requestPath;
    }
}
```

- [ ] **Step 4: 新增 WPF UserControl**

Create `src/ui/wpf/RoadProto.Terrain.UI/AgentAssistantControl.xaml`:

```xml
<UserControl x:Class="RoadProto.Terrain.UI.AgentAssistantControl"
             xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             MinWidth="340">
    <Grid Background="#15171C">
        <Grid.RowDefinitions>
            <RowDefinition Height="Auto" />
            <RowDefinition Height="*" />
            <RowDefinition Height="Auto" />
        </Grid.RowDefinitions>

        <Border Grid.Row="0" Padding="12" BorderBrush="#2F3542" BorderThickness="0,0,0,1">
            <StackPanel>
                <TextBlock Text="RoadProto Agent" Foreground="#F4F7FB" FontSize="18" FontWeight="SemiBold" />
                <TextBlock x:Name="StatusText" Text="连接本地 Agent 后端中" Foreground="#9AA4B2" Margin="0,4,0,0" />
            </StackPanel>
        </Border>

        <ScrollViewer Grid.Row="1" Padding="12">
            <StackPanel x:Name="MessagePanel" />
        </ScrollViewer>

        <Border Grid.Row="2" Padding="12" BorderBrush="#2F3542" BorderThickness="0,1,0,0">
            <Grid>
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="*" />
                    <ColumnDefinition Width="Auto" />
                </Grid.ColumnDefinitions>
                <TextBox x:Name="InputBox" MinHeight="42" TextWrapping="Wrap" AcceptsReturn="True" />
                <Button Grid.Column="1" Content="发送" Width="72" Margin="8,0,0,0" Click="SendButton_Click" />
            </Grid>
        </Border>
    </Grid>
</UserControl>
```

Create `src/ui/wpf/RoadProto.Terrain.UI/AgentAssistantControl.xaml.cs`:

```csharp
using System.Windows;
using System.Windows.Controls;
using RoadProto.Terrain.UI.AutoCad;
using RoadProto.Terrain.UI.Bridge;
using RoadProto.Terrain.UI.Services;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI;

public partial class AgentAssistantControl : UserControl
{
    private readonly AgentBackendClient client = new();
    private AgentToolCall? pendingToolCall;

    public AgentAssistantControl()
    {
        InitializeComponent();
        _ = RefreshHealthAsync();
    }

    private async System.Threading.Tasks.Task RefreshHealthAsync()
    {
        StatusText.Text = await client.IsHealthyAsync()
            ? "本地 Agent 后端已连接"
            : "本地 Agent 后端未启动";
    }

    private async void SendButton_Click(object sender, RoutedEventArgs e)
    {
        var text = InputBox.Text.Trim();
        if (string.IsNullOrWhiteSpace(text))
        {
            return;
        }

        InputBox.Clear();
        AddMessage("用户", text);
        var response = await client.SendAsync(new AgentChatRequest { Message = text });
        if (response == null)
        {
            AddMessage("Agent", "未收到后端响应。");
            return;
        }

        AddMessage("Agent", response.Reply);
        if (response.ToolCall != null && response.RequiresConfirmation)
        {
            pendingToolCall = response.ToolCall;
            AddToolConfirmation(response.ToolCall);
        }
    }

    private void AddMessage(string role, string content)
    {
        MessagePanel.Children.Add(new TextBlock
        {
            Text = $"{role}: {content}",
            Foreground = System.Windows.Media.Brushes.White,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 10)
        });
    }

    private void AddToolConfirmation(AgentToolCall toolCall)
    {
        var button = new Button
        {
            Content = $"确认执行：{toolCall.ConfirmationTitle}",
            Margin = new Thickness(0, 0, 0, 10)
        };
        button.Click += (_, _) => ExecutePendingTool();
        MessagePanel.Children.Add(new TextBlock
        {
            Text = toolCall.ConfirmationBody,
            Foreground = System.Windows.Media.Brushes.LightGray,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 6)
        });
        MessagePanel.Children.Add(button);
    }

    private void ExecutePendingTool()
    {
        if (pendingToolCall == null)
        {
            return;
        }

        var requestPath = AgentToolExecutionFile.WriteToolRequest(pendingToolCall).Replace('\\', '/');
        CoreApplication.DocumentManager.MdiActiveDocument?
            .SendStringToExecute($"RD_AI_EXECUTE_TOOL_FILE \"{requestPath}\"\n", true, false, true);
        AddMessage("Agent", "已提交工具执行请求，请按 CAD 命令行提示完成操作。");
        pendingToolCall = null;
    }
}
```

- [ ] **Step 5: 新增 AutoCAD Palette 命令**

Create `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AgentAssistantCommands.cs`:

```csharp
using Autodesk.AutoCAD.Runtime;
using Autodesk.AutoCAD.Windows;

[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.AgentAssistantCommands))]

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class AgentAssistantCommands
{
    private static PaletteSet? palette;

    [CommandMethod("RD_AI_ASSISTANT_OPEN", CommandFlags.Session)]
    public void OpenAgentAssistant()
    {
        if (palette == null)
        {
            palette = new PaletteSet("RoadProto Agent")
            {
                Style = PaletteSetStyles.ShowCloseButton
                    | PaletteSetStyles.ShowAutoHideButton
                    | PaletteSetStyles.ShowPropertiesMenu,
                DockEnabled = DockSides.Right | DockSides.Left,
                MinimumSize = new System.Drawing.Size(360, 480),
                Size = new System.Drawing.Size(420, 720)
            };
            palette.AddVisual("Agent", new AgentAssistantControl());
        }

        palette.Visible = true;
        palette.Dock = DockSides.Right;
    }
}
```

- [ ] **Step 6: 更新 Ribbon**

Modify `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`:

Add assembly command class near other assembly attributes:

```csharp
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.AgentAssistantCommands))]
```

Add constants:

```csharp
private const string AgentPanelId = "ROADPROTO_AGENT_PANEL";
private const string AgentAssistantButtonId = "ROADPROTO_RD_AI_ASSISTANT_OPEN";
```

Inside `TryCreateRibbon()`, add a panel:

```csharp
var agentPanel = tab.Panels.FirstOrDefault(item => item.Source.Id == AgentPanelId);
if (agentPanel == null)
{
    var source = new RibbonPanelSource
    {
        Id = AgentPanelId,
        Title = "Agent",
    };
    agentPanel = new RibbonPanel { Source = source };
    tab.Panels.Add(agentPanel);
}

if (!agentPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == AgentAssistantButtonId))
{
    agentPanel.Source.Items.Add(CreateCommandButton(
        AgentAssistantButtonId,
        "AI 助手",
        "打开 RoadProto 设计软件原型 Agent 面板",
        "RD_AI_ASSISTANT_OPEN "));
}
```

- [ ] **Step 7: 构建 WPF**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: build succeeds and writes `artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll`.

- [ ] **Step 8: 提交**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/ui/wpf/RoadProto.Terrain.UI
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add agent assistant palette"
```

---

## Task 7: 后端模型 Provider 配置与 OpenAI-compatible 客户端

**Files:**
- Create: `src/agent/RoadProto.Agent.Host/Providers/ModelProviderOptions.cs`
- Create: `src/agent/RoadProto.Agent.Host/Providers/OpenAiCompatibleChatClient.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Services/AgentChatService.cs`
- Modify: `src/agent/RoadProto.Agent.Host/Program.cs`
- Test: `src/agent/RoadProto.Agent.Tests/SubgradeTemplateToolPlannerTests.cs`

- [ ] **Step 1: 新增 Provider 配置模型**

Create `src/agent/RoadProto.Agent.Host/Providers/ModelProviderOptions.cs`:

```csharp
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
```

- [ ] **Step 2: 新增 OpenAI-compatible 客户端**

Create `src/agent/RoadProto.Agent.Host/Providers/OpenAiCompatibleChatClient.cs`:

```csharp
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json;

namespace RoadProto.Agent.Host.Providers;

public sealed class OpenAiCompatibleChatClient
{
    private readonly HttpClient httpClient = new();

    public async Task<string> CompleteAsync(ModelProfileOptions profile, string systemPrompt, string userMessage)
    {
        var apiKey = Environment.GetEnvironmentVariable(profile.ApiKeyEnvironmentVariable);
        if (string.IsNullOrWhiteSpace(apiKey))
        {
            return "模型 API Key 未配置，当前使用本地规则规划工具调用。";
        }

        httpClient.BaseAddress = new Uri(profile.BaseUrl.TrimEnd('/') + "/");
        httpClient.Timeout = TimeSpan.FromSeconds(profile.TimeoutSeconds);
        httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", apiKey);

        var payload = new
        {
            model = profile.Model,
            temperature = profile.Temperature,
            messages = new object[]
            {
                new { role = "system", content = systemPrompt },
                new { role = "user", content = userMessage }
            }
        };

        using var response = await httpClient.PostAsJsonAsync("chat/completions", payload).ConfigureAwait(false);
        if (!response.IsSuccessStatusCode)
        {
            return $"模型服务返回错误：{response.StatusCode}";
        }

        using var document = JsonDocument.Parse(await response.Content.ReadAsStringAsync().ConfigureAwait(false));
        var root = document.RootElement;
        return root.GetProperty("choices")[0].GetProperty("message").GetProperty("content").GetString()
            ?? string.Empty;
    }
}
```

- [ ] **Step 3: 新增 AgentChatService**

Create `src/agent/RoadProto.Agent.Host/Services/AgentChatService.cs`:

```csharp
using Microsoft.Extensions.Options;
using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Providers;
using RoadProto.Agent.Host.Skills;
using RoadProto.Agent.Host.Tools;

namespace RoadProto.Agent.Host.Services;

public sealed class AgentChatService
{
    private readonly SubgradeTemplateToolPlanner planner;
    private readonly MarkdownSkillRepository skills;
    private readonly OpenAiCompatibleChatClient modelClient;
    private readonly RoadProtoAgentOptions options;

    public AgentChatService(
        SubgradeTemplateToolPlanner planner,
        MarkdownSkillRepository skills,
        OpenAiCompatibleChatClient modelClient,
        IOptions<RoadProtoAgentOptions> options)
    {
        this.planner = planner;
        this.skills = skills;
        this.modelClient = modelClient;
        this.options = options.Value;
    }

    public async Task<AgentChatResponse> ReplyAsync(AgentChatRequest request)
    {
        var toolCall = planner.TryPlan(request.Message);
        if (toolCall != null)
        {
            return new AgentChatResponse(
                "我识别到你要创建路基模板。请确认工具调用参数。",
                toolCall,
                true);
        }

        var profile = options.ModelProfiles.FirstOrDefault(item => item.Name == request.ModelProfile)
            ?? options.ModelProfiles.FirstOrDefault();
        if (profile == null)
        {
            return new AgentChatResponse(
                "当前未配置模型。你仍可使用首个本地规则工具：创建路基模板。",
                null,
                false);
        }

        var prompt = "你是 RoadProto 设计软件原型 Agent。回答软件操作和道路设计原型问题。可用 skill：\n"
            + skills.ReadSubgradeTemplateCreateSkill();
        var reply = await modelClient.CompleteAsync(profile, prompt, request.Message).ConfigureAwait(false);
        return new AgentChatResponse(reply, null, false);
    }
}
```

- [ ] **Step 4: 更新 Program.cs 注入服务**

Replace `Program.cs` with:

```csharp
using RoadProto.Agent.Host.Models;
using RoadProto.Agent.Host.Providers;
using RoadProto.Agent.Host.Services;
using RoadProto.Agent.Host.Skills;
using RoadProto.Agent.Host.Tools;

var builder = WebApplication.CreateBuilder(args);
builder.Services.Configure<RoadProtoAgentOptions>(
    builder.Configuration.GetSection("RoadProtoAgent"));
builder.Services.AddSingleton<SubgradeTemplateToolPlanner>();
builder.Services.AddSingleton<OpenAiCompatibleChatClient>();
builder.Services.AddSingleton(serviceProvider =>
{
    var root = FindRepositoryRoot(AppContext.BaseDirectory);
    return new MarkdownSkillRepository(root);
});
builder.Services.AddSingleton<AgentChatService>();

var app = builder.Build();

app.MapGet("/health", () => Results.Ok(new { status = "ok" }));
app.MapPost("/api/chat", async (AgentChatRequest request, AgentChatService service) =>
    Results.Ok(await service.ReplyAsync(request).ConfigureAwait(false)));

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
```

- [ ] **Step 5: 运行测试和构建**

Run:

```powershell
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
```

Expected: tests pass, host builds.

- [ ] **Step 6: 提交**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/agent
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add openai compatible agent provider"
```

---

## Task 8: 工具结果文件与 WPF 结果展示

**Files:**
- Modify: `src/cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AgentToolExecutionFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AgentAssistantControl.xaml.cs`

- [ ] **Step 1: C++ 写成功和失败结果文件**

In `ObjectArxAgentToolCommand.cpp`, add:

```cpp
void writeResultFile(
    const std::wstring& resultPath,
    const std::wstring& requestId,
    const std::wstring& tool,
    bool succeeded,
    const std::wstring& message,
    const std::wstring& handle = L"")
{
    if (resultPath.empty()) {
        return;
    }
    std::wofstream output(resultPath);
    output << L"{\n";
    output << L"  \"requestId\": \"" << requestId << L"\",\n";
    output << L"  \"tool\": \"" << tool << L"\",\n";
    output << L"  \"succeeded\": " << (succeeded ? L"true" : L"false") << L",\n";
    output << L"  \"entityHandle\": \"" << handle << L"\",\n";
    output << L"  \"entityType\": \"DnSubgradeTemplateEntity\",\n";
    output << L"  \"message\": \"" << message << L"\"\n";
    output << L"}\n";
}
```

Call it after success:

```cpp
writeResultFile(
    request.resultPath,
    request.requestId,
    request.tool,
    true,
    L"已创建路基模板实体。",
    handle);
```

Call it before each validation return where `request` exists:

```cpp
writeResultFile(request.resultPath, request.requestId, request.tool, false, error);
```

- [ ] **Step 2: WPF 返回请求和结果路径**

Modify `AgentToolExecutionFile.WriteToolRequest` to return both paths:

```csharp
public sealed record AgentToolExecutionPaths(string RequestPath, string ResultPath);

public static AgentToolExecutionPaths WriteToolRequest(AgentToolCall toolCall)
{
    var requestPath = Path.Combine(
        Path.GetTempPath(),
        $"RoadProtoAgentToolRequest_{Process.GetCurrentProcess().Id}_{toolCall.RequestId}.json");
    var resultPath = Path.Combine(
        Path.GetTempPath(),
        $"RoadProtoAgentToolResult_{Process.GetCurrentProcess().Id}_{toolCall.RequestId}.json");

    var payload = new
    {
        tool = toolCall.Tool,
        requestId = toolCall.RequestId,
        arguments = toolCall.Arguments,
        resultPath
    };

    File.WriteAllText(
        requestPath,
        JsonSerializer.Serialize(payload, new JsonSerializerOptions { WriteIndented = true }));
    return new AgentToolExecutionPaths(requestPath, resultPath);
}
```

- [ ] **Step 3: WPF 显示结果路径提示**

Modify `AgentAssistantControl.ExecutePendingTool`:

```csharp
var paths = AgentToolExecutionFile.WriteToolRequest(pendingToolCall);
var requestPath = paths.RequestPath.Replace('\\', '/');
CoreApplication.DocumentManager.MdiActiveDocument?
    .SendStringToExecute($"RD_AI_EXECUTE_TOOL_FILE \"{requestPath}\"\n", true, false, true);
AddMessage("Agent", $"已提交工具执行请求。结果文件：{paths.ResultPath}");
```

- [ ] **Step 4: 构建 ARX 测试和 WPF**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: tests pass and WPF builds.

- [ ] **Step 5: 提交**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/cad_adapter/objectarx/agent src/ui/wpf/RoadProto.Terrain.UI
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: report agent tool execution results"
```

---

## Task 9: 集成构建、AutoCAD 验证和文档收尾

**Files:**
- Modify: `README.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/business/agent/设计软件原型Agent.md`

- [ ] **Step 1: 全量构建**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Release /p:Platform=x64
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
```

Expected:

- ARX build succeeds.
- WPF build succeeds through solution or direct build.
- Agent host builds.
- Agent tests pass.

- [ ] **Step 2: 手工 AutoCAD 验证**

In AutoCAD 2021:

```text
ARXLOAD artifacts\x64\Release\RoadProto_v0.1.31_20260527_SectionDrawingConfig.arx
NETLOAD artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll
```

Start backend:

```powershell
artifacts\agent\Release\RoadProto.Agent.Host.exe
```

Run:

```text
RD_AI_ASSISTANT_OPEN
```

In panel, send:

```text
帮我创建一个二级公路路基模板，比例1:20，名字叫K1路基模板
```

Expected:

- Panel shows confirmation card.
- Confirming sends `RD_AI_EXECUTE_TOOL_FILE`.
- CAD prompts for insertion point.
- Picking point creates `DnSubgradeTemplateEntity`.
- Command line prints created handle.
- Entity displays in model space.

- [ ] **Step 3: 更新测试记录**

Modify `tests/README.md` by adding:

```markdown
## Agent 原型验证

- 核心测试覆盖 Agent 工具请求解析、路基模板工具参数映射和 `AI_AGENT` 模块命令元数据。
- 后端测试覆盖路基模板自然语言规划与非工具问答不误触发。
- AutoCAD 手工验证流程：启动 Agent 后端，加载 ARX 和 WPF 托管插件，运行 `RD_AI_ASSISTANT_OPEN`，确认创建二级公路路基模板并点取插入点。
```

- [ ] **Step 4: 更新业务文档验收记录**

Modify `docs/business/agent/设计软件原型Agent.md` by appending:

```markdown
## 验证记录

- `RD_AI_ASSISTANT_OPEN` 可打开右侧 Agent 面板。
- 本地 Agent 后端 `/health` 可返回正常状态。
- 用户请求创建二级公路路基模板时，后端生成 `cross_section.subgrade_template.create` 工具调用。
- 用户确认后，`RD_AI_EXECUTE_TOOL_FILE` 可创建 `DnSubgradeTemplateEntity`。
```

- [ ] **Step 5: 收尾状态检查**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" status --short
```

Expected: only intended docs are modified.

- [ ] **Step 6: 提交收尾文档**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add README.md docs tests/README.md
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "docs: record agent prototype verification"
```

---

## Task 10: 主目录同步检查

本计划当前在主项目目录 `F:\0_GPT_道路设计原型功能项目` 中执行，不需要从 `.worktrees/<分支名>` 同步回主目录。如果后续切换到 worktree 执行本计划，收尾前必须把以下范围按同相对路径同步回主项目目录：

- `README.md`
- `RoadProto.sln`
- `src/`
- `tests/`
- `docs/agent/`
- `docs/business/agent/`
- `docs/modules/`
- `docs/reuse/`
- `docs/dev/`
- `docs/superpowers/plans/`

同步完成后运行：

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" status --short
```

Expected: 主项目目录保留最新可见副本，且不包含 `.git/`、`.vs/`、`.worktrees/`、`bin/`、`obj/` 和构建缓存。

---

## 验证总命令

完成全部任务后运行：

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj
dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

全部通过后，再执行 AutoCAD 2021 手工验证。

## 自查清单

- [ ] 设计规格中的 6 个大任务都有实施任务覆盖。
- [ ] `cross_section.subgrade_template.create` 参数 schema 覆盖名称、比例、道路等级、插入点、部件、颜色、变宽表、变坡表和结构层引用。
- [ ] WPF 不直接创建 CAD 实体。
- [ ] Agent 后端不依赖 ObjectARX。
- [ ] C++ 工具网关不执行任意命令字符串。
- [ ] 核心测试覆盖不依赖 AutoCAD 的解析和映射逻辑。
- [ ] AutoCAD 手工验证覆盖真实实体创建。
- [ ] 文档、模块索引、复用说明、版本记录同步更新。
