# 平面设计模块

## 模块编码

`ALIGNMENT`

## 模块目的

承载道路平面线形、道路中线实体和后续纵断面、横断面、交叉口联动所需的路线基础对象。

## V0.1 命令

| 命令 | 说明 | 业务文档 |
| --- | --- | --- |
| `RD_ALIGN_CENTERLINE_CREATE` | 点取控制点并生成道路中线自定义实体，支持连续控制点链。 | `docs/business/alignment/平面布线_道路中线创建.md` |
| `RD_ALIGN_CURVE_PARAM_EDIT` | 通过 WPF 编辑道路中线的 `T1`、`T2`、`LS1`、`R`、`LS2`。 | `docs/business/alignment/平曲线参数编辑.md` |
| `RD_ALIGN_CENTERLINE_EXPORT_ICD` | 选择道路中线实体并导出为 `.icd` 积木法线形单元文件。 | `docs/business/alignment/道路中线_ICD导入导出.md` |
| `RD_ALIGN_CENTERLINE_IMPORT_ICD` | 从 `.icd` 文件导入道路中线实体，支持 `5/6` 不完整缓和曲线。 | `docs/business/alignment/道路中线_ICD导入导出.md` |
| `RD_ALIGN_CENTERLINE_EDIT_HANDLE` | 内部双击 Bridge 命令，按 handle 打开 WPF 道路中线编辑窗口。 | `docs/business/alignment/道路中线_属性编辑.md` |
| `RD_ALIGN_APPLY_DIALOG_FILE` | 内部 WPF Bridge 回写命令，应用道路中线对话框响应文件。 | `docs/business/alignment/道路中线_WPF桥接回写.md` |

## Ribbon

- AutoCAD Ribbon 位置：`RoadProto` 选项卡 / `平面设计` 面板。
- 按钮：`平面布线`、`编辑平曲线参数`、`导出中线 ICD`、`导入中线 ICD`。
- 图标：托管 Ribbon 插件内置小型道路中线图标，尺寸与数模按钮一致。

## 边界

领域层负责回旋线、平曲线、桩号、派生切线长和属性数据；ObjectARX 适配层负责点取 Jig 预览、实体绘制、DWG 持久化、夹点、请求/响应文件 Bridge 和内部应用命令；WPF 只负责道路中线属性、平曲线参数和桩号设置界面；托管 Ribbon 插件创建按钮、双击转发和 WPF 命令入口。

## 功能文档索引

| 功能 | 文档 |
| --- | --- |
| 道路中线总览 | `docs/business/alignment/平面布线_道路中线.md` |
| 道路中线创建 | `docs/business/alignment/平面布线_道路中线创建.md` |
| 道路中线属性编辑 | `docs/business/alignment/道路中线_属性编辑.md` |
| 平曲线参数编辑 | `docs/business/alignment/平曲线参数编辑.md` |
| WPF 桥接回写 | `docs/business/alignment/道路中线_WPF桥接回写.md` |
| 夹点动态编辑 | `docs/business/alignment/道路中线_夹点动态编辑.md` |
| 元素链与不完整缓和曲线 | `docs/business/alignment/平面线形_元素链与不完整缓和曲线.md` |
| ICD 导入导出 | `docs/business/alignment/道路中线_ICD导入导出.md` |

模块文档只记录模块职责、代码落点和文档索引。每个独立功能的操作流程、输入输出和业务规则以对应业务文档为准。

## V0.1.8 最终代码落点

| 层 | 路径 | 职责 |
| --- | --- | --- |
| 领域层 | `src/domain/alignment` | 回旋线公式、五单元平曲线构建、连续控制点链、道路等级、桩号格式化和参数派生；不得依赖 ObjectARX。 |
| 应用层 | `src/application/alignment` | 组织平面线形创建和参数更新的用例入口，保持业务流程可复用。 |
| 模块层 | `src/modules/alignment` | 通过 `ModuleRegistry` / `CommandRegistry` 注册 `ALIGNMENT` 模块和 `RD_ALIGN_` 命令元数据。 |
| CAD 适配层 | `src/cad_adapter/objectarx/alignment` | ObjectARX 点取、Jig 实时预览、自定义实体绘制、DWG 持久化、夹点动态预览、双击 handle 编辑和 WPF Bridge 回写。 |
| UI 层 | `src/ui/wpf/RoadProto.Terrain.UI` | Ribbon 按钮、双击事件转发、道路中线属性窗口、平曲线参数页和桩号标注设置窗口。 |
| 测试 | `tests/core_tests.cpp` | 不依赖 AutoCAD 的平曲线构建、夹点编辑、参数保护、连续控制点链、桩号和命令元数据测试。 |

## 代码文档维护规则

- 新增命令时同步更新本文件的命令表和功能文档索引。
- 新增领域能力时同步更新代码落点表和 `docs/reuse/capability_catalog.md`。
- 具体操作流程、输入输出和业务规则不得写在模块文档中，应写入对应功能业务文档。
