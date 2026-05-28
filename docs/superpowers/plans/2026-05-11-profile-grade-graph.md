# 纵断面拉坡图实施计划

> 本文件为历史实施计划的中文归档版。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

## 目标

实现 `RD_PROFILE_GRADE_GRAPH_CREATE` 纵断面拉坡图原型，支持从道路中线关联数模或 `.dmx` 文件生成纵断面地面线图。

## 主要任务

- 在 `domain/profile` 中实现 DMX 文件读取、桩号/高程样本、布局映射和网格范围计算。
- 在 `application/profile` 中实现拉坡图创建服务。
- 在 `cad_adapter/objectarx/profile` 中实现 `DnProfileGradeGraphEntity`、命令选择流程、DMX 路径选择和 WPF 属性 Bridge。
- 在 `modules/profile` 和托管 Ribbon 中注册纵断面拉坡图入口。
- 增加业务文档、模块文档、复用说明、测试说明和版本记录。

## 测试与验证

- 核心测试覆盖 DMX 解析、布局映射、默认属性、无效输入和命令元数据。
- AutoCAD 手工验证选择道路中线、DMX 回退、拉坡图显示、属性编辑、保存重开和 Ribbon 入口。
