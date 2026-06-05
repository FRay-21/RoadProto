# 创建路基模板 Agent Skill

## 工具

- ID：`cross_section.subgrade_template.create`
- 模块：`CROSS_SECTION`
- 业务文档：`docs/business/cross_section/路基模板_创建.md`
- 风险：创建 CAD 自定义实体
- 当前状态：首个已实现的自动化原子函数。WPF 会在执行前展示确认卡片，C++ 工具网关只接受白名单工具和受控结果路径；AutoCAD 图形界面的完整端到端点验仍待手工执行。

## 何时触发

用户要求创建、新建、生成路基模板时触发。例如：

- 创建一个二级公路路基模板。
- 帮我生成城市主干道路基模板，比例 1:20。
- 新建一个左右各两条 3.5 米行车道的路基模板。
- 创建市政道路路基模板，按默认参数。
- 我想创建一个市政道路的模板，并基于默认参数在最右侧增加一个行车道部件。

## 何时不要触发

- 用户只是询问路基模板是什么。
- 用户询问如何手动创建路基模板。
- 用户要求创建边坡模板、道路模型、路面结构层模板或工程量表。

## 参数规则

- `templateName` 缺省为 `默认路基模板`。
- `displayScale` 缺省为 `10`，只允许 `1`、`10`、`20`、`50`、`100`。
- `roadGrade` 缺省为 `Expressway`。
- 用户表达 `市政道路`、`城市道路`、`市政路`、`城区道路` 且没有明确具体等级时，`roadGrade` 使用 `UrbanArterial`。
- `insertionPoint.mode` 缺省为 `PickInCad`。
- 用户未明确给出部件列表时，使用 `DefaultByRoadGrade`。当前 `AgentPlanner` 优先走此路径。
- 用户明确给出完整部件时，使用 `ExplicitComponents`。C++ mapper 已支持该 schema，后续模型工具调用可逐步放开。
- 用户在默认参数基础上提出局部修改时，优先使用 `componentOperations`，不要为了一个局部修改改写完整 `components` 列表。
- 用户只给出部分部件且无法判断左右侧、宽度、类型或插入位置时，先追问。
- 不允许凭空生成 `pavementLayer.handle`。

## 局部修改规则

`componentOperations` 用于表达“基于默认参数再做局部修改”。它不是完整部件列表，而是对默认部件的受控增量操作。

当前已支持的局部修改：

| 用户表达 | 结构化操作 |
| --- | --- |
| 最右侧增加一个行车道部件 | `AddComponent` / `Right` / `TravelLane` / `OutermostMotorLane` |
| 右边再加一条车道 | `AddComponent` / `Right` / `TravelLane` / `OutermostMotorLane` |
| 右侧外侧增加机动车道 | `AddComponent` / `Right` / `TravelLane` / `OutermostMotorLane` |

宽度规则：

- 用户明确说出宽度时，写入 `componentOperations[].width`。
- 用户未说宽度时，`width` 使用 `null`，由 C++ mapper 按同道路等级、同侧最后一个同类型部件宽度补齐。
- 当前 `OutermostMotorLane` 只允许用于 `TravelLane`，不用于硬路肩、土路肩、人行道或边沟。

暂不支持的局部修改必须追问或说明暂未支持，不得强行猜测：

- 左侧新增部件的自然语言自动识别暂未在 `AgentPlanner` 首版开放，需补 Planner 示例和测试后放开。
- 删除、替换、移动部件。
- 同时修改多个不明确的部件。
- “加宽一点”“调整车道”这类没有宽度或对象位置的表达。

## JSON Schema

顶层请求结构：

```json
{
  "tool": "cross_section.subgrade_template.create",
  "requestId": "uuid",
  "arguments": {
    "templateName": "默认路基模板",
    "displayScale": 10,
    "roadGrade": "Expressway",
    "roadCenterlineHandle": "",
    "insertionPoint": {
      "mode": "PickInCad",
      "x": null,
      "y": null,
      "z": 0
    },
    "componentSource": "DefaultByRoadGrade",
    "components": [],
    "componentOperations": []
  },
  "resultPath": ""
}
```

`resultPath` 由受信任宿主生成。模型不得自行填写任意输出路径。

## 必填字段

- `tool`：必须等于 `cross_section.subgrade_template.create`。
- `requestId`：非空请求 ID，由本地后端或宿主生成。
- `arguments`：工具参数对象。
- `arguments.insertionPoint.mode`：插入点模式，缺省时使用 `PickInCad`。
- `arguments.componentSource`：部件来源，缺省时使用 `DefaultByRoadGrade`。

当 `insertionPoint.mode = Explicit` 时：

- `arguments.insertionPoint.x` 必须为数字。
- `arguments.insertionPoint.y` 必须为数字。
- `arguments.insertionPoint.z` 可缺省，缺省为 `0`。

当 `componentSource = ExplicitComponents` 时：

- `components` 必须为非空数组。
- 每个部件必须包含 `side`、`type`、`width`。
- 如果用户没有明确给出部件侧别、类型或宽度，Agent 必须追问。

## 可选字段

