# 道路中线与平曲线计算

## 能力分类

通用道路设计能力

## 能力说明

提供道路中线控制点、连续五单元平曲线链、平面线形元素链、回旋线缓和曲线、桩号格式化和道路中线自定义实体基础能力。

## 当前实现

- 源码路径：`src/domain/alignment`、`src/cad_adapter/objectarx/alignment`、`src/ui/wpf/RoadProto.Terrain.UI`
- 对外类型/函数：`HorizontalAlignmentBuilder`、`AlignmentElementChainBuilder`、`IcdAlignmentFile`、`AlignmentGripEditService`、`StationFormatter`、`DnRoadCenterlineEntity`、`AlignmentDialogBridge`
- 当前使用该能力的命令：`RD_ALIGN_CENTERLINE_CREATE`、`RD_ALIGN_CURVE_PARAM_EDIT`、`RD_ALIGN_CENTERLINE_EXPORT_ICD`、`RD_ALIGN_CENTERLINE_IMPORT_ICD`
- 对应功能文档：`docs/business/alignment/平面布线_道路中线创建.md`、`docs/business/alignment/道路中线_属性编辑.md`、`docs/business/alignment/平曲线参数编辑.md`、`docs/business/alignment/道路中线_夹点动态编辑.md`、`docs/business/alignment/平面线形_元素链与不完整缓和曲线.md`、`docs/business/alignment/道路中线_ICD导入导出.md`

## V0.1.8 最终能力边界

- 几何能力保留在 C++ 领域层：控制点链、五单元平曲线、圆曲线-不完整缓和曲线-圆曲线元素链、回旋线缓和曲线、`T1/T2` 派生和桩号格式化均不依赖 ObjectARX。
- CAD 能力保留在 ObjectARX Adapter 层：点取、Jig 实时预览、正式实体隐藏/恢复、自定义实体绘制、DWG 持久化、夹点和内部回写命令都不进入 WPF。
- UI 能力保留在 WPF 层：道路属性、道路等级、数模关联 handle、桩号标注间距和平曲线参数翻页编辑只负责展示、输入和命令转发。
- Bridge 能力当前采用请求/响应文件，接口对象应继续保持 DTO 化，便于后续替换为 C++/CLI、AutoCAD Palette、Qt 或其他正式 UI 通道。

## 可复用内容

- 回旋线公式、缓和曲线主点参数和采样。
- 单交点和多交点控制点链的五单元平曲线构建。
- 圆曲线-不完整缓和曲线-圆曲线元素链构建，支持同向卵形过渡和反向 S 型过渡。
- ICD `.icd` 文本文件读写，支持直线、圆曲线、完整缓和曲线和不完整缓和曲线单元。
- ICD 与内部道路中线之间转换时处理工程坐标和 CAD 坐标差异：ICD `x/y` 与 CAD `x/y` 互换，方位角随坐标系同步换算。
- 桩号格式化。
- 道路中线实体绘制、分元素着色、引线桩号、参数夹点动态预览与 DWG 持久化。
- WPF 道路中线属性/平曲线参数窗口通过 Bridge 调用 C++ 核心，可为后续 Palette/Qt/MFC UI 复用同一数据协议。
- 点取第三个控制点和后续控制点时，使用 ObjectARX Jig 显示道路中线自定义实体动态预览。
- 夹点拖动使用 ObjectARX `AcDbGripData` 与拖动缓存显示连续预览，释放后通过领域层夹点编辑服务正式重建道路中线。
- 交点、起点、终点等同坐标 shared grip 会在领域层按语义目标去重，避免同一个控制点被一次鼠标拖动重复移动。

## 不可复用或临时内容

- WPF 请求/响应文件 Bridge 是原型级实现，后续可替换为更正式的进程内 Bridge。
- `T1/T2` 当前作为派生值保护线形稳定，正式编辑模式需补充类似 EICAD 的等比缩放、锁定半径、锁定缓和曲线等控制方式。
- 元素链第一版只支持内部计算、采样、显示和 DWG 持久化，不提供夹点编辑、WPF 参数编辑或 Ribbon 入口。
- ICD 导入的元素链实体第一版不提供夹点编辑或 WPF 参数编辑，只支持显示、保存和再次导出。

## 依赖关系

- domain 依赖：无 ObjectARX 依赖。
- cad_adapter 依赖：ObjectARX 实体和命令。
- 模块依赖：`ALIGNMENT`。

## 扩展说明

后续可增加规范参数推荐、纵断面高程取样、路线文件流转、独立标注实体和更细的 grip 编辑模式。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- AutoCAD 手工验证：加载 ARX 后创建、编辑、保存并 `REGEN` 道路中线实体。
