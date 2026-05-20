# 边坡模板_WPF桥接回写

## 基本信息

- 功能名称：边坡模板 WPF 桥接回写
- 所属模块：横断面设计
- 命令名称：`RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE`
- 对应代码入口：`src/cad_adapter/objectarx/cross_section/ObjectArxSlopeTemplateCommand.cpp`
- 业务文档维护人：RoadProto 项目
- 原型版本：`v0.1.14`
- 是否可复用：部分可复用

## 功能背景

RoadProto 当前采用 C++ ObjectARX 核心和 WPF UI 解耦架构。边坡模板窗口由 WPF 展示和编辑参数，最终通过临时响应文件交回 C++，由 C++ 完成模板数据校验和 CAD 实体创建或更新。

## 业务目标

- 用请求文件把边坡模板初始数据传给 WPF。
- 用响应文件从 WPF 回传模板名称、显示比例、模板类型、控制条件和部件列表。
- 在 C++ 侧统一校验参数并创建或刷新 `DnSlopeTemplateEntity`。

## 操作流程

1. C++ 写出 WPF 请求文件并发送 `RD_SECTION_SLOPE_TEMPLATE_SHOW_WPF_DIALOG`。
2. WPF 读取请求文件，打开 `边坡模板` 窗口。
3. 用户确认后，WPF 写出响应文件。
4. WPF 发送 `RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE <responsePath>`。
5. C++ 读取响应文件并调用边坡模板规则标准化数据。
6. 响应中无 handle 时创建新实体，有 handle 时更新原实体。

## 关键业务规则

- 请求和响应文件采用 UTF-8 key-value 格式。
- WPF 只做界面展示和交互，不直接读取或修改 `AcDbEntity`。
- C++ 必须重新校验显示比例、部件数量、部件类型和几何参数。
- 用户取消窗口时不创建或更新实体。

## 后续扩展

- 后续可把临时文件 Bridge 替换为更正式的托管/非托管调用通道。
- 需要补充更细的字段级错误提示，便于定位某个部件参数无效。
