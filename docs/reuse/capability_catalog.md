# 复用能力目录

## 通用 CAD 能力

| 能力 | 当前状态 | 源码 |
| --- | --- | --- |
| 编辑器消息输出 | V0.1 ObjectARX 适配器 | `src/cad_adapter/objectarx/ObjectArxEditor.*` |
| 命令注册 | V0.1 ObjectARX 适配器 | `src/cad_adapter/objectarx/ObjectArxCommandRegistrar.*` |
| 选择集释放 guard | 预留辅助类 | `src/cad_adapter/objectarx/ObjectArxSelectionSetGuard.h` |
| 事务作用域 | 接口占位 | `src/cad_adapter/transaction/TransactionScope.h` |
| 图层规格 | 接口占位 | `src/cad_adapter/layer/LayerService.h` |
| 文字标注规格 | 接口占位 | `src/cad_adapter/annotation/TextAnnotationService.h` |
| 块插入规格 | 接口占位 | `src/cad_adapter/block/BlockInsertService.h` |
| ObjectARX 地形对象提取 | V0.1.6 原型，支持连续点选样例、按同图层同类型扫描模型空间、状态栏进度和按类隐藏源对象 | `src/cad_adapter/objectarx/terrain/ObjectArxTerrainTinCommand.cpp` |
| 地形构网参数确认窗口 | V0.1.6 历史过渡实现，C++ Win32 对话框，当前原型阶段后续重做需迁移为 WPF | `src/cad_adapter/objectarx/terrain/TerrainTinCreateDialog.*` |
| 地形 TIN 自定义实体 | V0.1.6 原型，支持 TrueColor 渐变边线高程着色、边界显示、DWG 持久化、RMesh 流转和双击编辑 handle 入口 | `src/cad_adapter/objectarx/terrain/DnTerrainTinEntity.*` |
| AutoCAD 可见 Ribbon 托管插件 | V0.1.8 原型，创建带小图标且尺寸一致的 `RoadProto` / `数模` / `平面设计` Ribbon 入口，并转发数模和道路中线双击事件 | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs` |

## 通用道路设计能力

| 能力 | 当前状态 | 源码 |
| --- | --- | --- |
| 设计实体唯一 ID | V0.1 已实现 | `src/domain/common/EntityId.*` |
| 实体依赖关系 | V0.1 已实现 | `src/domain/relation/EntityRelationManager.*` |
| 脏标记与重建请求 | V0.1 已实现 | `src/domain/relation/DesignEntity.h` |
| 文字高程解析 | V0.1.3 已实现 | `src/domain/terrain/TerrainTextElevationParser.*` |
| 地形点清洗与文字-点关联 | V0.1.3 已实现 | `src/domain/terrain/TerrainPointNormalizer.*` |
| Delaunay / TIN 构建 | V0.1.3 已实现，基于 `delaunator-cpp` | `src/domain/terrain/TerrainTinBuilder.*` |
| 第三方 Delaunay 头文件 | V0.1.3 已引入，MIT License | `third_party/delaunator-cpp/` |
| TIN 高程查询 | V0.1.3 已实现 | `src/domain/terrain/TerrainSurfaceQuery.*` |
| RMesh 地形数模文件读写 | V0.1.6 已实现，支持 `.rmesh` 导入导出和跨 DWG 流转 | `src/domain/terrain/TerrainMeshFile.*` |
| 回旋线与平曲线五单元构建 | V0.1.8 原型，支持主点切线长、旧切线长保护、直线-缓和曲线-圆曲线-缓和曲线-直线构建和采样 | `src/domain/alignment/HorizontalAlignmentBuilder.*` |
| 平面线形元素链与不完整缓和曲线 | V0.1.8 扩展，支持圆曲线、有限半径到有限半径的不完整缓和曲线、正负曲率过渡和元素链采样 | `src/domain/alignment/AlignmentElementChainBuilder.*` |
| ICD 道路中线文件读写 | V0.1.8 扩展，支持积木法 `.icd` 直线、圆曲线、完整缓和曲线、不完整缓和曲线读写、工程坐标/CAD 坐标转换和元素链转换 | `src/domain/alignment/IcdAlignmentFile.*` |
| 道路中线夹点编辑 | V0.1.8 原型，按夹点索引移动控制点或调整曲线参数，并在领域层重建五单元平曲线 | `src/domain/alignment/AlignmentGripEditService.*` |
| 桩号格式化 | V0.1.7 原型，支持 `K0+000`、`K0+100` 等道路桩号文本 | `src/domain/alignment/StationFormatter.*` |
| 道路中线自定义实体 | V0.1.8 原型，支持 DWG 持久化、分元素着色、引线桩号、特征点标注、参数标注和参数夹点动态预览，特征点不额外绘制小十字 | `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.*` |
| 道路中线 WPF 参数窗口 | V0.1.8 原型，通过请求/响应文件 Bridge 编辑道路属性、数模关联、桩号间距和平曲线参数 | `src/ui/wpf/RoadProto.Terrain.UI/AlignmentCenterlineWindow.xaml` |

## 模块专用能力

| 能力 | 当前状态 | 源码 |
| --- | --- | --- |
| 地形更新后的联动标记 | V0.1.6 保留的历史框架示例，不代表当前 TIN 实体已自动监听源对象修改 | `src/application/terrain/TerrainUpdateSampleService.*` |
| 地形构网流程服务 | V0.1.6 原型 | `src/application/terrain/TerrainTinCreateService.*` |
| 平交口模块注册 | V0.1 示例 | `src/modules/intersection/IntersectionModule.*` |
| 平面设计模块注册 | V0.1.8 原型，注册平面布线、编辑平曲线参数、ICD 导入导出、双击 handle 编辑命令和 WPF 回写内部命令，并按功能文档分配业务文档路径 | `src/modules/alignment/AlignmentModule.*` |

## 临时原型能力

| 能力 | 当前状态 | 后续处理 |
| --- | --- | --- |
| 固定示例实体 ID | `RD_TERRAIN_MARKDIRTY` 使用 | 后续替换为持久化 ID 和 DWG 元数据 |
| WPF 地形构网 UI Bridge 预留 | `RoadProto.Terrain.UI` 已编译，并提供可见 Ribbon 和双击事件转发；参数窗口仍由 C++ 对话框承接 | 后续用 C++/CLI 或 AutoCAD .NET Bridge 接入完整 WPF 参数窗口 |

## V0.1.8 平面布线最终复用边界

- `HorizontalAlignmentBuilder` 是道路中线几何核心能力，支持单交点和多交点控制点链、`LS1/LS2` 不等长、`T1/T2` 派生和无效平曲线参数拒绝。
- `AlignmentGripEditService` 是道路中线夹点编辑核心能力，封装控制点移动和特征点参数调整，不依赖 ObjectARX。
- `StationFormatter` 是桩号文本能力，当前用于 `K0+000`、`K0+100` 等道路桩号格式化。
- `DnRoadCenterlineEntity` 是 CAD 持久化和显示能力，负责分元素着色、引线桩号、曲线参数标注、特征点标注、DWG 保存重开和参数夹点动态预览。
- `AlignmentDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递道路属性、数模 handle、桩号间距和多组平曲线参数。
- 后续控制点实时预览、隐藏正式实体、防止旧实体短直线残留、处理 `Enter`/右键结束点取等行为属于 ObjectARX Adapter 层能力，不进入 WPF 业务逻辑。
- 数模关联当前沉淀为 handle 关联能力；WPF 不直接读取自定义实体类型，后续高程查询由 C++ Adapter/Service 接管。
