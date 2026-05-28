# 平面布线道路中线实施计划

> 本文件为历史实施计划的中文归档版。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

## 目标

实现平面布线道路中线原型：通过点取控制点创建 `DnRoadCenterlineEntity`，支持基础平曲线计算、显示、DWG 持久化、编辑入口和 Ribbon 入口。

## 主要任务

- 在 `domain/alignment` 中实现道路中线几何、回旋线公式、五单元平曲线构建和桩号格式化。
- 在 `application/alignment` 中组织创建流程。
- 在 `cad_adapter/objectarx/alignment` 中实现道路中线自定义实体、点取流程、显示和持久化。
- 在 `modules/alignment` 中注册 `RD_ALIGN_CENTERLINE_CREATE` 等命令。
- 在 WPF/Ribbon 托管插件中添加平面布线入口和双击编辑转发。
- 补充平面布线业务文档、模块文档、复用说明和版本记录。

## 测试与验证

- 核心测试覆盖回旋线、五单元平曲线、桩号格式化、命令元数据和源码契约。
- AutoCAD 手工验证点取控制点、生成道路中线、保存重开、双击编辑和 Ribbon 按钮。
