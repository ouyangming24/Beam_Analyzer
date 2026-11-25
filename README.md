# Beam Analyzer

Windows 桌面光束分析工具（Qt6 Widgets / C++），方向对标实验室 ISO 光束质量与功率/剖面测量工作台。  
**不宣称与 Cinogy 或其他商业产品功能/UI 像素级对等**；本仓库提供可扩展的离线/模拟采集分析工作台。

> **UI 灵感说明**：布局与测量工作流参考了常见实验室光束分析软件的交互习惯（更密的仪器面板、双轴 \(M^2\)/焦散、残差与报告导出等）。  
> **不是** [BeamLab / laser-beam-analyzer](https://github.com/aruchid/laser-beam-analyzer) 的 fork，未复制其代码、资源或品牌；本项目保持 **Qt6 Widgets**（不使用 Electron）。

| 阶段 | 状态 |
|------|------|
| Phase 1 | 离线 CSV MVP + 相机端口 |
| Phase 2 | 功率/ROI/剖面、M² 扫描 |
| Phase 3 | 相机协议适配层（GenTL/UVC 桩 + Simulated 实时） |
| Phase 4 | 商用级 UI 打磨、图像导入/导出、动态指标、最近文件/拖放 |
| Phase 5 | 鲁棒二阶矩/噪声、双曲线 \(M^2\)、电机粗扫→精扫、CLI、`docs/ALGORITHM.md` |
| Phase 6 | 仪器风工作站 UI：双轴焦散/残差、报告包导出、状态栏测量铬 |
| Phase 7 | 科技深色主题、3D 强度曲面、光斑 AOI、完整分析（ISO 13694/11670 概念 + BPP） |
| **Phase 8（当前）** | **仪器工作站增强：可保存布局、分组结果表、刀口/%能量宽度、暗场与标定向导、采集平均帧、单帧报告包** |

## Phase 4 亮点

- **专业桌面外观**：Qt6 QSS 深色/浅色主题、Segoe UI 系字体、工具栏 + 可停靠指标坞 + 状态栏摘要
- **图像 I/O**：打开 PNG / BMP / TIFF / JPEG（以及 CSV）；导出假彩色/灰度标注图（十字线、ROI、质心、指标条）
- **动态指标**：实时预览与帧历史 scrub 时 ISO/功率数值持续刷新，带单位
- **易用性**：最近打开文件、拖放图像/CSV 到主窗口

## 功能摘要

### 分析（Phase 1–2）
- ISO 11146：质心、\(d_\sigma=4\sigma\)、主轴/椭圆度
- 功率/能量标定占位、线剖面、ROI、背景扣除、M² 多点扫描与报告

### 相机（Phase 3）
- `CameraRegistry` + `simulated` / `gentl` 桩 / `uvc` 桩 / `null`
- 连接、实时预览、曝光/增益占位、触发模式、软触发（**未做真实硬件联调**）

### UI / I/O（Phase 4）
- 主题切换（视图菜单）、可浮动「测量指标」Dock
- 打开/拖放：`*.csv *.png *.bmp *.jpg *.jpeg *.tif *.tiff`
- 导出标注图像：`PNG/BMP/JPEG/TIFF`（BMP 走可移植库路径；其余走 Qt）
- 帧历史滑条：暂停实时后可回看最近帧并刷新指标

### Phase 5 — \(M^2\) / 电机扫描
- 背景 + 噪声门限选项（绝对值 / 峰值比例 / \(n\sigma\)）
- 双曲线 \(M^2\) 拟合：残差表、不确定度启发式、质量旗标；X/Y/合成或多轴单轴
- 电机位置规划：可配置粗扫→精扫、`IStage` / `SimulatedStage` + 采集钩子
- GUI：M² 对话框内仿真扫描、`D(z)` 曲线、报告/CSV 导出
- CLI：`beam_m2_scan`（无 Qt）运行仿真会话并导出报告
- 方法说明与文献/标准引用：[`docs/ALGORITHM.md`](docs/ALGORITHM.md)

### Phase 6 — 仪器风工作站 UI
- 更密的面板间距、状态栏模式芯片（离线/分析/相机/LIVE）、强度图测量铬条
- **M² 工作站**：双轴焦散 `Dx(z)` / `Dy(z)`、残差图+表、双轴摘要、报告包（Markdown+JSON+D(z)+残差 CSV）
- 假彩色/剖面优先；未引入 Electron 或重量级 3D 预览

### Phase 7 — 科技主题 / 3D / AOI / 完整分析
- 深色科技感主题（青强调色仪器风）；浅色主题仍可用
- **3D 强度曲面**（Qt `QPainter` 原生网格，可旋转缩放；大图显示降采样，与 2D/AOI 同步）
- **光斑 AOI**：矩形/圆/椭圆绘制与拖拽、自动 AOI、指标按 AOI 掩膜计算；叠加质心十字与光束椭圆
- **完整分析窗口**：分组参数表；均匀性/平坦度（ISO 13694 概念）、位置/指向稳定性（ISO 11670 概念）、M² 页补 BPP/发散角
- 算法在 `libs/core` 并有单测；公式写入 [`docs/ALGORITHM.md`](docs/ALGORITHM.md)
- **不**复制商业品牌资产，**不**声称认证对等

### Phase 8 — 商用工作站能力（手册工作流对标，自研实现）
- **布局预设**：2D / 3D / 剖面 / 结果表 splitter + dock 可命名保存/恢复
- **结果表**：分组可勾选（峰值、质心、D4σ、刀口、%能量、功率等）+ 可选滚动统计；单位清晰
- **宽度工具**：刀口 10/90·16/84、%峰值、%能量、包围功率直径（`libs/core` + ctest）
- **暗场 / 标定**：采集并应用暗场帧；像素尺寸与功率标定向导（模拟相机可用）
- **采集**：连续 / 单帧 / 平均 N 帧；帧历史批量导出
- **报告包**：一键 HTML + Markdown + JSON（含图与元数据）；UI 标明「概念实现 / 非正式计量证书」
- **不**声称与 BeamGage / Cinogy 等同或已认证

## 仓库结构

```
apps/BeamAnalyzer/   Qt6 Widgets 主程序（主题、Dock、拖放、M² 扫描）
apps/beam_m2_scan/   无 Qt 的 M² 扫描 CLI
libs/core/           ISO 11146、宽度工具、暗场/平均缓冲、ROI/AOI、M²、电机扫描
libs/io/             CSV + 图像导入/标注导出 + 单帧/M² 报告包
libs/camera/         ICamera、CameraRegistry、GenTL/UVC 桩、Simulated
docs/ALGORITHM.md    算法与测量实践说明（ISO 概念 + 刀口/%能量引用）
tests/core|camera|io 单元测试（含 TEM00 / 刀口 / 暗场 / M²）
samples/data/        示例 CSV
```

## 如何运行（Windows）

### 前置
1. CMake ≥ 3.21  
2. Visual Studio 2022 或 MinGW  
3. Qt 6 Widgets（例如 `C:\Qt\6.6.0\msvc2019_64`）  
4. （可选）`qtimageformats` 插件 —— 完整 TIFF 支持  
5. （可选）OpenCV —— 仅 UVC `VideoCapture`

### 配置与编译（MSVC）

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

可执行文件：`build\apps\BeamAnalyzer\Release\BeamAnalyzer.exe`

运行前请保证 Qt DLL 在 `PATH` 中（或用 `windeployqt`）。

### MinGW / Ninja

```bat
cmake -S . -B build -G Ninja ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\mingw_64 ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Linux 构建（CI / 开发机）

```bash
sudo apt-get install -y qt6-base-dev qt6-image-formats-plugins cmake ninja-build g++
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/apps/BeamAnalyzer/BeamAnalyzer
```

仅库与测试（无 Qt GUI；仍构建 `beam_m2_scan` CLI）：

```bash
cmake -S . -B build-libs -G Ninja -DBEAM_BUILD_APP=OFF
cmake --build build-libs
ctest --test-dir build-libs --output-on-failure
./build-libs/apps/beam_m2_scan/beam_m2_scan --m2 1.0 --out-prefix /tmp/m2_demo
```

## 使用示例

1. 启动 `BeamAnalyzer`
2. **打开** CSV / 图像，或把文件拖到窗口
3. 或：相机面板选 `Simulated Gaussian` → **连接** → **实时预览**（右侧指标坞与状态栏会动态更新）
4. 停止预览后拖动 **帧历史** 滑条回看
5. **导出标注图像…** 保存假彩色 + 十字线/ROI/指标条
6. 视图菜单可切换深色/浅色主题；指标坞可浮动/关闭
7. **M² 工作站…**：配置粗扫/精扫 → 仿真扫描 → 查看双轴焦散与残差 → **导出报告包**

### 截图说明（自行截取）

建议截取并放入发行说明 / PR：
1. 深色主题：主视图 + 右侧指标 Dock + 假彩色束斑  
2. 实时预览：状态栏 `Cx/Cy/Dx/Dy` 摘要与 LIVE 标记  
3. 导出的标注 BMP/PNG（含指标条）

本仓库 CI 为无头 Linux 构建，**不附带屏幕截图二进制**；请在 Windows 本机运行后补充。

### CSV 格式
- 逗号分隔、行主序；可选 `# pixel_um=5` 元数据

## 接入真实 SDK

保持 `ICamera` 稳定，向 `CameraRegistry` 注册厂商后端；默认构建不链接专有库。详见源码 `libs/camera` 与 Phase 3 注释。  
电机侧实现 `IStage`（当前提供 `SimulatedStage`），接入真实运动控制器后再接到粗扫→精扫会话。

## 路线图

| 阶段 | 内容 |
|------|------|
| Phase 1–7 | 已合入 main |
| Phase 8 | 布局/结果表/宽度工具/暗场标定/采集平均/单帧报告包 |
| 后续 | 真机相机/电机联调、Pass/Fail、多分区、连续日志 |

## 许可

暂未指定许可证；默认保留所有权利，直至仓库维护者另行声明。
