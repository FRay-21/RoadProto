# 路面结构层模板_WPF桥接回写

## 功能边界

`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE` 用于读取 WPF 写出的路面结构层模板响应文件，并在 CAD 中创建或更新模板实体。

## 响应语义

- `accepted=false`：取消操作，不创建或更新实体。
- `handle` 为空：创建新的 `DnPavementLayerTemplateEntity`。
- `handle` 非空：解析并更新既有路面结构层模板实体。

## 桥接字段

桥接文件使用 key-value 文本格式，包含模板名称、显示比例、预览宽度、插入点、层数以及每层类型、名称、等厚/非等厚厚度、内外侧加宽和内外侧坡度字段。
