# 编码规则

## 分层规则

- `domain` 不得包含 ObjectARX 头文件。
- `application` 负责组织流程，可调用 domain service 或 cad_adapter 接口。
- `modules` 负责注册命令、Ribbon 分组和模块元数据。
- `app/arx_entry` 只负责 ARX 生命周期。
- `cad_adapter/objectarx` 是直接调用 ObjectARX API 的唯一位置。
- 当前 RoadProto 原型阶段，用户对话框统一先放在 WPF UI 层实现。新增或重做参数窗口、编辑窗口、设置窗口、二级配置窗口时，不得在 `cad_adapter/objectarx` 中新增 C++ Win32/MFC 参数对话框作为当前阶段正式 UI；ObjectARX 层只负责 CAD 选择、实体打开、预览、绘制、持久化和 Bridge 调用。未来如整体更换 UI 技术栈，仍必须保持该边界。

## 命令规则

每个命令必须包含：

- 内部命令名。
- 显示名。
- 模块编码。
- 功能说明。
- 命令入口函数。
- 是否为原型功能。
- 是否具备复用价值。
- 对应业务文档路径。
- 是否可挂接 Ribbon。

## 文档规则

每个独立功能都需要在 `docs/business/<module>/` 下建立单独业务文档；一份业务文档只描述一个功能，不把创建、编辑、导入导出、内部桥接等不同功能混写。

每个原型命令的 `businessDocPath` 必须指向该命令所属功能的业务文档；内部命令如果承载 Bridge、handle 编辑或回写流程，也要指向对应功能或桥接说明文档。

模块文档和 README 只做索引、范围和入口说明，不替代功能业务文档。

每个可复用能力都应记录在 `docs/reuse/` 下。

每个生成的 ARX 版本都要记录到 `docs/dev/version_log.md`。

使用 `.worktrees/<分支名>` 开发时，文档需要双写：在 worktree 内保存正式文档后，收尾前必须把同一份文档按相同相对路径同步到主项目目录 `F:\0_GPT_道路设计原型功能项目`。这条规则适用于 `AGENTS.md`、`README.md`、`docs/business/`、`docs/modules/`、`docs/reuse/`、`docs/dev/`、`docs/architecture/`、`docs/rules/`、`tests/README.md` 和用户明确要求可见的说明文档。源代码不按此规则双写，避免主目录和 worktree 分支代码互相覆盖。

## 测试规则

- core、domain、application 逻辑应尽量提供不依赖 AutoCAD 的测试。
- ObjectARX 集成能力需要在 AutoCAD 2021 中加载 ARX 后验证。
- 测试应聚焦行为和扩展契约，不要只验证实现细节。
