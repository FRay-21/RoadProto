# 横断面设计模块

## 模块信息

- 模块名称：横断面设计
- 模块编码：`CROSS_SECTION`
- 命令前缀：`RD_SECTION_`
- 当前状态：已实现路基模板独立实体创建、WPF 参数窗口、二维预览、双击编辑入口和桥接回写。

## 命令清单

| 命令 | 显示名称 | 类型 | 文档 |
| --- | --- | --- | --- |
| `RD_SECTION_SUBGRADE_TEMPLATE_CREATE` | 创建路基模板 | 用户命令 | `docs/business/cross_section/路基模板_创建.md` |
| `RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE` | 按句柄编辑路基模板 | 内部桥接命令 | `docs/business/cross_section/路基模板_编辑.md` |
| `RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE` | 应用路基模板对话框结果 | 内部桥接命令 | `docs/business/cross_section/路基模板_WPF桥接回写.md` |

## Ribbon

- C++ Ribbon model：`RoadProto / 横断面设计 / 创建路基模板`
- 可见 AutoCAD WPF Ribbon：`RoadProto / 横断面设计 / 创建路基模板`
- 托管 Ribbon 插件文件：`src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`

## 代码落点

| 层 | 代码 | 职责 |
| --- | --- | --- |
| domain | `src/domain/cross_section/SubgradeTemplateModel.*` | 路基模板枚举、数据模型、默认值、颜色、显示比例和基础规则 |
| application | `src/application/cross_section/SubgradeTemplateCreateService.*` | 创建命令默认模板数据生成 |
| modules | `src/modules/cross_section/CrossSectionModule.*` | 模块、命令和 C++ Ribbon 元数据注册 |
| startup | `src/app/startup/CrossSectionStartupRegistration.*` | 启动期注册 `CROSS_SECTION` 模块 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.*` | 自定义实体显示、DWG 持久化、几何范围和变换 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/ObjectArxSubgradeTemplateCommand.*` | 插入点点取、弹窗、实体创建和回写命令 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/SubgradeTemplateDialogBridge.*` | WPF 请求/响应文件桥接 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/SubgradeTemplateWindow.xaml` | 参数窗口和二维预览 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/StationValueTableWindow.xaml` | 变宽/变坡二级表格 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SubgradeTemplateDialogCommands.cs` | WPF 弹窗命令和响应转发 |

## 边界

- `domain/cross_section` 不依赖 ObjectARX。
- WPF 不直接读写 CAD 自定义实体。
- 路基模板当前是独立实体，不绑定道路中线。
- 与道路中线、路面结构层和统一关系管理机制的关联在后续功能中扩展。
## 2026-05-13 更新

- 路基模板默认值已覆盖一级道路、二级道路、三级道路、四级道路、城市主干道、城市次干道和城市支路。
- WPF 路基模板编辑模式保留实体已保存参数，仅在空白新建请求时回退到高速公路默认值。
- 预览图支持直接点选部件，左、右按钮按横断面几何顺序移动。
