# 路基模板实现计划

日期：2026-05-12

## 约束

- 在主项目目录 `F:\0_GPT_道路设计原型功能项目` 直接开发，主目录保持最新文档和代码。
- 不新开 worktree。
- 先写核心测试，再补实现。
- `domain` 层不依赖 ObjectARX 和 WPF。
- 命令只通过 `CommandRegistry` 注册，模块只通过 `ModuleRegistry` 注册。
- WPF 只做 UI 和 Bridge 文件读写，不直接操作 CAD 对象。

## 步骤

1. 文档落点
   - 新增设计说明。
   - 新增本实现计划。

2. TDD 红灯
   - 在核心测试中增加高速公路、城市快速路默认模板测试。
   - 增加部件颜色、显示比例、变宽表基础计算测试。
   - 增加横断面模块命令注册和启动注册测试。
   - 增加托管 Ribbon 源码检查，确认新增面板、按钮、DXF 双击编辑入口。

3. Domain / Application
   - 新增 `SubgradeTemplateModel` 和默认模板服务。
   - 新增创建服务，输出默认模板数据。
   - 保持对 ObjectARX / WPF 零依赖。

4. 模块与启动注册
   - 新增 `CrossSectionModule`。
   - 新增 `CrossSectionStartupRegistration`。
   - 在 `Startup.cpp` 注册横断面模块。

5. ObjectARX
   - 新增 `DnSubgradeTemplateEntity`，完成绘制、DWG 读写、变换和 extents。
   - 新增创建、编辑、应用 Bridge 文件命令。
   - 新增 WPF Bridge 文件读写。
   - 在 ARX 入口初始化和卸载实体类。

6. WPF
   - 新增路基模板编辑窗口。
   - 新增二维预览 Canvas、部件选择、新增、删除、左右切换。
   - 新增变宽表和变坡表二级窗口。
   - 新增托管命令读取 pending 文件并回写 response 文件。
   - 在 Ribbon 增加“横断面设计 / 创建路基模板”，并增加双击编辑入口。

7. 工程文件
   - 更新 ARX `.vcxproj` 和核心测试 `.vcxproj`。
   - 更新构建版本到本次路基模板阶段。

8. 项目文档同步
   - 新增业务文档。
   - 新增模块文档。
   - 新增复用说明并更新复用目录。
   - 更新 README、测试说明、版本记录。

9. 验证
   - 构建并运行核心测试。
   - 构建 WPF Debug。
   - 构建 ARX Debug。
   - 在最终回复中给出主项目下 ARX 和托管 DLL 路径。
