# 道路模型_WPF桥接回写

## 基本信息

- 功能名称：道路模型 WPF 桥接回写
- 所属模块：横断面设计
- 命令名称：`RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE`
- 对应代码入口：`src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- 业务文档维护人：RoadProto 项目
- 原型版本：`v0.1.12`
- 是否可复用：部分可复用

## 功能背景

RoadProto 当前采用 C++ ObjectARX 核心和 WPF UI 解耦架构。道路模型窗口由 WPF 负责参数展示和表格编辑，最终结果通过临时响应文件交回 C++ ObjectARX 命令，由 C++ 完成 CAD 实体读取、构建和回写。

## 业务目标

- 用请求文件把道路模型配置传给 WPF。
- 用响应文件从 WPF 回传采样间距、模板范围表和目标道路模型 handle。
- 在 C++ 侧统一校验 handle、读取 CAD 实体、调用应用服务并创建或更新 `DnRoadModelEntity`。

## 适用场景

适用于 `RD_SECTION_ROAD_MODEL_CREATE` 和 `RD_SECTION_ROAD_MODEL_EDIT` 打开的 WPF 道路模型窗口。该命令为内部桥接命令，不挂接 Ribbon。

## 输入条件

- CAD 选择对象：无直接选择，由响应文件中的 handle 定位
- 用户输入参数：WPF 写出的响应文件路径
- 已有设计实体：道路中线、竖曲线、路基模板和可选道路模型实体
- 外部数据：WPF 响应临时文件

## 输出结果

- CAD 图形实体：新建或更新 `DnRoadModelEntity`
- 领域实体：`RoadModelData`
- 表格或报告：无
- 更新通知或重建请求：当前版本不自动接入统一关系重建

## 操作流程

1. WPF 窗口将用户配置写入响应文件。
2. WPF 发送 `RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE <responsePath>`。
3. C++ 桥接命令读取并解析响应文件。
4. 如果用户取消，命令直接返回。
5. 命令解析道路中线、竖曲线、路基模板和道路模型 handle。
6. 命令校验竖曲线所属拉坡图关联当前道路中线。
7. 命令调用 `RoadModelBuildService` 构建道路模型数据。
8. 响应中无道路模型 handle 时创建新实体，有 handle 时更新原实体。

## 关键业务规则

- 响应文件采用 UTF-8 文本和 key-value 格式，路径由 WPF 生成。
- `assignmentCount` 受桥接层上限保护，避免异常响应文件写坏内存。
- 采样间距、桩号范围和模板 handle 由 C++ application/domain 层再次校验。
- 竖曲线与道路中线不匹配时必须拒绝构建。
- 构建失败时不创建半成品道路模型实体。

## 可复用性说明

- 可复用内容：道路模型 DTO、请求/响应文件桥接、C++ 回写命令结构
- 临时原型内容：临时文件 Bridge 和命令行路径传递
- 正式复用前需要改造的内容：进程内 Bridge、模板选择器、统一错误码和日志

## 与其他模块的依赖关系

- 上游模块：WPF 横断面戴帽窗口、平面设计、纵断面设计、路基模板
- 下游模块：道路模型实体和后续三维/出图/算量功能
- 实体联动行为：当前版本不自动触发上游/下游联动

## 后续对接正式 EICAD 功能的注意事项

- 后续可将文件桥接替换为更正式的托管/非托管调用通道。
- 需要增加更细的错误提示，使用户能定位是哪一行模板范围或哪个 handle 无效。
