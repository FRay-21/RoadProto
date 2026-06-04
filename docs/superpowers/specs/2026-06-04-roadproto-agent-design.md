# RoadProto 设计软件原型 Agent 设计规格

## 目标

本设计用于在 RoadProto 当前架构中加入一个可维护的设计软件原型 Agent。首版目标不是做完整智能设计平台，而是搭起可扩展框架，并实现一个可真实执行的自动化原子函数：创建路基模板。

首版完成后，用户应能在 AutoCAD 中打开 RoadProto 右侧 AI 面板，通过自然语言描述需求，由 Agent 识别意图、读取 RoadProto Agent skill 文档、生成受控工具调用，并通过 C++ ObjectARX 白名单工具网关自动创建 `DnSubgradeTemplateEntity` 路基模板实体。

## 架构原则

RoadProto 继续遵守“C++ ObjectARX 核心 + 可替换 UI 层”的架构边界：

- Agent 对话框是新的 UI 入口，不承载 CAD 核心业务逻辑。
- 本地 Agent 后端不依赖 ObjectARX，不直接读写 `AcDbEntity`、`AcDbObjectId`、`ads_name`。
- WPF 面板只负责展示、输入、模型选择、工具确认和结果反馈。
- C++ 工具网关只执行白名单原子函数，不执行模型自由生成的 AutoCAD 命令字符串。
- 自动化工具应复用现有 domain/application/cad_adapter 能力，避免与用户命令形成两套业务规则。

推荐总体形态：

```text
AutoCAD / ARX
  -> RD_AI_ASSISTANT_OPEN
  -> WPF Palette 右侧 AI 面板
  -> localhost Agent Sidecar
  -> 模型 Provider / 知识检索 / Skill 读取 / 工具规划
  -> 工具调用 JSON
  -> RD_AI_EXECUTE_TOOL_FILE
  -> C++ 白名单工具网关
  -> domain / application / cad_adapter
```

## 大任务划分

### 1. Agent 架构与文档基线

新增 Agent 相关说明文档，固定架构边界、工具协议、skill 文档规则、安全确认规则和首个原子函数说明。建议文档落点：

- `docs/agent/overview.md`
- `docs/agent/tool_protocol.md`
- `docs/agent/skill_authoring_rules.md`
- `docs/agent/skills/cross_section/subgrade_template_create.md`
- `docs/business/agent/设计软件原型Agent.md`
- `docs/reuse/agent_tool_gateway.md`

若正式新增命令，还应同步更新模块文档、命令元数据业务文档路径和版本记录。

### 2. 本地 Agent 后端服务

新增独立本地 sidecar 服务，推荐使用 `.NET 8 Minimal API`。服务运行在本机 `localhost`，由 WPF 面板启动或连接。

职责：

- 管理会话和消息历史。
- 读取模型配置，调用模型 Provider。
- 读取 RoadProto Agent skill 文档和知识库 markdown。
- 判断用户意图：闲聊、软件操作问答、专业知识问答、工具调用。
- 输出受控工具调用 JSON。
- 保存工具调用日志和执行结果摘要。

后端不得调用 ObjectARX，不直接操作 CAD 对象。

### 3. 模型 Provider 适配层

模型 Provider 统一抽象为 OpenAI-compatible chat/tool calling 接口。首版支持以下供应商配置：

- OpenAI / ChatGPT
- DeepSeek
- 阿里百炼 / 千问

配置项包括：

- `provider`
- `baseUrl`
- `apiKeyEnvironmentVariable`
- `model`
- `temperature`
- `timeoutSeconds`
- `enableStreaming`

API Key 不写入仓库。仓库只保留示例配置和环境变量名称。

### 4. CAD 右侧 Agent 对话框

在现有托管 WPF 插件中新增 AutoCAD `PaletteSet + WPF UserControl`。命令建议：

- `RD_AI_ASSISTANT_OPEN`：打开或激活右侧 Agent 面板。
- `RD_AI_ASSISTANT_CLOSE`：关闭或隐藏 Agent 面板，可选。

面板能力：

- 输入自然语言问题。
- 展示流式或非流式回复。
- 支持选择模型配置。
- 展示工具调用确认卡片。
- 展示工具执行结果和错误。
- 支持关闭后再次打开，保留当前 AutoCAD 会话内的最近消息。

WPF 不直接创建 CAD 实体，只通过后端和 C++ 工具网关转发执行请求。

### 5. 工具调用网关

新增 C++ 侧受控工具网关，建议命令：

