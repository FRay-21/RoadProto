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
