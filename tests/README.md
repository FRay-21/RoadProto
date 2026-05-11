# 测试说明

`RoadProtoCoreTests.vcxproj` 用于验证 core、domain、application 和 Ribbon 模型行为，不需要加载 AutoCAD 或 ObjectARX。

运行方式：

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

当前测试覆盖：

- 命令注册元数据和重复注册检查。
- 模块注册、命令注册、Ribbon 分组注册。
- 实体依赖关系与脏标记/重建请求传播。
- 地形更新示例 use case。
- 文字高程解析。
- 地形点重复合并、Z 冲突统计、文字-点近邻赋高程。
- Delaunay / TIN 构网的正常三角化和共线失败。
- TIN 三角形查找与重心插值高程查询。
- RMesh `.rmesh` 地形数模文件的写入、读取、Unicode 元数据回读和非法文件拒绝。
- 回旋线公式、道路主点切线长、旧切线长保护、单交点五单元构建、多交点连续平曲线链构建、桩号格式化和无效平曲线参数拒绝。

V0.1.6 继续保留 `TerrainMeshFile` 领域层测试，用于保证 `DN_TERRAIN_TIN_EXPORT` / `DN_TERRAIN_TIN_IMPORT` 依赖的跨 DWG 数模文件数据不会在读写中丢失。

AutoCAD / ObjectARX 命令回归需要在本机 AutoCAD 环境中手动执行。当前 `v0.1.6` 已用 Core Console 验证 `DN_TERRAIN_TIN_CREATE` 的样例对象选择、同图层同类型提取、源对象隐藏、TIN 生成、`DN_TERRAIN_TIN_EDIT` 非 UI 编辑路径、`DN_TERRAIN_TIN_EDIT_HANDLE` 按 handle 编辑路径、`DN_TERRAIN_TIN_IMPORT` 的 `.rmesh` 导入、DWG 保存后重新打开和 `REGEN`；托管 Ribbon 插件已验证 Release 构建。完整 Ribbon 点击、导出文件对话框和真实鼠标双击弹窗需要在 AutoCAD 图形界面中人工确认。

## V0.1.8 平面布线验证范围

核心测试覆盖 `HorizontalAlignmentBuilder` 的五单元构建、连续多交点控制点链、`AlignmentGripEditService` 的夹点移动重建、交点 shared grip 去重、`LS1/LS2` 不等长时的 `T1/T2` 派生、旧切线长保护、无效参数拒绝和命令元数据注册。

核心测试同时覆盖 `AlignmentElementChainBuilder` 的圆曲线-不完整缓和曲线-圆曲线元素链、卵形同向曲率过渡、S 型正负曲率过渡和无效不完整缓和曲线拒绝。

核心测试还覆盖 `IcdAlignmentFile` 的五单元道路中线导出再导入、ICD `5` 不完整缓和曲线导入、工程坐标到 CAD 坐标和方位角的转换、导入后再次导出保持 `3/4` 完整缓和曲线类型。

AutoCAD 图形界面需要手工验证 `RD_ALIGN_CENTERLINE_CREATE`、`RD_ALIGN_CURVE_PARAM_EDIT`、`RD_ALIGN_CENTERLINE_EDIT_HANDLE`、`RD_ALIGN_APPLY_DIALOG_FILE` 和 `DnRoadCenterlineEntity`：

- 创建命令第三点后立即显示道路中线自定义实体预览，后续点取阶段无橡皮筋短直线残留。
- 后续点取阶段 `Enter`、鼠标右键和 `ESC` 均结束点取并打开 WPF 道路中线窗口。
- WPF 道路中线窗口可编辑道路属性、数模 handle 和桩号间距；平曲线参数页可按“第 x 个平曲线”翻页编辑多组参数。
- 双击道路中线实体可进入同一个 WPF 编辑窗口。
- 保存 DWG 后重开并 `REGEN`，道路中线、桩号、曲线参数标注和实体属性保持正常。
- 夹点拖动起终点、交点、直缓点、缓圆点、圆曲线中点、圆缓点和缓直点时，道路中线应跟随鼠标连续预览；释放后五单元平曲线基本样式保持稳定。
- `RD_ALIGN_CENTERLINE_EXPORT_ICD` 可将选中道路中线导出为 `.icd`。
- `RD_ALIGN_CENTERLINE_IMPORT_ICD` 可导入包含 `1/2/3/4/5/6` 单元的 `.icd` 并生成道路中线实体；ICD 起点工程坐标应正确转换为 CAD 坐标，含 `5/6` 的导入实体应显示、保存、重开和再次导出正常。
