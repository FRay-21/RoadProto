# 清表算量面域接口

## 能力分类

通用道路设计能力

## 能力说明

该能力为横断面图 `清表` 面域预留独立算量入口。清表层属于工程量对象，但不属于路面工程量；本接口只负责从横断面图面域中承接清表层点列、侧别、来源配置行和厚度，后续清表面积、体积、清单编码和报表输出应在独立功能中实现。

## 当前实现

- 源码路径：`src/domain/quantity/ClearTableQuantityDrawingFaceSampler.*`
- 对外类型/函数：`ClearTableQuantityDrawingFaceSampler::sampleAtStation`
- 当前使用该能力的命令：暂无正式用户命令；`ObjectArxPavementQuantityTableCommand.cpp` 已预留 `clearTableQuantityFacesFromSectionDrawing` 转换入口。

## 可复用内容

- 清表面域点列承接。
- 清表面域侧别承接。
- 清表配置来源行号承接。
- 清表厚度承接。
- 非有限点和少点面域过滤。

## 不可复用或临时内容

- 当前不计算清表面积、体积或工程量。
- 当前不生成清表工程量表格。
- 当前不定义清表单位、清单编码和报表口径的正式业务规则。

## 依赖关系

- domain 依赖：无。
- cad_adapter 依赖：无；领域接口不依赖 ObjectARX。
- 模块依赖：后续由 `DRAWING_QUANTITY` 模块的清表算量命令调用。

## 扩展说明

- 后续可在独立清表工程量命令中读取同一道路模型下的横断面图，使用该接口形成清表断面样本。
- 后续可按桩号区间累计清表面积或体积，并输出独立清表工程量报表。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- 当前自动化验证：清表面域采样保留桩号、侧别、来源配置行、厚度和点列，并过滤非有限或少点面域。
