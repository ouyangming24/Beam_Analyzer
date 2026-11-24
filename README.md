# Beam Analyzer

Windows 桌面光束分析工具（Qt6 / C++），方向对标 Cinogy 类 ISO 光束质量与功率/剖面测量。  
**不宣称与 Cinogy 功能或 UI 像素级对等**；本仓库提供可扩展的离线/模拟采集分析工作台。

| 阶段 | 状态 |
|------|------|
| Phase 1 | 离线 CSV MVP + 相机端口 |
| Phase 2 | 功率/ROI/剖面、M² 扫描 |
| Phase 3 | 相机协议适配层（GenTL/UVC 桩 + Simulated 实时） |
| **Phase 4（当前）** | **商用级 UI 打磨、图像导入/导出、动态指标、最近文件/拖放** |

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

## 仓库结构

```
apps/BeamAnalyzer/   Qt6 Widgets 主程序（主题、Dock、拖放）
libs/core/           ISO 11146、ROI/背景、功率、剖面、M²
libs/io/             CSV + BMP 图像导入/标注导出 + 结果导出
libs/camera/         ICamera、CameraRegistry、GenTL/UVC 桩、Simulated
tests/core|camera|io 单元测试
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

仅库与测试（无 GUI）：

```bash
cmake -S . -B build-libs -G Ninja -DBEAM_BUILD_APP=OFF
cmake --build build-libs
ctest --test-dir build-libs --output-on-failure
```

## 使用示例

1. 启动 `BeamAnalyzer`
2. **打开** CSV / 图像，或把文件拖到窗口
3. 或：相机面板选 `Simulated Gaussian` → **连接** → **实时预览**（右侧指标坞与状态栏会动态更新）
4. 停止预览后拖动 **帧历史** 滑条回看
5. **导出标注图像…** 保存假彩色 + 十字线/ROI/指标条
6. 视图菜单可切换深色/浅色主题；指标坞可浮动/关闭

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

## 路线图

| 阶段 | 内容 |
|------|------|
| Phase 1–3 | 已合入 main |
| Phase 4 | UI / 图像 I/O / 动态指标（本版本） |
| 后续 | 硬件联调、标定向导、更完整的序列/报告工作流 |

## 许可

暂未指定许可证；默认保留所有权利，直至仓库维护者另行声明。
