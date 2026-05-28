# 横断面图配置实施计划

> 本文件为历史实施计划的中文归档版。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

## 目标

实现 `RD_SECTION_DRAWING_CONFIG`，让用户选择一个横断面图后配置同一道路模型全部横断面图的路面结构层，并让工程量统计优先使用图上当前面域。

## 主要任务

- 在 `domain/cross_section` 中实现 `SectionDrawingConfigModel`，支持 CSV 导入导出、桩号范围、路基类型多选、模板 handle 和行优先级。
- 在 `domain/quantity` 中新增横断面图面域采样器，按当前面域计算投影宽度和断面面积。
- 扩展 `DnRoadModelSectionDrawingEntity`，持久化配置、结构层面域、来源模板、配置行号、`manualEdited` 和顶点夹点。
- 新增 C++/WPF Bridge 和 `SectionDrawingConfigWindow`，支持 CSV 路径、导入、导出、模板点选和绘制动作。
- 在 ObjectARX 中实现同一道路模型横断面图扫描、模板读取、面域重建和手动编辑面域保留。
- 更新横断面模块、出图出表算量、业务文档、复用说明、README、测试说明和版本记录。

## 测试与验证

- 核心测试覆盖配置 CSV、路基类型匹配、行优先级、面域采样、实体持久化、夹点契约和命令元数据。
- 托管 Bridge 测试覆盖请求/响应、CSV 往返、窗口控件和 Ribbon 入口。
- AutoCAD 手工验证选择横断面图、CSV 导入导出、模板点选、批量绘制、顶点夹点修改、双击二次编辑和工程量统计。
