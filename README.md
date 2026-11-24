# Beam Analyzer

Windows 桌面光束分析工具（Qt6 / C++），方向对标 Cinogy 类 ISO 光束质量与功率/剖面测量。

**Phase 1**：离线 CSV 帧分析 MVP + 相机适配端口。  
**Phase 2（当前）**：功率/能量标定占位、线剖面、ROI、背景扣除、M² 多点扫描与报告导出、设置持久化。  
**不宣称**完整 Cinogy 功能对等；真实相机 SDK 见 Phase 3。

## 功能

### Phase 1
- ISO 11146：质心、二阶矩束宽 \(d_\sigma=4\sigma\)、主轴/椭圆度
- CSV 二维强度帧导入（`# pixel_um=` 元数据）
- 灰度/假彩色显示、指标面板、结果导出 JSON/CSV
- 相机抽象 `ICamera` + `NullCamera` / `SimulatedCamera`

### Phase 2
- **功率/能量**：`counts→W` 标定占位、曝光时间 → 能量；可导出功率 JSON
- **线剖面**：单击图像设置十字线；水平/垂直剖面图；可导出剖面 CSV
- **ROI**：工具栏「框选 ROI」拖拽矩形；「启用 ROI」后指标在裁剪区域计算
- **背景扣除**：无 / 常数 / 边框均值 / 背景条带均值
- **M² 多点扫描**：对话框添加轴向测点 → 拟合 M²_X / M²_Y / 合成 → 导出 Markdown/JSON 报告
- **设置持久化**：色图、背景模式、标定参数、窗口几何（`QSettings`）

## 仓库结构

```
apps/BeamAnalyzer/   Qt6 Widgets 主程序（含剖面图、ROI、M² 对话框）
libs/core/           ISO 11146、ROI/背景、功率、剖面、M² 拟合与扫描
libs/io/             CSV 导入与结果/报告导出
libs/camera/         ICamera 抽象与模拟源
tests/core/          合成高斯与 Phase 2 单元测试
samples/data/        示例 CSV
```

## Windows 构建（Qt6）

### 前置

1. 安装 [CMake](https://cmake.org/) ≥ 3.21
2. 安装 Visual Studio 2022（含 “使用 C++ 的桌面开发”）或 MinGW
3. 安装 Qt 6（Widgets），并记住安装路径，例如 `C:\Qt\6.6.0\msvc2019_64`

### 配置与编译（MSVC）

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

可执行文件：`build\apps\BeamAnalyzer\Release\BeamAnalyzer.exe`  
示例数据会复制到可执行文件旁的 `samples\`。

### 配置与编译（MinGW）

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

无显示环境时可只编库与测试：

```bash
cmake -S . -B build -G Ninja -DBEAM_BUILD_APP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

## 使用示例

1. 启动 `BeamAnalyzer`
2. **打开 CSV…** → `samples/data/gaussian_ellipse.csv`（或 **模拟帧**）
3. 单击图像设置剖面十字线；查看下方水平/垂直剖面
4. **框选 ROI** → 勾选「启用 ROI」；可选背景扣除模式
5. 设置 `counts→W` / 曝光以查看标定功率/能量（占位）
6. **M² 扫描…** → 添加 ≥3 个轴向测点 → 拟合 → 导出报告
7. 导出 ISO 指标 JSON/CSV、功率 JSON、剖面 CSV

### CSV 格式

- 逗号分隔、行主序：每行一个 Y，列对应 X
- 可选首行元数据：`# pixel_um=5` 或 `# pixel_um_x=5.2,pixel_um_y=5.0`

## 路线图摘要

| 阶段 | 内容 |
|------|------|
| Phase 1 | 离线 CSV MVP、ISO 基础量、相机端口 |
| Phase 2 | 功率/剖面、ROI、背景扣除、M² 扫描 UI、设置持久化（本 PR） |
| Phase 3 | 真实相机 SDK 插件、触发与标定 |

## 许可

暂未指定许可证；默认保留所有权利，直至仓库维护者另行声明。
