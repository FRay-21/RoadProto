# 路面工程量统计表设计

## 背景

用户需要在 Ribbon 中新增 `出图出表` 模块，并提供 `路面工程量统计表` 命令。命令以 `查看横断面` 已绘制的横断面图为选择对象，统计路面结构层的面积和体积，并生成 Excel 可打开的 `.xls` 表格。

## 设计选择

采用独立 `DRAWING_QUANTITY` 模块，命令前缀为 `RD_DRAWING_`。核心计算放入 `src/domain/quantity`，CAD 选择、道路模型读取和输出路径提示放入 `src/cad_adapter/objectarx/drawing_quantity`，Ribbon 可见入口放入托管插件 `RoadProtoRibbonExtension.cs`。

## 数据流

1. 用户选择一组 `DnRoadModelSectionDrawingEntity`。
2. 命令校验这些断面图来自同一个道路模型。
3. 优先读取对应 `DnRoadModelEntity` 中的断面结构层节点和构造物范围。
4. 每个断面同名结构层合并左右侧和多个部件的投影宽度、截面积。
5. 领域计算器按构造物范围切分普通段、桥梁段和隧道段。
6. 面积按投影宽度沿桩号累计，体积按平均断面法累计。
7. 写出 SpreadsheetML `.xls` 文件。

## 关键规则

- 构造物范围按起终桩号切段；桥梁输出 `桥梁段`，隧道输出 `隧道段`，其余连续区间输出 `普通段`。
- 若构造物边界没有断面图，按相邻断面线性插值边界处的投影宽度和截面积。
- 表格列为序号、起讫桩号、类型、各结构层面积列、各结构层体积列。
- `domain/quantity` 不依赖 ObjectARX。

## 测试策略

- 核心测试覆盖构造物切段、边界插值、平均断面法体积、动态 `.xls` 列和模块/Ribbon 元数据。
- AutoCAD 手工验证覆盖横断面图选择、输出路径提示和 Excel 打开效果。
