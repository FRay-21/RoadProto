# RMesh 地形数模文件读写复用说明

## 能力定位

`TerrainMeshFile` 是地形数模跨 DWG 流转的领域层复用能力，用于把已经构建完成的 `TinBuildResult` 写入 `.rmesh` 文件，并从 `.rmesh` 文件恢复为可插入 DWG 的 TIN 数据。

该能力位于 `domain/terrain`，不依赖 ObjectARX，也不直接操作 `AcDbEntity`、`AcDbObjectId` 或选择集。ObjectARX 命令层只负责选择实体、选择文件、调用读写接口和创建 `DnTerrainTinEntity`。

## 当前实现

- 源码：`src/domain/terrain/TerrainMeshFile.h`
- 源码：`src/domain/terrain/TerrainMeshFile.cpp`
- 测试：`tests/core_tests.cpp`
- 文件后缀：`.rmesh`
- 文件格式：RoadProto RMesh V1 二进制格式
- 对应功能文档：`docs/business/terrain/地形数模_RMesh导入导出.md`

RMesh V1 保存以下内容：

- 提取参数 `TinExtractOptions`
- 构网参数 `TinBuildOptions`
- 提取统计 `TinExtractSummary`
- 高程范围
- `TinPoint` 点集
- `TinTriangle` 三角形
- 边界边

## 使用约定

- 导出命令应统一把目标文件后缀修正为 `.rmesh`。
- 导入命令必须校验点和三角形数量，并拒绝没有可用三角网数据的文件。
- 读写失败必须返回清晰错误信息，不允许异常穿透到 AutoCAD 命令入口。
- RMesh 文件只保存数模结果和来源追踪信息，不保存原 DWG 对象本体。
- 导入其他 DWG 后生成新的 `DnTerrainTinEntity`，该实体继续支持 DWG 持久化、显示模式切换、双击编辑和后续查询接口。

## 已知限制

- V1 暂不保存外部 DWG 依赖关系，也不实现来源对象自动重连。
- V1 暂不做多版本格式迁移，只识别当前 RMesh 版本。
- 来源对象 ObjectId 在跨 DWG 后只作为追踪信息保留，不能保证在目标 DWG 中可解析回原对象。
