# VisualMetrics: 基于 IBL 的 PBR 模型视觉误差评估系统

## 1. 项目简介

VisualMetrics 是一个基于现代 OpenGL (C++17) 开发的自动化视觉质量评估工具。它旨在通过物理渲染 (PBR) 管线，客观量化三维模型在几何简化（Simplification）或优化（Optimization）前后的视觉差异。

该系统模拟标准化的“虚拟摄影”环境，支持自动化批量处理多个模型，自动生成多视角对比图像，并计算 **PSNR (峰值信噪比)**、**ND (法线一致性误差)** 和 **SD (轮廓误差)** 三项关键指标。最终输出多层级的 CSV 数据报表及带有像素级精度刻度的可视化热力图。

## 2. 渲染场景生成

为了确保评估的客观性和一致性，本工程构建了一套标准化的虚拟采样环境。

### 2.1 模型归一化 (Normalization)

由于输入模型（Ref 和 Base）可能具有不同的尺度和原点偏移，系统在加载阶段执行自动归一化：

1. **计算包围盒 (AABB)**：遍历模型所有网格顶点，计算轴对齐包围盒。
2. **中心化**：将模型几何中心平移至世界坐标原点 $(0,0,0)$。
3. **统一缩放**：计算包围盒的最长对角线，将模型统一缩放到半径为 $1.0$ 的单位球体内。

### 2.2 斐波那契视点采样 (Fibonacci Viewpoint Sampling)

为了全方位覆盖模型的几何特征，系统摒弃了传统的单一视角，采用球面均匀采样策略：

- **采样算法**：基于斐波那契格点 (Fibonacci Lattice) 算法。
- **采样参数**：
  - 视点数量 $N = 64$。
  - 摄像机所在球体半径 $R = 2.0$ (完全包裹半径为 1 的模型)。
  - 摄像机方向：始终看向原点 $(0,0,0)$。
  - 视场角 (FOV)：根据模型包围球自适应计算，确保模型充满画面且不被裁剪。
- **优势**：相比经纬度网格采样，斐波那契采样在球面上分布更加均匀，且面积加权一致。

### 2.3 统一光照环境 (Unified IBL Lighting)

采用基于图像的照明 (Image Based Lighting) 技术，提取 HDR 环境贴图的漫反射辐照度（Irradiance）与预滤波环境映射（Prefilter），确保 Ref 和 Opt 模型在完全相同的复杂物理光照环境下进行渲染。

---

## 3. 误差指标定义

系统计算参考模型 ($Ref$) 与优化模型 ($Opt$) 在 $N$ 个视角下的平均误差。

### 3.1 峰值信噪比 (PSNR)

衡量渲染图像在像素层面的全局颜色保真度。

$$PSNR = 10 \cdot \log_{10} \left( \frac{255^2}{MSE_{color}} \right)$$

- **输入**：PBR 渲染后的 RGB 颜色缓冲区。
- **意义**：值越大表示失真越小。通常 > 30dB 表示图像质量良好，> 40dB 表示极好（人眼难以察觉差异）。

### 3.2 法线一致性误差 (Normal Difference, ND)

衡量模型表面几何法线方向的偏差，反映了光照反射的准确性。

$$L_{nd} = \frac{1}{N} \sum_{k=1}^{N} MSE \left( \mathbf{n}_{ref}^{(k)}, \mathbf{n}_{opt}^{(k)} \right)$$

- **输入**：从 G-Buffer 提取的线性世界空间法线贴图 (World Space Normal Map)。
- **热力图映射**：为了更科学地呈现局部误差，热力图采用法线点积（夹角）映射：$Error = (1.0 - \mathbf{n}_{ref} \cdot \mathbf{n}_{opt}) / 2.0$。
- **意义**：值越小越好。该指标对于评估低模是否保留了高模的曲率特征至关重要。

### 3.3 轮廓误差 (Silhouette Difference, SD)

衡量模型边缘轮廓及内部关键结构线（如窗框、屋脊、墙角）的完整性。系统采用 GPU Shader 加速提取特征线。

$$L_{sd} = \frac{1}{N} \sum_{k=1}^{N} MSE \left( S_{ref}^{(k)}, S_{opt}^{(k)} \right)$$

