# 平交口模块

## 模块编码

`INTERSECTION`

## 模块目的

为平交口设计原型流程预留模块位置，后续可扩展渠化、导流岛、转角、标线等能力。

## V0.1 命令

| 命令 | 说明 | 业务文档 |
| --- | --- | --- |
| `RD_INTERSECTION_INFO` | 输出平交口模块框架状态。 | `docs/business/intersection/平交口模块框架说明.md` |

## Ribbon

分组标题：`Intersection`

## 后续说明

- 增加平交口领域对象后，再增加几何生成命令。
- 平交口对路线、纵断面的依赖关系应通过 `EntityRelationManager` 表达。
- 将可复用渠化规则和项目专用验证代码分开沉淀。
