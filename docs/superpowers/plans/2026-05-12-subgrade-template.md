# 路基模板实施计划

> 本文件为历史实施计划的中文归档版。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

## 目标

实现 `RD_SECTION_SUBGRADE_TEMPLATE_CREATE` 路基模板原型，支持默认部件、WPF 参数编辑、路面结构层模板绑定和 DWG 持久化。

## 主要任务

- 在 `domain/cross_section` 中实现路基模板模型、默认值和规则。
- 在 `application/cross_section` 中实现创建服务。
- 在 `cad_adapter/objectarx/cross_section` 中实现 `DnSubgradeTemplateEntity`、创建命令和 WPF Bridge。
- 在 WPF 中实现路基模板参数窗口、部件编辑、变宽/变坡表和结构层模板点选请求。
- 更新横断面模块文档、业务文档、复用说明和测试说明。

## 验证

- 核心测试覆盖默认模板、部件显示名、坡度/变宽规则、模板绑定和命令元数据。
- AutoCAD 手工验证创建、编辑、点选结构层模板、保存重开和夹点移动。
