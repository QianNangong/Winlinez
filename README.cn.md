# Color Linez（Win32 复原版）

对 **Color Linez v1.21**（`WINLINEZ.EXE`，60KB，1999 年）——1992 年 DOS
经典游戏《Color Lines》的 Win32 移植版——通过纯静态分析从原始二进制
完整复原出的可编译源码工程。

```
 横、竖、斜方向凑齐 5 个以上同色球即可得分。
 击败"国王"的分数（Handicap，100 分），你的国王将加冕。
```

| | |
|---|---|
| 原始 DOS 版 | *Color Lines* (c) 1992 GAMOS LTD，俄罗斯 —— 程序：Olga Demina，美术：Igor Ivkin & Gennady Denisov |
| Win32 移植版 | (c) 1998-1999 Ivan Golubev（m53group）—— 免费软件，"写给我弟弟的生日礼物" |
| 原始工具链 | GCC/egcs（mingw32）+ GNU ld，无 CRT，手写启动代码 |
| 本复原工程 | 标准 C99 + Win32 API，MinGW（i686）+ CMake 构建 |

原始程序可从 <https://archive.org/details/ColorLinez_1020> 获取。

## 目录结构

```
color-linez/
├── CMakeLists.txt                     构建脚本（原生 Windows 或 mingw 交叉）
├── cmake/mingw-i686.toolchain.cmake   交叉编译工具链文件
├── src/
│   ├── winlinez.c                     全部游戏逻辑（注释标注原二进制地址
│   │                                  FUN_004xxxxx，便于对照反汇编）
│   ├── inflate.c                      原 EXE 内嵌的 raw-DEFLATE 解压器的
│   │                                  整洁重写版
│   ├── gfxdata.c                      从原 .data 节逐字节提取的压缩精灵图集
│   ├── resource.h / res/winlinez.rc   与原版逐字节比对的资源
│   └── test_inflate.c                 gfxdata 的 CRC-32 回归测试
└── res/
    ├── icon.ico                       游戏图标（由 RT_ICON 重新打包）
    ├── about_king.bmp / about_mask.bmp  关于对话框国王立绘
    └── sprites.bmp                    解压后的 511×585 精灵图集（参考件；
                                       构建时内嵌的是 src/gfxdata.c 中的
                                       压缩数据，与原版一致）
```

## 构建

需要 Windows 交叉编译器或原生编译器：MinGW-w64（系统包）或
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw)。在装好 mingw-w64
的 Linux/Unix 上，CMake 会自动探测：

```sh
cmake -B build
cmake --build build
```

通过两个缓存选项选择目标架构与工具链：

```sh
cmake -B build -DCOLORLINEZ_ARCH=x86_64 -DCOLORLINEZ_TOOLCHAIN=llvm-mingw
cmake --build build
```

| `COLORLINEZ_ARCH`    | `COLORLINEZ_TOOLCHAIN`        | 验证方式                |
|----------------------|-------------------------------|-------------------------|
| `i686`（默认）       | `mingw`（默认）/ `llvm-mingw` | wine + Windows 实测     |
| `x86_64`             | `mingw` / `llvm-mingw`        | wine + Windows 实测     |
| `aarch64`            | `llvm-mingw`                  | 仅编译验证              |

llvm-mingw 需要在 `PATH` 中；也可以直接指定编译器
（`-DCMAKE_C_COMPILER=... -DCMAKE_RC_COMPILER=...`）。产物：
`build/winlinez.exe`——Windows 原生运行，Unix 下用
`wine build/winlinez.exe` 运行。

## 测试

```sh
ctest --test-dir build
```

验证内嵌精灵数据能解压出原始的 300,598 字节位图（CRC-32 `cd043a57`）。

## 复原保真度

* 全部游戏逻辑（BFS 寻路、计分、国王竞赛、高分榜、统计、菜单/对话框、
  动画）均遵循原版代码，注释中引用原始二进制地址（`FUN_004xxxxx`）。
* 图形管线完全一致：一张 raw-DEFLATE 压缩的 511×585 8bpp BMP 内嵌于
  `.data` 节，WM_CREATE 时解压为 DIB + 调色板——同样的字节、同样的
  解压器逻辑。
* 资源（菜单含右对齐的 `MF_HELP` Help 弹出项、加速键、五个对话框、
  图标、About 位图）与原版模板逐字节比对一致。
* 原版怪癖有意保留：球颜色为 1..7（值 8 保留作寻路种子标记）、
  `>7` 洪水标记清理、统计把消除数记入最后一种颜色等。

### 对原版的有意偏差

1. **棋盘居中**：棋盘原点在 `WM_CREATE` 和 `WM_SIZE` 中都会重算，并在
   任意窗口尺寸下都居于工具栏与窗口底部之间的中央。原版仅依赖
   `WM_SIZE`，且窗口处于最小高度时棋盘被顶在工具栏下方、底部被裁
   （Windows 10 上可见；XP/Wine 下正常）。
2. `WM_PAINT` 绘制前先将客户区填黑，走子失败后主动失效重绘——
   否则现代 Windows 可能暴露残留像素。
3. `res/about_king.bmp` / `res/about_mask.bmp` 为标准 3.00 格式 BMP
   （原版 PE 内是裸 DIB，MSVC rc.exe 拒绝编译，windres 会错误重编码）。

与原版的已知差异记录在 `src/winlinez.c` 头部注释中。

## 致谢

* 原始游戏：GAMOS LTD 1992 —— Olga Demina（程序），
  Igor Ivkin & Gennady Denisov（美术）。
* Win32 移植：Ivan Golubev，1998-1999，m53group。
  原始程序下载：<https://archive.org/details/ColorLinez_1020>。
* 复原：由 [OpenCode](https://opencode.ai) 中的 **Ox Alpha** 模型使用
  Ghidra 逆向完成——反编译、资源解析、精灵图集恢复以及全部 C 源码
  重写均在同一会话中完成。