```text
RD_AI_EXECUTE_TOOL_FILE "<jsonPath>"
```

执行流程：

1. WPF 面板从后端获得工具调用 JSON。
2. WPF 展示执行确认卡片。
3. 用户确认后，WPF 将 JSON 写入临时文件。
4. WPF 发送 `RD_AI_EXECUTE_TOOL_FILE "<jsonPath>"`。
5. C++ 读取 JSON，校验工具名和参数。
6. C++ 只执行白名单中的工具。
7. C++ 将结果写回结果文件或通过 editor 输出，并刷新图形。

工具网关不得执行任意命令字符串。所有工具必须有固定名称、固定 schema、固定参数校验和固定业务文档路径。

### 6. Agent Skill 文档体系

RoadProto Agent skill 是给本项目 Agent 后端读取的能力说明文档，不等同于 Codex 本机 skill。每个可调用工具应有一份 Agent skill 文档，用来帮助模型判断何时调用、如何补参数、何时追问、何时禁止执行。

建议每份 Agent skill 包含：

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

首个 skill 文档为：

```text
docs/agent/skills/cross_section/subgrade_template_create.md
```

### 7. 首个自动化原子函数

首个原子函数为自动化创建路基模板：

```text
tool: cross_section.subgrade_template.create
module: CROSS_SECTION
risk: creates-cad-entity
entity: DnSubgradeTemplateEntity
businessDocPath: docs/business/cross_section/路基模板_创建.md
```

该工具不打开现有 WPF 参数窗口，而是根据工具参数直接创建 `DnSubgradeTemplateEntity`。缺省值由现有 C++ 默认规则和归一化规则提供。

## 首个工具参数设计

工具请求 JSON 建议结构如下：

```json
{
  "tool": "cross_section.subgrade_template.create",
  "requestId": "uuid",
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
  "resultPath": "%TEMP%/RoadProtoAgentToolResult_<pid>_<requestId>.json"
}
```

### 顶层参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `templateName` | string | `默认路基模板` | 路基模板名称。 |
| `displayScale` | number | `10` | 只允许 `1`、`10`、`20`、`50`、`100`。 |
| `roadGrade` | enum | `Expressway` | 道路等级。 |
| `roadCenterlineHandle` | string | 空 | 本版保留字段，不主动绑定道路中线。 |
| `insertionPoint.mode` | enum | `PickInCad` | `PickInCad` 或 `Explicit`。 |
| `insertionPoint.x/y/z` | number/null | `null/null/0` | `Explicit` 时必须提供 `x`、`y`，`z` 缺省为 `0`。 |
| `componentSource` | enum | `DefaultByRoadGrade` | `DefaultByRoadGrade` 或 `ExplicitComponents`。 |
| `components` | array | 空 | 用户明确给出部件时使用。 |

### 枚举

`roadGrade` 可取：

```text
Expressway
FirstClass
SecondClass
ThirdClass
FourthClass
UrbanExpressway
UrbanArterial
UrbanSubArterial
UrbanBranch
```

`component.side` 可取：

```text
Left
Right
```

`component.type` 可取：

```text
Median
TravelLane
HardShoulder
EarthShoulder
SideMedian
Sidewalk
BikeLane
CurbStrip
```

`component.slopeMode` 可取：

```text
Fixed
VariableByStation
```

### 部件参数

当 `componentSource = ExplicitComponents` 时，`components` 中每个部件可包含：

```json
{
  "side": "Left",
  "type": "TravelLane",
  "width": 3.75,
  "height": 0,
  "slopeMode": "Fixed",
  "fixedSlope": -0.02,
  "color": {
    "r": 0,
    "g": 90,
    "b": 180
  },
  "wideningTable": [
    { "station": 0, "value": 0 },
    { "station": 100, "value": 0.5 }
  ],
  "variableSlopeTable": [
    { "station": 0, "value": -0.02 },
    { "station": 100, "value": -0.025 }
  ],
  "pavementLayer": {
    "linked": false,
    "handle": "",
    "name": "",
    "thickness": 0
  }
}
```

部件默认规则：

- `width` 缺省为 `0`，但显式部件中建议由 Agent 追问用户确认。
- `height` 缺省为 `0`。
- `slopeMode` 缺省为 `Fixed`。
- `fixedSlope` 缺省为 `0`。
- `color` 缺省时按部件类型调用 C++ 默认颜色规则。
- `wideningTable` 缺省为空。
- `variableSlopeTable` 缺省为空；仅 `slopeMode = VariableByStation` 时使用。
- `pavementLayer.linked` 缺省为 `false`。
- 不允许 Agent 凭空生成 `pavementLayer.handle`；只有用户明确提供或通过 CAD 点选获得时才可绑定。

