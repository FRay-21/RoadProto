# 命令与模块规则

## 命令前缀

- `RD_TERRAIN_xxx`：地形数模。
- `DN_TERRAIN_xxx`：地形数模构网和数模对象原型命令。当前保留用于地形构网功能，与用户指定命令名兼容。
- `RD_ALIGN_xxx`：平面设计。
- `RD_INTERCHANGE_xxx`：立交设计。
- `RD_INTERSECTION_xxx`：平交口设计。
- `RD_PROFILE_xxx`：纵断面设计。
- `RD_SECTION_xxx`：横断面设计。
- `RD_DRAWING_xxx`：出图、出表、算量。
- `RD_UTIL_xxx`：辅助功能。

## 模块元数据

每个模块必须提供：

- 模块名称。
- 模块编码。
- 模块说明。
- 初始化函数。
- 卸载函数。
- 注册命令函数。
- 注册 Ribbon 菜单函数。
- 模块业务文档路径。

## 命令元数据

每个命令必须提供：

- 命令内部名。
- 命令显示名。
- 所属模块编码。
- 功能说明。
- 命令入口函数。
- 是否为原型功能。
- 是否具备复用价值。
- 对应业务文档路径。
- 是否可挂接 Ribbon。

## 禁止模式

- 在任意 `.cpp` 文件中直接注册 AutoCAD 命令。
- 在命令回调中堆业务算法。
- 在 UI 事件中堆业务算法。
- 模块之间为了联动更新直接互相调用。
- 在 `domain` 中包含 ObjectARX 头文件。

## Ribbon 更新规则

新增可挂接 Ribbon 的命令时，必须同步更新两层 Ribbon：

1. C++ 框架层：模块的 `registerRibbon` 必须调用 `RibbonModel::ensurePanel`，命令元数据必须设置 `ribbonAttachable = true`。
2. AutoCAD 可见层：当前由托管插件 `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs` 创建可见 Ribbon，新增按钮必须在该文件中添加面板、按钮、命令发送和图标。
3. 按钮尺寸沿用现有 `RibbonItemSize.Standard`、`Orientation.Horizontal`，同一面板内保持一致。
4. 图标可以先用托管插件内置矢量图标；若使用 PNG，路径应记录在 `assets/icons/README.md`，命名优先使用命令名。
5. 模块文档必须说明 Ribbon 位置、按钮显示名、命令名和图标策略。
