# Cze 对轨 (CzeAlign) — AviUtl2 插件 / AviUtl2 Plugin

读取 Reaper 工程（`.rpp`），把选中的轨道按 item 的时间/音高拆开，
在 AviUtl2 的图层上批量生成**空白图片物件**（`画像ファイル`）。

Reads a Reaper project (`.rpp`), splits the selected tracks by item time/pitch and
creates blank **image objects** (`画像ファイル`) on AviUtl2 layers.

- 插件类型 / Type：泛用插件 `.aux2`
- 适用 / Host：AviUtl ExEdit2 x64 (2.x)
- 版本 / Version：1.0.0
- 许可 / License：MIT
- 界面 / UI：中文 · English · 日本語

---

# 中文

## 1. 功能

```
1.rpp ──► 勾选轨道 + 时间范围 ──► 从起始帧开始，每个 item 生成一个物件
                                  第1层 第3层 第5层 … 到第 N 层后回到第 1 层
                                  如此循环，直到所有 item 都生成完
```

- 每个 item = 一个物件，长度严格按 item 的起止时间（可用时间范围裁剪）；
- 物件按时间先后排布，每生成一个就下移「空层数 + 1」层；
- 回卷可控：每 N 层或每 N 秒回到起始层，**一直生成到最后一条**，不会卡在等第 100 层；
- 物件默认命名 `take 名 + 音高`，如 `ding_c.mp3 -3`；
- 分批生成：进度窗口显示进度、可随时取消，AviUtl2 不会假死。

## 2. 安装

- **安装包**：把 `dist/CzeAlign-v1.0.0.au2pkg.zip` 拖到 AviUtl2 预览画面，按提示安装，重启。
- **单文件**：把 `dist/CzeAlign.aux2` 复制到 `C:\ProgramData\aviutl2\Plugin\`（或其子文件夹）。
- 便携版：AviUtl2 目录下有 `data` 文件夹时，插件目录是 `data\Plugin`。
- 首次加载弹「脚本・插件的追加」时选择信任。
- 装过旧名字（ReaperAlign 等）的，请先删除旧的 `.aux2`。

## 3. 使用

打开：**文件 →「Cze 对轨导入 (RPP)...」**，或把 `.rpp` 拖到 AviUtl2 窗口。

| 项目 | 默认 | 说明 |
|---|---|---|
| RPP 文件 | — | 选 `.rpp` 后自动列出轨道 |
| 轨道列表 | 全选 | 勾选要用的轨道，显示 item 数与时间范围 |
| RPP 时间（秒） | 0 – 工程长度 | 只生成范围内的 item，跨界自动裁剪；结束填 0 = 到结尾 |
| 起始帧 | 0 | 时间范围开始对应 AviUtl2 的第几帧（生成起点） |
| 起始图层 | 1 | 第一个物件所在层 |
| 空层数 | 1 | 1 = 物件在 1、3、5… 层；0 = 每层都有 |
| 回卷方式 | 按层数，每 8 层 | `按层数`：每 N 层回到起始层；`按时间`：每 N 秒回到起始层 |
| 物件类型 | 画像ファイル | `画像ファイル` / `図形` / `グループ制御` |
| 用 item 名命名物件 | 开 | 物件名 = take 名 + 音高 |
| 语言 | 自动 | 左下角可切 中文 / English / 日本語 |

生成前点 **统计…** 可查看物件数、图层范围、回卷次数。

## 4. 回卷规则

- 按层数（默认 8）：第 1、3、5…15 层，第 9 个物件回第 1 层；
- 按时间（默认 10 秒）：每 10 秒（从范围开始算）回起始层；
- 两种方式都会一直生成到最后一个 item；起始层 + 空层数超过 100 层时在第 100 层折回。

## 5. 界面语言

默认读 AviUtl2 的 `[Translate] Language=`；读不到则跟随 Windows 显示语言。
对话框左下角可手动切换，立即生效（菜单名重启后变）。

## 6. 构建与测试

需要 Visual Studio 2022 x64 C++ 工具链，SDK 头文件在 `sdk/aviutl2_sdk/`。

```bat
build.bat
build\core_test.exe "G:\reaper\123123123.rpp"
build\mock_host.exe build\CzeAlign.aux2
```

期望：`67 checks, 0 failure(s)` 与 `11 checks, 0 failure(s)`。

产物：`build\CzeAlign.aux2`、`dist\CzeAlign.aux2`、`dist\CzeAlign-v1.0.0.au2pkg.zip`。

## 7. 限制

- 只处理当前场景，不会新建场景；
- 目标图层位置已有物件时该物件会被跳过（宿主限制），并在结束时统计；
- 每个批次是一个撤销点；
- 本机无法启动 AviUtl2 界面，对话框与真实建物件未在宿主中实测。

---

# English

## 1. What it does

```
1.rpp ──► pick tracks + time range ──► starting at the start frame, one object per item
                                       layer 1, 3, 5 … then back to layer 1 after N layers
                                       keeps cycling until every item is created
