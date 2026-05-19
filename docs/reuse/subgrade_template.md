# 路基模板复用说明

## 能力范围

路基模板能力沉淀在 `domain/cross_section`、`application/cross_section` 和 `cad_adapter/objectarx/cross_section` 中，用于表达道路横断面中线两侧的部件组成、默认参数、颜色、变宽表、坡度变化数据表和路面结构层关联预留字段。

## 可复用单元

- `SubgradeTemplateModel`：道路等级、部件类型、左右侧、坡度模式、颜色、桩号表和模板数据模型。
- `SubgradeTemplateDefaults`：高速公路、城市快速路以及二级、三级、四级道路默认模板。
- `SubgradeTemplateRules`：显示比例检查、宽度加宽计算、坡度查询、变化坡度固定值隔离和路面结构层厚度启用规则。
- `SubgradeTemplateCreateService`：创建命令默认数据生成。
- `DnSubgradeTemplateEntity`：CAD 中的独立路基模板实体显示、DWG 持久化和插入点夹点移动。
- `SubgradeTemplateDialogBridge`：C++ 与 WPF 参数窗口之间的请求/响应文件桥接。

## 复用边界

- 几何和业务参数规则放在 C++ domain/application，后续替换 WPF、MFC、Qt 或正式 EICAD UI 时仍可复用。
- WPF 预览只用于参数编辑体验，不作为最终 CAD 绘制算法来源。
- 变宽表和坡度变化数据表当前持久化保存；变化坡度模式不回退固定坡度，待道路中线关联功能实现后，由业务服务按桩号计算实际宽度和坡度。
- `DnSubgradeTemplateEntity` 图面标注使用中文部件名；WPF 预览显示部件宽度和坡度，但预览表达仍只属于 UI 辅助能力。
- `DnSubgradeTemplateEntity` 的插入点夹点只用于移动模板图面位置，不参与模板业务参数计算。
- 路面结构层关联当前只保留句柄、启用状态和厚度字段，结构层实体实现后再接入关系管理。
