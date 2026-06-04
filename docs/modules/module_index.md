# 模块索引

| 模块 | 编码 | 命令前缀 | V0.1 状态 | 文档 |
| --- | --- | --- | --- | --- |
| 地形数模 | `TERRAIN` | `RD_TERRAIN_` / `DN_TERRAIN_` | 示例模块、TIN 构网、数模双击编辑和 RMesh 导入导出命令已实现；业务文档已按功能拆分 | `docs/modules/terrain.md` |
| 平面设计 | `ALIGNMENT` | `RD_ALIGN_` | 道路中线原型、WPF 编辑、夹点优化、元素链和 ICD 导入导出已实现；业务文档已按功能拆分 | `docs/modules/alignment.md` |
| 立交设计 | `INTERCHANGE` | `RD_INTERCHANGE_` | 目录已预留 | 待补充 |
| 平交口设计 | `INTERSECTION` | `RD_INTERSECTION_` | 示例模块已实现 | `docs/modules/intersection.md` |
| 纵断面设计 | `PROFILE` | `RD_PROFILE_` | 纵断面拉坡图创建、DMX/数模地面线来源、自定义实体、WPF 属性编辑、竖曲线实体、夹点/PVI 编辑和 WPF 编辑入口已实现 | `docs/modules/profile.md` |
| 横断面设计 | `CROSS_SECTION` | `RD_SECTION_` | 路基模板、边坡模板和路面结构层模板独立实体已实现；路面结构层模板新建时直接套用“沥青路面-主线行车道”预设并打开 WPF 编辑窗口，保留原创建向导代码，同时支持当前层编辑、材料名称可编辑下拉推荐项、索引颜色、填充类型/角度/比例、显示全部通用参数折叠区、模板名称居中标题、`.rpavement.xml` 导入导出和路基部件绑定；道路模型横断面戴帽已接入结构层三维边界线和弱化填充面、边坡模板组、构造物范围、生成进度、断面地面快照、查看横断面预览、预览拖动缩放、批量绘制横断面、`DnRoadModelSectionDrawingEntity` 自定义实体落图、白色外框/桩号文字、横断面图配置、CSV 导入导出、图上结构层面域夹点编辑、双击编辑入口和 Bridge 回写 | `docs/modules/cross_section.md` |
| 出图、出表、算量 | `DRAWING_QUANTITY` | `RD_DRAWING_` | 已实现路面工程量统计表原型命令和路面结构图例命令；工程量可优先从横断面图实体当前面域读取修改后的结构层断面数据，按构造物范围切段，并支持按部件+结构层或按结构层类型生成 `.xls` 表格；结构图例可从道路模型或同路横断面图收集模板并绘制普通 CAD 图元 | `docs/modules/drawing_quantity.md` |
| 设计软件原型 Agent | `AI_AGENT` | `RD_AI_` | 右侧 Agent 面板、工具调用网关和首个自动化路基模板工具规划实现 | `docs/business/agent/设计软件原型Agent.md` |
| 辅助功能 | `UTILS` | `RD_UTIL_` | 目录已预留 | 待补充 |
