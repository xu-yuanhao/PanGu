# 主线整合与分支收尾

用户授权：将GitHub收敛为一个main，提交全部代码并保持工作区干净。TUI开发者已由用户确认停止写入。

## 代码与历史保留

初始远端28个分支，默认分支为bootstrap/project-foundation，没有远端main。所有分支tip及恢复来源登记在main-consolidation.json。整合提交将这些tip作为父提交保留；旧分支删除后，旧源码、原控制登记和证据仍通过提交SHA可追溯，而非丢弃分支代码。

从远端分支恢复当前工作区缺失的191个src文件；同路径有多个历史版本时，使用tip提交时间较新的分支内容，完整候选版本见JSON及Git历史。当前Runtime/TUI源码不覆盖。这些历史代码保持实验/测试属性，不自动加入Runtime默认构建；未声称191个文件组合后具备统一ABI或已经全量重编译。

当前Runtime源码、TUI源码/测试/脚本、文档和控制登记一并提交。TUI与真实Runtime尚未桥接，不声明已有真实模型TUI产品。并行Agent未交付最终报告，当前提交保存它已完成的源码，而非替它声明全部验收。

## 构建产物与必要修复

已有TUI构建产物移到仓库外临时备份；构建脚本默认输出到/tmp/pangu-tui-build-UID，可用PANGU_TUI_BUILD_DIR覆盖。空的tools/tools目录也移出，源码不丢弃。新增.gitignore仅排除只读references，避免把独立参考仓库作为子模块或产品源码提交。

修复TUI假事件源测试的终态后无限循环，以及两处应等待有效事件却假定poll立即返回1的测试调用。没有放宽产品规则或删除断言。检查时停止两个确切的、已卡住的test_fake_runtime进程，其中一个来自另一开发会话的遗留运行。

G00新增精确文件登记类型：client源码/测试/构建、历史native源码及.gitignore；限定目录/扩展名，仍默认拒绝未登记文件。历史source类型不能用于runtime/contracts/client，相关正反例已补。不是生产准入或沙箱保证。

## 验证

- `timeout 30 bash tools/client/tui/build.sh run-tests`：TUI五组单测通过。
- `python3 -B tools/runtime/build_test.py`：Loop/SSE/registry/read/projection五组O0/O2 UBSan构建执行通过。
- `python3 -B tools/gates/test_check.py`：16项检查器测试通过。
- 最终G00、格式、凭据预检及远端保留/分支状态由本次收尾实际执行，不以本文提前声称成功。远端若拒绝操作，保持分支与历史，不关闭规则以绕过。

## 控制面自审

同一Agent按用户授权自审自批上述有限库存类型和源码整合，不是独立审核。风险：历史实验组合未整体认证、TUI未接真实Runtime，远端main仍受既有规则约束。没有调整旧性能预算、参考基线或远端保护。所有初始tip均须可从最终提交到达，才允许删除远端分支。后续使用main作为主线，临时开发分支完成后及时清理。

本次自审固定摘要（记录自身排除）：

```text
7c57317405b19c2b172c86e8e66c2672f8435a7574abaf99556f91719ad2b672  tools/gates/check.py
ebb5db8850eb5ff4a1fe35adf7bc6303d9bc86e3e02ec958cc89ec687575b579  tools/gates/test_check.py
5002e6d3370c6f53191f71e5671d3acc16260497448dcfb57b2451da5d4be5c9  governance/bootstrap-policy.json
c357ceb213c82d56e36a01946817b40789ab69084642e43e233d9b079e6fe958  tools/client/tui/build.sh
374190164a6a410bacccf3609b15cf2f50478f1705f444f18028474c5e149fd7  tests/client/tui/test_fake_runtime.c
0d48407ea251ad6b8e85ec6ee07860d50a9242ffb3fe924fe2425ac74fec3b3b  docs/migration/main-consolidation.json
cd948cae6e3bb5464617a857e4ed6c367e39b808341781949e1b1e0da8588a4f  .gitignore
```
