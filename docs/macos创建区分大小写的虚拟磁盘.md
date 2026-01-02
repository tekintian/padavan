# 创建一个区分大小写的磁盘镜像



## 基础方案：磁盘工具（图形界面）快速创建
macOS 自带的“磁盘工具”是最直观的创建方式，无需命令行，适合新手：
1. 打开“磁盘工具”（Launchpad → 其他 → 磁盘工具）；
2. 点击顶部菜单栏「文件」→「新建图像」→「空白图像」；
3. 配置关键参数：
   - 存储为：设置虚拟磁盘文件保存路径（如 `~/CaseSensitiveDisk.dmg`）；
   - 名称：挂载后显示的磁盘名称（如 `CaseSensitiveDisk`）；
   - 大小：按需设置（如 50GB）；
   - 格式：选择「APFS（区分大小写）」或「Mac OS 扩展（日志式，区分大小写）」；
   - 分区：「单一分区 - GUID 分区图」；
   - 图像格式：「稀疏磁盘映像」（动态扩容，节省空间）；
4. 点击「存储」，系统自动创建并挂载虚拟磁盘，可在访达侧边栏查看。

**优势**：操作简单，无参数错误风险；**不足**：无法批量创建或集成到脚本中。



## 命令行创建
通过 `hdiutil -help` 确认原生参数
执行 `hdiutil create -help` 查看官方参数说明，发现核心规律：
- `hdiutil create` 强制语法：`hdiutil create <sizespec> <imagepath>`（大小参数必须在路径前）；
- 支持的区分大小写格式：
  - APFS 格式：`Case-sensitive APFS`（原生参数，简写 `APFSCaseSensitive`）；
  - Mac OS 扩展格式：`Case-sensitive Journaled HFS+`（简写 `JHFSX`）；
- APFS 格式必须搭配 `-layout GPTSPUD`（GUID 分区图，APFS 依赖该分区方案）。

### 最终可用命令：
#### （1）APFS 区分大小写（推荐）
注意这里的核心参数 -fs "Case-sensitive APFS"  这里的Case-sensitive 就是区分大小写的核心参数,必须在前面
```bash
hdiutil create -size 50g -type SPARSE -layout GPTSPUD -fs "Case-sensitive APFS" -volname "CaseSensitiveDisk" ~/CaseSensitiveDisk.dmg

# （2）Mac OS 扩展区分大小写（兼容低版本）
hdiutil create -size 50g -type SPARSE -volname CaseSensitiveDisk -fs JHFSX ~/CaseSensitiveDisk.dmg


# 加载虚拟盘
hdiutil mount ~/CaseSensitiveDisk.dmg
~~~

