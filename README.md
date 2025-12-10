# VisualMetrics: 基于 IBL 的 PBR 模型视觉误差评估系统

## 1. 项目简介

VisualMetrics 是一个基于现代 OpenGL (C++17) 开发的视觉质量评估工具。它旨在通过物理渲染 (PBR) 管线，客观量化三维模型在几何简化（Simplification）或优化（Optimization）前后的视觉差异。

该系统模拟标准化的“虚拟摄影”环境，自动生成多视角对比图像，并计算 PSNR (峰值信噪比)、ND (法线一致性误差) 和 SD (轮廓误差) 三项关键指标，最终输出 CSV 数据报表及可视化热力图。

## 2. 渲染场景生成

为了确保评估的客观性和一致性，本工程构建了一套标准化的虚拟采样环境。

### 2.1 模型归一化 (Normalization)

由于输入模型（Ref 和 Base）可能具有不同的尺度和原点偏移，系统在加载阶段执行自动归一化：

1. 计算包围盒 (AABB)：遍历模型所有网格顶点，计算轴对齐包围盒。

2. 中心化：将模型几何中心平移至世界坐标原点 $(0,0,0)$。

3. 统一缩放：计算包围盒的最长对角线，将模型统一缩放到半径为 $1.0$ 的单位球体内。

### 2.2 斐波那契视点采样 (Fibonacci Viewpoint Sampling)

为了全方位覆盖模型的几何特征，系统摒弃了传统的单一视角，采用球面均匀采样策略：

- 采样算法：基于斐波那契格点 (Fibonacci Lattice) 算法。

- 采样参数：

  - 视点数量 $N = 64$。

  - 摄像机所在球体半径 $R = 2.0$ (完全包裹半径为1的模型)。

  - 摄像机方向：始终看向原点 $(0,0,0)$。

  - 视场角 (FOV)：根据模型包围球自适应计算，确保模型充满画面且不被裁剪。

- 优势：相比经纬度网格采样，斐波那契采样在球面上分布更加均匀，且面积加权一致。

### 2.3 统一光照环境 (Unified IBL Lighting)

采用基于图像的照明 (Image Based Lighting) 技术，确保 Ref 和 Base 模型在完全相同的光照环境下渲染：


## 3. 误差指标定义

系统计算参考模型 ($Ref$) 与优化模型 ($Opt$) 在 $N=64$ 个视角下的平均误差。

### 3.1 峰值信噪比 (PSNR)

衡量渲染图像在像素层面的保真度。

$$PSNR = 10 \cdot \log_{10} \left( \frac{MAX_I^2}{MSE_{color}} \right)$$

- 输入：PBR 渲染后的 RGB 颜色缓冲区。

- 定义：$MAX_I$ 为像素最大值 (1.0)，$MSE$ 为均方误差。

- 意义：值越大表示失真越小。通常 > 30dB 表示人眼难以察觉差异。

### 3.2 法线一致性误差 (Normal Difference, ND)

衡量模型表面几何法线方向的偏差，反映了光照反射的准确性。

$$L_{nd} = \frac{1}{N} \sum_{k=1}^{N} MSE \left( \mathbf{n}_{ref}^{(k)}, \mathbf{n}_{opt}^{(k)} \right)$$

- 输入：世界空间法线贴图 (World Space Normal Map)。

- 处理：将法线向量 $\mathbf{n} \in [-1, 1]$ 映射到颜色空间 $[0, 1]$ 输出 ($RGB = (\mathbf{n}+1)/2$)。

- 意义：值越小越好。该指标对于评估低模是否保留了高模的曲率特征至关重要。

### 3.3 轮廓误差 (Silhouette Difference, SD)

衡量模型边缘轮廓及内部关键结构线（如窗框、屋脊）的完整性。

$$L_{sd} = \frac{1}{N} \sum_{k=1}^{N} MSE \left( S_{ref}^{(k)}, S_{opt}^{(k)} \right)$$

- 输入：深度图 (Depth Buffer) 和 法线图。

- 特征提取：

  1. 计算梯度：$G = |\nabla Depth| + |\nabla Normal|$。

  2. 二值化：若 $G > Threshold$，则像素标记为轮廓 ($1$)，否则为背景 ($0$)。

- 意义：值越小越好。反映了简化模型是否发生了体积坍塌或轮廓丢失。

##  4. 工程目录结构
```text
VisualMetrics/
├── CMakeLists.txt                # CMake 构建脚本 (已配置 PCH 和自动源文件查找)
├── assets/                       # [资源根目录]
│   ├── hdrtextures/              # HDR 环境贴图
│   ├── refmodel/                 # 参考高模
│   ├── optmodel/                 # 待测低模
│   └── shaders/                  # 着色器源码
├── output/                       # [输出] 渲染截图或评估报告
├── third_party/                  # 第三方库源码
│   └── stb/                      # stb_image, stb_image_write
├── src/                          # 源代码根目录
│   ├── main.cpp                  # 程序入口 (仅负责实例化 App)
│   ├── pch.h                     # 预编译头文件 (GLM, GLFW, STL)
│   ├── App/                      # [模块] 应用程序逻辑
│   │   ├── Application.h/cpp     # 主控类 (初始化, 渲染循环, 状态管理)
│   │   └── Config.h              # 全局配置 (分辨率, 路径常量)
│   │
│   ├── Scene/                    # [模块] 场景与数据
│   │   ├── Scene.h               # 场景容器 (持有 Model 和 IBLMaps)
│   │   ├── Model.h/cpp           # 模型加载 (Assimp 封装)
│   │   ├── Mesh.h                # 网格数据结构
│   │   └── CameraSampler.h/cpp   # 相机采样逻辑 (斐波那契球)
│   │
│   ├── Renderer/                 # [模块] 渲染管线
│   │   ├── PBRRenderer.h/cpp     # PBR 渲染器 (只负责画)
│   │   ├── IBLBaker.h/cpp        # IBL 预计算 (烘焙 Irradiance/Prefilter)
│   │   └── Shader.h/cpp          # Shader 编译工具
│   │
│   ├── Resources/                # [模块] 资源管理
│   │   └── ResourceManager.h/cpp # 模型/纹理缓存管理
│   │
│   ├── Metrics/                  # [模块] 评估与可视化
│   │   ├── MetricVisualizer.h/cpp# 分屏对比与误差热力图绘制
│   │   ├── Evaluator.h/cpp       # 误差计算逻辑
│   │   └── ImageUtils.h/cpp      # 图像处理辅助
│   │
│   └── Utils/                    # [模块] 通用工具
│       ├── FileSystemUtils.h     # 文件路径搜索工具
│       └── GeometryUtils.h/cpp   # 基础几何体 (Cube, Quad)
```


## 5环境依赖

- 编译器: 支持 C++17 (MSVC 2019+, GCC 8+, Clang 6+).

- 构建工具: CMake 3.20+.

- 第三方库 (推荐使用 VCPKG 安装):

  - `glfw3`

  - `glad`

  - `glm`

  - `assimp`

  - `stb`

