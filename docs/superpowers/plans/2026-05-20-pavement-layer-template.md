# 路面结构层模板实施计划

> 本文件为历史实施计划的中文归档版。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

## 目标

实现独立 `DnPavementLayerTemplateEntity`，支持路面结构层模板参数、显示方式、填充、颜色、通用设计字段、WPF 编辑、XML 导入导出和道路模型结构层生成。

## 主要任务

- 在 `domain/cross_section` 中实现 `PavementLayerTemplateModel`、默认值、规则、厚度/加宽/坡度和四边形/梯形横断面预览。
- 在 `application/cross_section` 中实现创建服务。
- 在 `cad_adapter/objectarx/cross_section` 中实现模板实体、绘制、DWG 持久化、创建/编辑命令和 WPF Bridge。
- 在 WPF 中实现参数窗口、层列表、颜色/填充设置、通用设计字段、预览、XML 导入导出。
- 接入路基模板部件绑定和道路模型结构层边界生成。
- 更新业务文档、模块文档、复用说明、README、测试说明和版本记录。

## 测试与验证

- 核心测试覆盖结构层类型、显示名、颜色、填充、设计字段、厚度、加宽、坡度、轮廓几何、命令元数据和源码契约。
- 托管 Bridge 测试覆盖 WPF 请求/响应、XML 往返、预览交互和窗口控件契约。
- AutoCAD 手工验证模板创建、编辑、保存重开、路基模板绑定、道路模型结构层显示。
