# 创建路基模板 Agent Skill

## 工具

- ID：`cross_section.subgrade_template.create`
- 模块：`CROSS_SECTION`
- 业务文档：`docs/business/cross_section/路基模板_创建.md`
- 风险：创建 CAD 自定义实体
- 当前状态：计划中的首个自动化原子函数，工具执行器和 UI 确认流程仍需后续实现。

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
    "components": []
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

## 未知字段处理

- 顶层未知字段必须拒绝。
- `arguments` 未知字段必须拒绝。
- `components[]` 未知字段必须拒绝。
- `color`、`pavementLayer`、`wideningTable[]` 和 `variableSlopeTable[]` 内的未知字段必须拒绝。
- 拒绝时不执行工具，并返回用户可读错误。

## 失败策略

- 非法 `displayScale`：不调用工具，提示只能取 `1`、`10`、`20`、`50`、`100`。
- 非法枚举值：不调用工具，列出允许值。
- `Explicit` 插入点缺少 `x` 或 `y`：追问或改为 `PickInCad` 并要求用户确认。
- `ExplicitComponents` 部件不完整：追问，不自行补工程参数。
- `pavementLayer.linked = true` 但缺少可信 `handle`：追问或取消绑定，不凭空生成 handle。
- 请求 JSON 超出工具协议安全边界：拒绝执行。

## 执行前确认

确认卡片必须展示：

- 模板名称。
- 道路等级。
- 显示比例。
- 部件来源。
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
    "components": []
  },
  "resultPath": ""
}
```
