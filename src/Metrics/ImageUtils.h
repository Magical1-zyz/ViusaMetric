#pragma once

namespace Metrics {

    class ImageUtils {
    public:
        // --- 轮廓提取相关 ---

        /**
         * @brief 基于深度和法线图，在 CPU 端计算二值化轮廓图 (对应论文逻辑)
         * @param depthMap 线性深度数据 (float vector)
         * @param normalMap 法线数据 (RGB float vector, 范围 [0,1] 或 [-1,1])
         * @param width 图像宽
         * @param height 图像高
         * @param depthThresh 深度梯度阈值
         * @param normalThresh 法线梯度阈值
         * @return 二值化图像 (0=背景, 255=轮廓)
         */
        static std::vector<unsigned char> GenerateSilhouetteCPU(
                const std::vector<float>& depthMap,
                const std::vector<float>& normalMap,
                int width, int height,
                float depthThresh = 0.05f,
                float normalThresh = 0.2f
        );

        // 辅助：线性化深度 (如果读回来的深度是非线性的)
        static float LinearizeDepth(float d, float zNear, float zFar);
    };
}