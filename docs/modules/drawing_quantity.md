# 出图、出表、算量模块

## 模块信息

- 模块名称：出图、出表、算量
- 模块编码：`DRAWING_QUANTITY`
- 命令前缀：`RD_DRAWING_`
- 当前状态：已实现从横断面落图生成路面工程量统计表的原型命令；当前版本优先从横断面图实体当前保存的结构层面域采样，图上夹点修改后的尺寸会进入工程量统计。

## 命令清单

| 命令 | 显示名称 | 类型 | 文档 |
| --- | --- | --- | --- |
| `RD_DRAWING_PAVEMENT_QUANTITY_TABLE` | 路面工程量统计表 | 用户命令 | `docs/business/drawing_quantity/路面工程量统计表.md` |

## Ribbon

- C++ Ribbon model：`RoadProto / 出图出表 / 路面工程量统计表`
- 可见 AutoCAD WPF Ribbon：`RoadProto / 出图出表 / 路面工程量统计表`
- 托管 Ribbon 插件文件：`src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- 图标策略：当前使用托管插件内置矢量图标，后续如替换 PNG，应在 `assets/icons/README.md` 记录资源路径。

## 代码落点

| 层 | 代码 | 职责 |
| --- | --- | --- |
| domain | `src/domain/quantity/PavementQuantityTable.*`、`src/domain/quantity/RoadModelPavementQuantitySampler.*`、`src/domain/quantity/PavementQuantityDrawingFaceSampler.*` | 构造物切段、横断面图实体当前面域采样、旧道路模型部件名反推、部件/结构层双模式聚合、平面投影面积、平均断面法体积和带格式 SpreadsheetML `.xls` 写出 |
| modules | `src/modules/drawing_quantity/DrawingQuantityModule.*` | 模块、命令和 C++ Ribbon 元数据注册 |
| startup | `src/app/startup/DrawingQuantityStartupRegistration.*` | 启动期注册 `DRAWING_QUANTITY` 模块 |
| cad_adapter | `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.*` | 横断面图选择、道路模型读取、输出路径提示和表格生成命令入口 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs` | AutoCAD 可见 Ribbon 出图出表入口 |

## 边界

- `domain/quantity` 不依赖 ObjectARX。
- CAD 选择、道路模型实体读取和文件保存路径提示只在 `cad_adapter/objectarx` 中完成。
- 第一版表格导出为 Excel 可打开的 SpreadsheetML `.xls`，不依赖 Excel COM。
- 统计结果是一次性导出快照，道路模型或横断面图变化后需要重新运行命令。
- 若横断面图中存在配置生成或用户夹点修改后的结构层面域，统计命令以横断面图实体当前面域为准；只有缺少可用面域时才回退道路模型断面数据。
