# 路面工程量统计表实施计划

> 本文件为历史实施计划的中文归档版。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

## 目标

新增 `RD_DRAWING_PAVEMENT_QUANTITY_TABLE`，从横断面图或道路模型断面数据生成路面结构层面积和体积统计 `.xls` 表格。

## 主要任务

- 新增 `DRAWING_QUANTITY` 模块和 Ribbon 面板。
- 在 `domain/quantity` 中实现构造物切段、投影面积、平均断面法体积和 SpreadsheetML `.xls` 写出。
- 在 ObjectARX 中实现横断面图选择、道路模型读取和输出路径提示。
- 更新业务文档、模块文档、复用说明、README、测试说明和版本记录。

## 验证

- 核心测试覆盖分段、插值、面积体积、动态列和命令元数据。
- AutoCAD 手工验证横断面图选择、路径选择、表格生成和 Excel 打开效果。
