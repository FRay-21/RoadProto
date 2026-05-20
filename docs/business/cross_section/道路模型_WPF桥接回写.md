# 道路模型_WPF桥接回写

## 基本信息

- 功能名称：道路模型 WPF 桥接回写
- 所属模块：横断面设计
- 命令名称：`RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE`
- 对应代码入口：`src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- 业务文档维护人：RoadProto 项目
- 原型版本：`v0.1.19`
- 是否可复用：部分可复用

## 功能背景

RoadProto 当前采用 C++ ObjectARX 核心和 WPF UI 解耦架构。道路模型窗口由 WPF 负责参数展示和表格编辑，最终结果通过临时响应文件交回 C++ ObjectARX 命令，由 C++ 完成 CAD 实体读取、构建和回写。

## 业务目标

- 用请求文件把道路模型配置传给 WPF。
- 用响应文件从 WPF 回传采样间距、路基模板范围表、左右边坡模板组、搜索宽度、目标道路模型 handle 和点选模板动作。
- 在 C++ 侧统一校验 handle、读取 CAD 实体、调用应用服务并创建或更新 `DnRoadModelEntity`。
- 创建或更新道路模型时一并保存生成采样桩号、断面节点链、结构层线、地面剖面快照和三维网格线框，供 CAD 显示和 `查看横断面` 读取。
- 当 WPF 请求点选某一行路基模板时，由 C++ 侧提示选择 `DnSubgradeTemplateEntity` 并重新打开窗口。
- 当 WPF 请求点选某一侧模板组的边坡模板时，由 C++ 侧提示选择 `DnSlopeTemplateEntity` 并追加到对应模板组。
- WPF 可在本地对已回传的模板组内模板进行删除、上移、下移和清空；需要新增模板实体时仍通过点选动作回到 C++。

## 适用场景

适用于 `RD_SECTION_ROAD_MODEL_CREATE` 和 `RD_SECTION_ROAD_MODEL_EDIT` 打开的 WPF 道路模型窗口。该命令为内部桥接命令，不挂接 Ribbon。

## 输入条件

- CAD 选择对象：无直接选择，由响应文件中的 handle 定位
- 用户输入参数：WPF 写出的响应文件路径；点选路基模板动作中的行号；点选边坡模板动作中的侧别和模板组序号
- 已有设计实体：道路中线、竖曲线、路基模板、边坡模板、可选地形 TIN 和可选道路模型实体
- 外部数据：WPF 响应临时文件

## 输出结果

- CAD 图形实体：新建或更新 `DnRoadModelEntity`
- 领域实体：`RoadModelData`，包含三维部件线、结构层线、断面节点链、网格线框和采样桩号
- 表格或报告：无
- 更新通知或重建请求：当前版本不自动接入统一关系重建

## 操作流程

1. WPF 窗口将用户配置写入响应文件。
2. WPF 发送 `RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE <responsePath>`。
3. C++ 桥接命令读取并解析响应文件。
4. 如果响应动作是 `pickTemplate`，命令提示用户选择路基模板实体，把选中模板写入对应行并重新打开 WPF 窗口。
5. 如果响应动作是 `pickLeftSlopeTemplate` 或 `pickRightSlopeTemplate`，命令提示用户选择边坡模板实体，把选中模板追加到对应侧模板组并重新打开窗口。
6. 如果用户取消且不是点选模板动作，命令直接返回。
7. 生成模型时，命令解析道路中线、竖曲线、路基模板、路面结构层模板、边坡模板、道路模型 handle 和可用 TIN 地面数据。
8. 命令校验竖曲线所属拉坡图关联当前道路中线。
9. 命令调用 `RoadModelBuildService` 构建道路模型数据，并把领域层进度回调转接到 AutoCAD 状态栏进度条。
10. 响应中无道路模型 handle 时创建新实体，有 handle 时更新原实体。

## 关键业务规则

- 响应文件采用 UTF-8 文本和 key-value 格式，路径由 WPF 生成。
- 响应文件通过 `action=pickTemplate` 和 `pickAssignmentIndex` 表达“当前行点选路基模板”请求。
- 响应文件通过 `action=pickLeftSlopeTemplate` / `pickRightSlopeTemplate` 和 `pickSlopeGroupIndex` 表达“当前侧模板组点选边坡模板”请求。
- 模板组内模板删除、上移、下移和清空不需要额外 Bridge 动作，WPF 直接写回最新模板组列表。
- `assignmentCount` 受桥接层上限保护，避免异常响应文件写坏内存。
- 采样间距、桩号范围、搜索宽度和模板 handle 由 C++ application/domain 层再次校验。
- 路基模板部件保存路面结构层模板 handle 时，C++ 侧负责读取 `DnPavementLayerTemplateEntity` 并传入道路模型构建服务。
- 点选模板过程中允许当前表格还不满足生成校验，只有点击 `生成模型` 时才执行完整构建校验。
- 竖曲线与道路中线不匹配时必须拒绝构建。
- 构建失败时不创建半成品道路模型实体。

## 可复用性说明

- 可复用内容：道路模型 DTO、请求/响应文件桥接、C++ 回写命令结构
- 临时原型内容：临时文件 Bridge 和命令行路径传递
- 正式复用前需要改造的内容：进程内 Bridge、模板库选择器、统一错误码和日志

## 与其他模块的依赖关系

- 上游模块：WPF 横断面戴帽窗口、平面设计、纵断面设计、路基模板
- 下游模块：道路模型实体和后续三维/出图/算量功能
- 实体联动行为：当前版本不自动触发上游/下游联动

## 后续对接正式 EICAD 功能的注意事项

- 后续可将文件桥接替换为更正式的托管/非托管调用通道。
- 需要增加更细的错误提示，使用户能定位是哪一行模板范围、哪个点选结果或哪个 handle 无效。