- `templateName`：路基模板名称。
- `displayScale`：显示比例。
- `roadGrade`：道路等级。
- `roadCenterlineHandle`：本版保留字段，缺省为空，不主动绑定道路中线。
- `components[].height`：高度差，缺省为 `0`。
- `components[].slopeMode`：坡度模式，缺省为 `Fixed`。
- `components[].fixedSlope`：固定坡度，缺省为 `0`。
- `components[].color`：RGB 颜色，缺省时由 C++ 默认颜色规则提供。
- `components[].wideningTable`：变宽表，缺省为空。
- `components[].variableSlopeTable`：变化坡度表，缺省为空。
- `components[].pavementLayer`：路面结构层模板引用，缺省为未绑定。
- `componentOperations[]`：默认参数基础上的局部部件操作，缺省为空数组。

## 枚举

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

`insertionPoint.mode` 可取：

```text
PickInCad
Explicit
```

`componentSource` 可取：

```text
DefaultByRoadGrade
ExplicitComponents
```

`components[].side` 可取：

```text
Left
Right
```

`components[].type` 可取：

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

`components[].slopeMode` 可取：

```text
Fixed
VariableByStation
```

`componentOperations[].action` 当前可取：

```text
AddComponent
```

`componentOperations[].side` 当前可取：

```text
Left
Right
```

工具协议侧别保留 `Left` 和 `Right`；当前 `AgentPlanner` 首版只自动生成 `Right` 侧新增行车道操作。

`componentOperations[].type` 当前可取：

```text
TravelLane
```

`componentOperations[].position` 当前可取：

```text
OutermostMotorLane
```

## 显式部件结构

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
    { "station": 0, "value": 0 }
  ],
  "variableSlopeTable": [
    { "station": 0, "value": -0.02 }
  ],
  "pavementLayer": {
    "linked": false,
    "handle": "",
    "name": "",
    "thickness": 0
  }
}
```

## 局部操作结构

```json
{
  "action": "AddComponent",
  "side": "Right",
  "type": "TravelLane",
  "position": "OutermostMotorLane",
  "width": null
}
```

## 未知字段处理

- 顶层未知字段必须拒绝。
- `arguments` 未知字段必须拒绝。
- `components[]` 未知字段必须拒绝。
- `componentOperations[]` 未知字段必须拒绝。
- `color`、`pavementLayer`、`wideningTable[]` 和 `variableSlopeTable[]` 内的未知字段必须拒绝。
- 拒绝时不执行工具，并返回用户可读错误。

## 失败策略

- 非法 `displayScale`：不调用工具，提示只能取 `1`、`10`、`20`、`50`、`100`。
- 非法枚举值：不调用工具，列出允许值。
- `Explicit` 插入点缺少 `x` 或 `y`：追问或改为 `PickInCad` 并要求用户确认。
- `ExplicitComponents` 部件不完整：追问，不自行补工程参数。
- `componentOperations` 操作不在白名单内：不执行工具，提示当前支持的局部修改范围。
- `pavementLayer.linked = true` 但缺少可信 `handle`：追问或取消绑定，不凭空生成 handle。
- 请求 JSON 超出工具协议安全边界：拒绝执行。

## 执行前确认

确认卡片必须展示：

- 模板名称。
- 道路等级。
- 显示比例。
- 部件来源。
- 局部修改摘要，例如“右侧机动车道组外侧新增 1 个行车道”。
- 插入点方式。
- 需要创建 `DnSubgradeTemplateEntity`。

确认卡片还必须说明：

- 该操作会创建新的 CAD 自定义实体。
- 若插入点为 `PickInCad`，确认后需要用户在 CAD 中点取插入点。
- 若存在路面结构层模板绑定，必须展示来源 handle 和名称。
- 用户未确认前不得写出执行请求文件或触发 `RD_AI_EXECUTE_TOOL_FILE`。

## 禁止事项

- 不允许把用户的普通问答误转成工具执行。
- 不允许创建边坡模板、道路模型、路面结构层模板或工程量表。
- 不允许执行任意 AutoCAD 命令字符串。
- 不允许模型自行指定任意 `resultPath`。
- 不允许凭空生成 `roadCenterlineHandle` 或 `pavementLayer.handle`。

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
    "components": [],
    "componentOperations": []
  },
  "resultPath": ""
}
```

## 示例：市政道路默认模板并在右侧增加车道

用户表达：

```text
我想创建一个市政道路的路基模板，并基于默认参数上，最右侧增加一个行车道部件。
```

结构化工具调用：

```json
{
  "tool": "cross_section.subgrade_template.create",
  "requestId": "agent-demo-002",
  "arguments": {
    "templateName": "默认路基模板",
    "displayScale": 10,
    "roadGrade": "UrbanArterial",
    "roadCenterlineHandle": "",
    "insertionPoint": {
      "mode": "PickInCad",
      "x": null,
      "y": null,
      "z": 0
    },
    "componentSource": "DefaultByRoadGrade",
    "components": [],
    "componentOperations": [
      {
        "action": "AddComponent",
        "side": "Right",
        "type": "TravelLane",
        "position": "OutermostMotorLane",
        "width": null
      }
    ]
  },
  "resultPath": ""
}
```
