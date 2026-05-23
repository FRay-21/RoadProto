# 路面结构层模板_WPF桥接回写

## 基本信息

- 功能名称：路面结构层模板 WPF 桥接回写
- 所属模块：横断面设计
- 命令名称：`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE`
- 对应代码入口：`src/cad_adapter/objectarx/cross_section/ObjectArxPavementLayerTemplateCommand.cpp`
- 业务文档维护人：RoadProto 项目
- 原型版本：`v0.1.23`
- 是否可复用：部分可复用

## 功能背景

RoadProto 当前采用 C++ ObjectARX 核心和 WPF UI 解耦架构。路面结构层模板窗口由 WPF 负责参数展示、编辑、预览和 `.rpavement.xml` 导入导出，最终结果通过响应文件交回 C++，由 C++ 完成实体创建、类型校验、领域归一化和 DWG 回写。

## 业务目标

- 从 WPF 响应文件读取用户确认、取消、目标 handle、插入点和模板数据。
- 在 C++ 侧创建新 `DnPavementLayerTemplateEntity` 或更新既有实体。
- 统一校验结构层类型、RGB 颜色、厚度模式、内外侧厚度、加宽和坡度参数。
- 统一传递模板显示方式、每层填充类型、填充角度和填充比例；C++ 侧继续通过领域规则归一化非法显示方式、填充名、角度或比例。
- 统一传递结构代号、路基干湿类型、路面类型、路基土组、设计弯沉和累计轴次；这些字段仅作为模板通用数据保存，暂不驱动预览标注、DWG 模板实体尺寸文字或道路模型结构层显示。
- 保持 WPF 与 ObjectARX 解耦，WPF 不直接操作 `AcDbEntity`、`AcDbObjectId` 或 `ads_name`。

## 输入条件

- CAD 选择对象：无直接选择，由响应文件中的 handle 定位。
- 用户输入参数：WPF 写出的响应文件路径。
- 已有设计实体：可选既有 `DnPavementLayerTemplateEntity`。
- 外部数据：WPF 响应临时文件；XML 导入导出已在 WPF 内完成，不由该命令直接读取 `.rpavement.xml`。

## 输出结果

- CAD 图形实体：新建或更新 `DnPavementLayerTemplateEntity`
- 领域实体：`PavementLayerTemplateData`
- 表格或报告：无
- 更新通知或重建请求：当前版本不自动触发引用方重建。

## 操作流程

1. WPF 窗口将当前模板数据写入响应文件。
2. WPF 发送 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE <responsePath>`。
3. C++ 桥接命令读取响应文件。
4. 如果 `accepted=false`，命令直接返回，不创建或更新实体。
5. 如果 `handle` 为空，命令创建新的 `DnPavementLayerTemplateEntity` 并写入插入点和模板数据。
6. 如果 `handle` 非空，命令解析 handle、校验实体类型并更新原实体。
7. 更新前调用 `PavementLayerTemplateRules` 做归一化；失败时不写入半成品数据。

## 桥接字段

- 模板字段：模板名称、显示比例、预览宽度、显示方式、插入点、显示全部通用参数标记、结构代号、路基干湿类型、路面类型、路基土组、设计弯沉、累计轴次。
- 层级字段：层数、层类型、层名、RGB 颜色、填充类型、填充角度、填充比例、等厚标志、单一厚度、内侧厚度、外侧厚度、内侧加宽、外侧加宽、内侧坡度、外侧坡度。
- 结构层类型编码对应 WPF 下拉显示：上面层、中面层、下面层、基层、底基层、垫层。
- 内侧 = closer to road centerline for the subgrade component；外侧 = farther from road centerline。

## 关键业务规则

- 响应文件采用 UTF-8 key-value 文本格式。
- 等厚标志为真时读取单一厚度；等厚标志为假时读取内外侧厚度。
- 加宽和坡度的一致/分侧是 WPF 编辑交互；桥接文件仍写出归一后的内侧值和外侧值，C++ 侧按内外值计算结构层几何。
- RGB 颜色随每一层写出为 `colorR/colorG/colorB`，C++ 回写后由 `DnPavementLayerTemplateEntity`、道路模型结构层填充面/边线和查看横断面预览共同使用；缺少颜色的旧数据按默认层号色补齐。
- 显示方式写出为 `displayMode`，每层填充写出为 `layer.N.hatchPattern`、`layer.N.hatchAngle` 和 `layer.N.hatchScale`；WPF 预览和 `DnPavementLayerTemplateEntity` 按该模式显示，`DnRoadModelEntity` 仍保持颜色模式。
- 高级通用参数写出为 `showAllGeneralParameters`、`structureCode`、`subgradeMoistureTypes`、`pavementType`、`subgradeSoilGroups`、`designDeflection` 和 `cumulativeAxleLoads`；多选字段用分号连接稳定枚举编码。C++ 只做保存、归一化和回写，不把这些字段转成 CAD 标注或道路模型图形。
- 加宽允许为负值，表示当前层顶边相对上一层底边所在直线缩短；除第一层外，当前层顶边必须沿上一层底边所在直线平行/共线延长或收回，保持四边形/梯形。坡度允许为负值，负坡度表示从顶边向下到底边时当前层侧边向内收，正坡度表示向外放。非有限数值、非法类型编码和非正厚度必须拒绝。
- 桥接层只做数据转换和调用转发，不实现结构层业务算法。
- `.rpavement.xml` 是 WPF 模板流转格式；本桥接命令只读取 WPF 的对话框响应文件。

## 可复用性说明

- 可复用内容：请求/响应字段、C++ 回写命令结构、实体创建/更新流程
- 临时原型内容：临时文件 Bridge 和命令行路径传递
- 正式复用前需要改造的内容：进程内 Bridge、统一错误码、模板引用关系通知

## 与其他模块的依赖关系

- 上游模块：WPF 路面结构层模板窗口
- 下游模块：`DnPavementLayerTemplateEntity`、路基模板引用和道路模型生成
- 实体联动行为：当前版本不自动触发上游/下游联动。

## 后续对接正式 EICAD 功能的注意事项

- 后续可将文件桥接替换为托管/非托管直接调用。
- 后续应提供更细的字段级错误提示，定位具体层号和参数名。
