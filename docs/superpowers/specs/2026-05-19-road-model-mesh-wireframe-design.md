# 道路模型网格线框重新定义设计

## 背景

当前道路模型以 `componentLines` 和 `slopeLines` 保存三维部件边界纵向线。它能表达“某个部件边界沿路线方向的线”，但不能表达目标图中的断面横向骨架、外侧封边、端面封闭和节点数量变化时的三角/四边形过渡。

本次调整把道路模型第一层语义改为“采样断面节点链”，再由断面节点链派生显示线框。旧纵向部件线保留，用于兼容旧 DWG 和已有预览逻辑。

## 目标效果

- 每个采样桩号都生成一条横向断面骨架线。
- 相邻采样断面之间按节点连接，形成三角形或四边形网格线框。
- 左右侧最外侧边坡点沿路线方向闭合连接，形成连续外侧封边。
- 模型起点、终点、模板断开处生成端面封闭线。
- 前后断面节点数量不一致时，通过累计断面长度归一化插值，生成过渡连接线。
- 路基/道路主体颜色保持现有规则。
- 边坡相关线框颜色使用边坡模板部件自带颜色，不额外按填方/挖方强制改色。

## 数据定义

新增 `RoadModelSectionNode`：

- `kind`：`Subgrade`、`Slope`、`Ground`
- `side`：左侧或右侧
- `offset`：相对道路中线偏距，左正右负
- `elevation`：节点高程
- `point`：三维世界坐标
- `color`：显示颜色
- `label`：用于查看横断面和调试

新增 `RoadModelSection`：

- `station`
- `leftNodes`
- `rightNodes`
- `succeeded`
- `errorMessage`

新增 `RoadModelWireLine`：

- `kind`：`SectionRib`、`Longitudinal`、`OuterBoundary`、`Transition`、`EndCap`
- `side`
- `color`
- `points`

`RoadModelData` 新增：

- `sections`
- `wireLines`

旧字段继续保留：

- `componentLines`
- `slopeLines`
- `sampledStations`

## 生成规则

1. 对每个采样桩号独立生成左右两侧断面节点链。
2. 断面链先加入路基模板边界节点，再从路基最外侧按边坡模板生成边坡节点。
3. 断面横向骨架线由同一侧节点顺序连接，生成 `SectionRib`。
4. 相邻断面如果节点数量一致，按节点序号生成 `Longitudinal`。
5. 相邻断面如果节点数量不一致，计算每条断面链的累计长度参数，把两侧节点参数合并后插值生成过渡连接，生成 `Transition`。
6. 相邻断面的最后节点始终生成 `OuterBoundary`。
7. 第一条和最后一条有效断面生成 `EndCap`；中间如果前后某侧断面缺失，也在断开处生成 `EndCap`。

## 绘制规则

`DnRoadModelEntity` 优先绘制 `wireLines`。如果旧图纸没有 `wireLines`，继续绘制 `componentLines` 和 `slopeLines`。

颜色规则：

- 路基相关线使用路基模板部件颜色。
- 边坡相关线使用对应边坡模板部件颜色。
- 由两个不同颜色节点形成的过渡线，使用外侧或当前边坡节点颜色；若无法判断则使用后一断面节点颜色。

## 查看横断面

查看横断面优先读取 `sections` 中与当前桩号匹配的节点链，直接输出二维偏距-高程预览；旧图纸没有 `sections` 时回退到现有由纵向线插值的逻辑。

## 兼容策略

- `DnRoadModelEntity` 持久化版本升级。
- 旧版本 DWG 读取后 `sections` 和 `wireLines` 为空，绘制和预览回退旧逻辑。
- 新版本生成时同时保留旧 `componentLines/slopeLines`，降低对现有测试和查看逻辑的冲击。

## 测试要点

- 生成结果包含每个采样桩号的断面节点链。
- 每个有效采样断面都有 `SectionRib`。
- 最外侧节点生成连续 `OuterBoundary`。
- 节点数量不一致的相邻断面生成 `Transition`。
- 边坡线颜色继承模板部件颜色。
- `DnRoadModelEntity` 源码契约包含新字段持久化和优先绘制 `wireLines`。
