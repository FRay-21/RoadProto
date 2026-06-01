# 路面工程量统计表

## 能力分类

通用道路设计能力

## 能力说明

该能力从多个横断面结构层截面样本中生成路面工程量统计表。它负责按构造物范围切分普通段、桥梁段和隧道段，并按平面投影面积累计面积。体积算法可选择平均断面法或依照路面面积方法；后者保持面积列为平面投影面积，体积按平面面积乘平均厚度计算。聚合方式支持按结构层类型汇总，也支持按部件名称和结构层名称组合拆分。

## 当前实现

- 源码路径：`src/domain/quantity/PavementQuantityTable.*`、`src/domain/quantity/RoadModelPavementQuantitySampler.*`、`src/domain/quantity/PavementQuantityDrawingFaceSampler.*`
- 对外类型/函数：`PavementQuantityCalculationMethod`、`PavementQuantityTableCalculator::build`、`PavementQuantityTableXlsWriter::write`、`RoadModelPavementQuantitySampler::sampleAtStation`、`PavementQuantityDrawingFaceSampler::sampleAtStation`
- 当前使用该能力的命令：`RD_DRAWING_PAVEMENT_QUANTITY_TABLE`

## 可复用内容

- 构造物边界切段。
- 缺少构造物边界断面时的线性插值。
- 从 `DnRoadModelSectionDrawingEntity` 当前结构层面域采样投影宽度和截面积，支持图上夹点修改后的数据进入算量。
- 路面工程量采样明确排除 `clearTable:` 清表面域；清表面域及其厚度由独立清表算量接口承接。
- 按 `部件名称-结构层名称` 或按结构层类型的双模式聚合。
- 旧道路模型缺少结构层节点部件名时，可从路基部件线和结构层纵向线反推部件名称。
- 结构层平面投影面积梯形累计。
- 结构层体积支持平均断面法累计。
- 结构层体积支持依照路面面积方法累计：面积列仍为平面投影面积，体积按平面面积乘平均厚度计算。
- 动态结构层列 SpreadsheetML `.xls` 写出，包含居中、自动换行、列宽、宋体/Times New Roman 和 10 号字号样式。

## 不可复用或临时内容

- 当前命令从 `DnRoadModelSectionDrawingEntity` 和 `DnRoadModelEntity` 读取数据，属于 ObjectARX 适配层接入。
- 横断面图面域的图形编辑和 `manualEdited` 标记由 CAD 自定义实体负责，领域采样器只接收已整理的点列和来源字段。
- `.xls` 第一版使用 SpreadsheetML，后续正式报表可替换为模板化 Excel 输出。

## 依赖关系

- domain 依赖：`domain/alignment/StationFormatter`
- cad_adapter 依赖：无；领域能力不依赖 ObjectARX。
- 模块依赖：当前由 `DRAWING_QUANTITY` 模块调用。

## 扩展说明

- 后续可增加材料编码、单位、清单项映射和多道路汇总。
- 后续清表工程量应使用独立清表算量接口及清表厚度字段，不并入本路面工程量统计表。
- 后续可把构造物范围替换为正式桥梁、隧道对象关系。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- AutoCAD 手工验证：加载 ARX 和托管 DLL 后，先通过 `查看横断面 / 绘制横断面` 生成断面图，再运行 `RD_SECTION_DRAWING_CONFIG` 配置并绘制图上路面结构层，可拖动某一层面域顶点改变面积；随后运行 `RD_DRAWING_PAVEMENT_QUANTITY_TABLE` 选择同一道路模型的一组断面图，分别用 `按部件和结构层`、`按结构层类型`、`平均断面法` 与 `依照路面面积方法` 导出 `.xls`，检查结果以修改后的面域尺寸和所选断面计算方法为准。