### 默认值和覆盖规则

首版采用“完整 schema、保守执行”：

- 用户未明确给出部件列表时，`componentSource` 使用 `DefaultByRoadGrade`，C++ 调用 `SubgradeTemplateDefaults::create(roadGrade)`。
- 用户明确给出完整部件时，`componentSource` 使用 `ExplicitComponents`，C++ 按参数构建数据并调用 `SubgradeTemplateRules::normalize`。
- 用户只说道路等级、比例、名称时，只覆盖这些顶层字段，不改默认部件。
- 用户给出部分部件但语义不完整时，Agent 应追问，而不是自行补齐工程参数。
- 插入点未给出时，CAD 命令提示用户点取。

## 工具执行结果

工具成功结果建议：

```json
{
  "requestId": "uuid",
  "tool": "cross_section.subgrade_template.create",
  "succeeded": true,
  "entityHandle": "AB12",
  "entityType": "DnSubgradeTemplateEntity",
  "templateName": "K1 路基模板",
  "roadGrade": "SecondClass",
  "displayScale": 20,
  "componentCount": 6,
  "message": "已创建路基模板实体。"
}
```

工具失败结果建议：

```json
{
  "requestId": "uuid",
  "tool": "cross_section.subgrade_template.create",
  "succeeded": false,
  "errorCode": "InvalidDisplayScale",
  "message": "路基模板显示比例必须为 1、10、20、50 或 100。"
}
```

## Agent Skill 触发规则示例

首个 Agent skill 应规定：

- 当用户说“创建路基模板”“新建路基模板”“生成某等级道路路基模板”时触发。
- 当用户只是询问“路基模板是什么”“怎么创建路基模板”时不要触发工具，只回答知识。
- 当用户要求创建道路模型、边坡模板、路面结构层模板时不要误触发本工具。
- 创建实体前必须展示确认卡片。
- 如果缺少插入点，确认卡片应说明“确认后需要在 CAD 中点取插入点”。
- 如果用户提供非法比例，应纠正或追问，不调用工具。
- 如果用户要求绑定路面结构层模板但没有提供 handle 或点选来源，应追问或引导先选择，不凭空生成 handle。

## 测试策略

### 后端测试

- 模型配置读取。
- Provider 选择。
- Agent skill 文档加载。
- 用户语义到工具 JSON 的转换。
- 非工具问答不误触发工具。
- 非法参数拒绝或追问。

### C++ 核心测试

- 工具参数到 `SubgradeTemplateData` 的转换。
- 道路等级默认部件生成。
- 显示比例校验。
- 显式部件颜色缺省规则。
- 显式部件变宽表、变坡表解析。
- 工具命令元数据和业务文档路径。

### AutoCAD 手工验证

- 加载 ARX 和 WPF 托管插件。
- 打开右侧 Agent 面板。
- 使用自然语言创建默认二级道路路基模板。
- 使用自然语言创建显式部件路基模板。
- 未提供插入点时可在 CAD 中点取。
- 创建成功后实体显示、handle 输出、DWG 保存重开可见。

## 首版不做范围

首版暂不实现：

- 远程多用户 Agent 平台。
- 用户权限系统。
- 云端知识库管理后台。
- 多 Agent 协作。
- 任意 AutoCAD 命令执行。
- 静默绑定道路中线。
- 凭空绑定路面结构层模板。
- 自动创建道路模型、边坡模板或工程量表。

## 实施顺序建议

1. 写 Agent 架构文档、工具协议文档和首个 skill 文档。
2. 新建本地 Agent 后端骨架。
3. 接入 OpenAI-compatible Provider 配置。
4. 实现 skill 文档读取和首个工具 JSON 规划。
5. 新增 WPF Palette AI 面板。
6. 新增 C++ 工具网关命令。
7. 实现 `cross_section.subgrade_template.create` 自动化工具。
8. 增加测试和 AutoCAD 手工验证记录。

## 设计结论

本方案把 Agent 能力拆为 UI、后端、skill、工具协议和 CAD 执行网关五个边界。它比直接在 WPF 中调用模型并拼接 CAD 命令更慢一点，但更适合 RoadProto 长期扩展。后续新增原子函数时，只需要按固定路径增加 Agent skill、工具 schema、C++ 白名单执行器和必要测试，不需要重做 Agent 平台。