- **输入**：深度图 (Depth Buffer) 和 法线图 (Normal Map)。
- **特征提取机制** (GPU 并行计算)：
  1. **深度断层**：计算相邻像素**线性深度**的最大绝对差值 $G_{depth}$，用于捕捉模型外部遮挡轮廓。
  2. **法线折角**：计算相邻像素**法线向量的最小点积** $Dot_{normal}$，用于捕捉建筑内部结构死角。
  3. **复合判定**：当 $(G_{depth} > Threshold_{depth}) \lor (Dot_{normal} < Threshold_{normal})$ 时，该像素标记为特征线 ($1.0$)，否则为背景 ($0.0$)。
- **意义**：不仅能反映模型外形的体积坍塌，还能敏锐捕捉到建筑内部关键棱线的丢失情况。

---

## 4. 输出与配置管理 (AppConfig)

系统通过 `src/App/Config.h` 提供高度灵活的参数配置，并产出结构化的数据报告。

- **动态热力图灵敏度 (`colorErrorMultiplier`)**：可自由控制 PSNR 热力图的视觉宽容度。例如设为 `2.5` 时，代表两张图发生 `40%` 的 RGB 相对误差即在热力图上显示为最高警戒（纯红）。系统会根据该配置自动演算并生成对应的 `legend_psnr.png` 像素图例。
- **分层 CSV 报表**：
  - **局部数据**：每个模型的各个视角独立存储在 `output/ModelName/metrics_xxx/` 目录下，便于帧级别追溯。
  - **全局数据**：所有模型的综合平均值统一汇总在 `output/` 根目录的 `metrics_psnr/normal/silhouette.csv` 中，方便直接导入学术图表工具。
- **自定义背景**：支持自定义展示窗口、截图以及热力图的纯色背景色，且完全不干扰 PBR 的 IBL 环境光照计算与底层的误差评估逻辑。

---

## 5. 工程目录结构

```text
VisualMetrics/
├── CMakeLists.txt                # CMake 构建脚本 (已配置自动源文件查找)
├── assets/                       # [资源根目录]
│   ├── hdrtextures/              # HDR 环境贴图
│   ├── refmodel/                 # 参考高模
│   ├── optmodel/                 # 待测低模 (批量处理将自动在此寻找同名文件夹)
│   └── shaders/                  # 着色器源码 (PBR, IBL, Metrics等)
├── output/                       # [输出目录] 自动生成的渲染截图、CSV及图例
├── third_party/                  # 第三方库源码
│   └── stb/                      # stb_image, stb_image_write
├── src/                          # 源代码根目录
│   ├── main.cpp                  # 程序入口
│   ├── pch.h                     # 预编译头文件 (GLM, GLFW, STL)
│   ├── App/                      # [模块] 应用程序逻辑
│   │   ├── Application.h/cpp     # 主控类 (初始化, 渲染循环, 资源复用)
│   │   ├── BatchProcessor.h/cpp  # 自动化批量处理调度系统
│   │   └── Config.h              # 全局配置核心
│   │
│   ├── Scene/                    # [模块] 场景与数据
│   │   ├── Scene.h               # 场景容器
│   │   ├── Model.h/cpp           # 模型加载 (Assimp 封装)
│   │   ├── Mesh.h                # 网格数据结构
│   │   └── CameraSampler.h/cpp   # 相机采样逻辑 (斐波那契球)
│   │
│   ├── Renderer/                 # [模块] 渲染管线
│   │   ├── PBRRenderer.h/cpp     # PBR 渲染器
│   │   ├── IBLBaker.h/cpp        # IBL 预计算 (Irradiance/Prefilter)
│   │   └── Shader.h/cpp          # Shader 编译工具
│   │
│   ├── Resources/                # [模块] 资源管理
│   │   └── ResourceManager.h/cpp # 模型/纹理缓存管理
│   │
│   ├── Metrics/                  # [模块] 评估与可视化
│   │   ├── MetricVisualizer.h/cpp# 分屏对比渲染
│   │   └── Evaluator.h/cpp       # 核心误差计算及热力图生成映射
│   │
│   └── Utils/                    # [模块] 通用工具
│       ├── FileSystemUtils.h     # 文件与路径工具
│       └── GeometryUtils.h/cpp   # 基础几何体 (Cube, Quad)
```

## 6. 环境依赖

- 编译器: 支持 C++17 (MSVC 2019+, GCC 8+, Clang 6+).

- 构建工具: CMake 3.20+.

- 第三方库 (推荐使用 VCPKG 安装):

  - `glfw3`

  - `glad`

  - `glm`

  - `assimp`

  - `stb`

