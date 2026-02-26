#pragma once
#include <vector>
#include <glm/glm.hpp>

namespace Metrics {

    class ImageUtils {
    public:
        /**
         * @brief 基于深度和法线图，在 CPU 端计算二值化轮廓图（更稳健版）
         *
         * 关键改动：
         * - 深度先线性化（避免非线性 depth 在远处引入噪声）
         * - 法线用“角度差”（1 - dot）而不是直接 length(RGB差)
         * - 背景/无效像素跳过，减少杂线
         *
         * @param depthMap 从 OpenGL 深度纹理读回的 [0,1] 深度
         * @param normalMap RGB float，范围 [0,1]（由 shader 输出 N*0.5+0.5）
         * @param width
         * @param height
         * @param depthThresh 线性深度差阈值（建议 0.02~0.08，和场景尺度有关）
         * @param normalThresh 法线角度差阈值，用 (1-dot) 表示（0.05~0.2 常见）
         * @param zNear
         * @param zFar
         */
        static std::vector<unsigned char> GenerateSilhouetteCPU(
                const std::vector<float>& depthMap,
                const std::vector<float>& normalMap,
                int width, int height,
                float depthThresh = 0.04f,
                float normalThresh = 0.12f,
                float zNear = 0.1f,
                float zFar  = 100.0f
        );

        // 辅助：线性化深度
        static float LinearizeDepth(float d, float zNear, float zFar);
    };
}
