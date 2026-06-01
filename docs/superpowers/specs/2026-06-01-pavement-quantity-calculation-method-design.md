# 路面工程量统计断面计算方法设计

## 背景

`RD_DRAWING_PAVEMENT_QUANTITY_TABLE` 当前已从横断面图结构层面域或道路模型断面数据生成路面工程量统计表。面积列按结构层平面投影宽度沿桩号累计，体积列按每层横断面截面积使用平均断面法累计。用户需要在导出时增加“断面计算方法”选择，保留当前精确横断面面积算法，同时支持设计院常用的“依照路面面积方法”。

## 目标

- 在路面工程量统计表导出保存界面增加一个独立的“断面计算方法”选项。
- 方法一为 `平均断面法`，保持现有行为：相邻桩号每层横断面面积平均后乘桩号差。
- 方法二为 `依照路面面积方法`：面积列仍输出平面投影面积；体积列按该层平面投影面积乘该层厚度计算。
- 断面计算方法与现有“统计方式”互不替代：统计方式继续控制 `按部件和结构层` 或 `按结构层类型` 聚合，断面计算方法只控制体积算法。
- 核心算法保留在 `domain/quantity`，CAD 保存对话框只负责收集用户选项和传参。

## 非目标

- 不改变横断面图结构层面域采样规则。
- 不改变清表面域排除规则。
- 不新增 Excel 模板、材料编码、清单编号或多道路汇总。
- 不修改路面结构层模板、横断面图配置或道路模型生成逻辑。

## 领域模型

新增领域枚举 `PavementQuantityCalculationMethod`：

- `AverageEndArea`：平均断面法，作为旧重载和默认导出行为。
- `PlanAreaByThickness`：依照路面面积方法。

`PavementQuantityTableCalculator::build` 增加带 `PavementQuantityCalculationMethod` 的重载。现有不带方法参数的重载保留，并继续默认走 `AverageEndArea`，避免影响已有调用方和测试。

## 计算规则

面积列两种方法一致：

```text
平面面积 = (起点投影宽度 + 终点投影宽度) / 2 × 桩号差
```

`平均断面法` 体积保持现状：

```text
体积 = (起点横断面面积 + 终点横断面面积) / 2 × 桩号差
```

`依照路面面积方法` 先从每个断面反推厚度：

```text
厚度 = 横断面面积 / 投影宽度
区间平均厚度 = (起点厚度 + 终点厚度) / 2
体积 = 平面面积 × 区间平均厚度
```

若某端投影宽度为 0 或该层不存在，则该端厚度按 0 处理。所有宽度、截面积仍要求非负有限数值。

## CAD 交互

`ObjectArxPavementQuantityTableCommand.cpp` 的保存对话框继续使用 `IFileDialogCustomize`：

- 保留现有 `统计方式` 单选组。
- 新增 `断面计算方法` 单选组。
- 默认选中 `平均断面法`，保证旧流程不变。
- 当系统保存对话框不可用并回退 `acedGetFileD` 时，继续使用默认统计方式 `按部件和结构层`，默认断面计算方法 `平均断面法`，并在命令行警告中说明使用默认选项。

## 数据流

1. 用户选择同一道路模型的一组横断面图。
2. 命令优先从横断面图当前结构层面域采样 `projectedWidth` 和 `sectionArea`，无面域时回退道路模型断面数据。
3. 命令收集输出路径、统计方式和断面计算方法。
4. 命令调用 `PavementQuantityTableCalculator::build(samples, structures, aggregationMode, calculationMethod, errorMessage)`。
5. `PavementQuantityTableXlsWriter` 仍只接收最终表格，不感知算法选择。

## 错误处理

- 数据校验沿用现有错误信息：横断面数量、重复桩号、无结构层、宽度和截面积非负有限。
- `PlanAreaByThickness` 遇到 0 投影宽度时不报错，厚度按 0，避免除零。
- 保存对话框不可用时不阻断导出，使用默认算法并提示。

## 测试

核心测试新增覆盖：

- `AverageEndArea` 显式参数与旧默认重载结果一致。
- `PlanAreaByThickness` 在面积列不变的前提下，体积按平面面积乘平均厚度计算。
- 0 投影宽度时 `PlanAreaByThickness` 不除零，体积贡献为 0。
- CAD 命令源码契约包含新的“断面计算方法”对话框文案和 `PlanAreaByThickness` 传参。

## 文档同步

实现时同步更新：

- `docs/business/drawing_quantity/路面工程量统计表.md`
- `docs/reuse/pavement_quantity_table.md`
- `docs/reuse/capability_catalog.md`
- `docs/modules/drawing_quantity.md`
- `docs/dev/version_log.md`
- `tests/README.md`
- `README.md`
