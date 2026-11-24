# Beam Analyzer

Windows 桌面光束分析工具（Qt6 / C++），方向对标 Cinogy 类 ISO 光束质量与功率/剖面测量。

**Phase 1**：离线 CSV 帧分析 MVP + 相机适配端口。  
**Phase 2**：功率/能量标定占位、线剖面、ROI、背景扣除、M² 多点扫描与报告导出、设置持久化。  
**Phase 3（当前）**：相机通信层第一版——插件注册/发现、`ICamera` 扩展、GenTL/UVC 桩、Simulated 实时帧率与触发；**未做真实硬件联调**。

## 功能

### Phase 1–2
- ISO 11146：质心、二阶矩束宽 \(d_\sigma=4\sigma\)、主轴/椭圆度
- CSV 导入、灰度/假彩色、功率/剖面/ROI/背景、M² 扫描与导出

### Phase 3 — 相机通信层
- **`CameraRegistry`**：发现/注册后端，按设备 id 创建 `ICamera`
- **内置后端**
  - `simulated`：高斯束；支持 FreeRun FPS、Software/Hardware 触发仿真
  - `gentl`：GenICam / GigE Vision 风格 **GenTL consumer 桩**（文档化 CTI 挂钩；无专有库）
  - `uvc`：UVC / USB 采集桩；可选 `-DBEAM_ENABLE_OPENCV=ON` 启用 OpenCV `VideoCapture`
  - `null`：无帧占位
- **UI**：源选择、连接/断开、实时预览、曝光/增益占位、触发模式枚举、软触发
- **CI**：默认不链接 OpenCV / GenTL；单元测试覆盖 registry 与 Simulated 实时路径

## 仓库结构

```
apps/BeamAnalyzer/   Qt6 Widgets 主程序（含相机采集面板）
libs/core/           ISO 11146、ROI/背景、功率、剖面、M²
libs/io/             CSV 导入与结果/报告导出
libs/camera/         ICamera、CameraRegistry、GenTL/UVC 桩、Simulated
tests/core/          算法单元测试
tests/camera/        registry / 实时模拟路径测试
samples/data/        示例 CSV
```

## 如何接入真实 SDK 插件（后续）

保持 `ICamera` 稳定，按以下步骤挂厂商 SDK（Basler Pylon、IDS peak、FLIR Spinnaker、Thorlabs 等）：

1. **实现** `beam::camera::ICamera`（`open/close`、`setControls`、`startAcquisition`/`grab`、触发）。
2. **注册**到 `CameraRegistry`：
   ```cpp
   CameraRegistry::instance().registerBackend(
       CameraBackendDescriptor{"sdk:vendor", "Vendor Cameras", "sdk:vendor", true,
                               "Requires Vendor SDK"},
       /*discover*/ [] { /* enumerate serials */ return devices; },
       /*factory*/  [](const CameraInfo& d) { return createVendorCamera(d); });
   ```
3. **可选依赖**：在 `libs/camera/CMakeLists.txt` 增加 `option(BEAM_ENABLE_VENDOR …)`，仅在找到 SDK 时 `target_link_libraries`；默认 OFF，保证无 SDK 的 CI/开发机仍可构建。
4. **GenICam / GigE / USB3 Vision**：优先走 GenTL——把厂商 `.cti` 路径交给 `configureGenTL()`，把 `GenTLCamera.cpp` 中标注的 hook（`TLOpen` / `IFOpen` / `DevOpen` / `DSStartAcquisition` / GenApi 节点）换成真实 GenTL consumer；不必把专有头文件推进默认构建。
5. **普通 UVC 摄像头**：`-DBEAM_ENABLE_OPENCV=ON` 即可；真正的 USB3 Vision（GenICam）仍用 GenTL + CTI。

调用 `registerAllBuiltInBackends()`（应用启动时已调用）或在动态库加载时自行 `registerBackend`。

## Windows 构建（Qt6）

### 前置

1. CMake ≥ 3.21  
2. Visual Studio 2022（“使用 C++ 的桌面开发”）或 MinGW  
3. Qt 6 Widgets，例如 `C:\Qt\6.6.0\msvc2019_64`  
4. （可选）OpenCV 4.x —— 仅当需要 UVC `VideoCapture` 时  
5. （可选）厂商 GenTL `.cti` + GenTL 加载库 —— 仅当替换 GenTL 桩时

### 配置与编译（MSVC）

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

可执行文件：`build\apps\BeamAnalyzer\Release\BeamAnalyzer.exe`

### 可选 OpenCV（UVC）

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64;C:\opencv\build ^
  -DBEAM_ENABLE_OPENCV=ON
```

Windows 上 OpenCV 常通过 DirectShow/Media Foundation 枚举索引 `0..N`。运行时请保证 Qt 与 OpenCV 的 DLL 在 `PATH` 中。

### MinGW

```bat
cmake -S . -B build -G Ninja ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\mingw_64 ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Linux 构建（CI / 开发机）

```bash
sudo apt-get install -y qt6-base-dev cmake ninja-build g++
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

仅库与测试（无 Qt GUI）：

```bash
cmake -S . -B build -G Ninja -DBEAM_BUILD_APP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

## 使用示例

1. 启动 `BeamAnalyzer`
2. **相机采集**：选择 `Simulated Gaussian` → **连接** → 勾选 **实时预览**
3. 调整曝光/增益占位、触发模式；软触发模式下点 **软触发一次**
4. 或 **打开 CSV…** / 工具栏 **模拟帧** 做离线分析
5. ROI / 剖面 / M² / 导出与 Phase 2 相同

### CSV 格式

- 逗号分隔、行主序；可选 `# pixel_um=5` 元数据

## 路线图摘要

| 阶段 | 内容 |
|------|------|
| Phase 1 | 离线 CSV MVP、ISO 基础量、相机端口 |
| Phase 2 | 功率/剖面、ROI、背景、M² 扫描、设置持久化 |
| Phase 3 | 相机协议适配层第一版（本 PR）；真实 SDK 待厂商库接入 |
| 后续 | 硬件联调、标定向导、触发线验证 |

## 许可

暂未指定许可证；默认保留所有权利，直至仓库维护者另行声明。