```

- One object per item, length follows the item exactly (clipped by the time range);
- Objects go down one layer group at a time (`empty layers + 1` each step);
- Wrap is configurable: every N layers or every N seconds, and it **keeps generating to the end**;
- Objects are named `take name + pitch`, e.g. `ding_c.mp3 -3`;
- Creation is batched: progress window, cancel button, the host stays responsive.

## 2. Install

- **Package**: drag `dist/CzeAlign-v1.0.0.au2pkg.zip` onto the AviUtl2 preview, install, restart.
- **Single file**: copy `dist/CzeAlign.aux2` into `C:\ProgramData\aviutl2\Plugin\` (or a subfolder).
- Portable setup: if AviUtl2 has a `data` folder, the plugin folder is `data\Plugin`.
- Accept the “script/plugin added” trust dialog on first load.
- Remove older builds (ReaperAlign, etc.) to avoid duplicate menus.

## 3. Usage

Open **File → “CzeAlign Import (RPP)...”** or drag a `.rpp` onto the AviUtl2 window.

| Item | Default | Notes |
|---|---|---|
| RPP file | — | Tracks are listed after you pick the file |
| Tracks | all | Check the tracks to use; shows item count and time range |
| RPP time (s) | 0 – project end | Only items inside the range; items are clipped. End `0` = to the end |
| Start frame | 0 | AviUtl2 frame that the range start maps to |
| Start layer | 1 | Layer of the first object |
| Empty layers | 1 | 1 = objects on layers 1, 3, 5…; 0 = every layer |
| Wrap | by layers, every 8 | `By layers`: back to start layer every N layers; `By time`: every N seconds |
| Object type | Image file | `Image file` / `Figure` / `Group control` |
| Name objects after items | on | Object name = take name + pitch |
| Language | auto | Switch at the bottom-left: 中文 / English / 日本語 |

Click **Stats…** before generating to preview object count, layer range and wrap count.

## 4. Wrap rules

- By layers (default 8): layers 1, 3, 5…15, then back to layer 1 on the 9th object;
- By time (default 10 s): back to the start layer every 10 s from the range start;
- Both keep creating every remaining item; if start layer + empty layers exceed 100, it folds at layer 100.

## 5. UI language

Follows AviUtl2's `[Translate] Language=` setting, otherwise the Windows display language.
You can also switch it in the dialog (takes effect immediately; menu names change on restart).

## 6. Build & test

Requires Visual Studio 2022 (x64 C++ tools); SDK headers are in `sdk/aviutl2_sdk/`.

```bat
build.bat
build\core_test.exe "G:\reaper\123123123.rpp"
build\mock_host.exe build\CzeAlign.aux2
```

Expected: `67 checks, 0 failure(s)` and `11 checks, 0 failure(s)`.

Outputs: `build\CzeAlign.aux2`, `dist\CzeAlign.aux2`, `dist\CzeAlign-v1.0.0.au2pkg.zip`.

## 7. Limitations

- Only edits the current scene;
- Objects that would overlap an existing object are skipped (host restriction) and counted;
- Each batch is one undo step;
- The AviUtl2 GUI could not be started on the build machine, so the dialog and real object creation were not exercised in the host.

---

## License

MIT. AviUtl ExEdit2 Plugin SDK © ＫＥＮくん (MIT).
