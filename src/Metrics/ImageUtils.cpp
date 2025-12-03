#include "ImageUtils.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace Metrics {

    // 辅助函数：获取像素索引
    inline int GetIdx(int x, int y, int w) {
        return y * w + x;
    }

    std::vector<unsigned char> ImageUtils::GenerateSilhouetteCPU(
            const std::vector<float>& depthMap,
            const std::vector<float>& normalMap,
            int width, int height,
            float depthThresh,
            float normalThresh
    ) {
        std::vector<unsigned char> silhouette(width * height, 0);

        // 遍历图像内部像素 (跳过边缘以避免越界)
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int idx = GetIdx(x, y, width);

                // --- 1. 计算深度梯度 (Sobel 简化版或十字差分) ---
                // 这里采用文中描述的“相邻像素深度的绝对差值”
                float dC = depthMap[idx];
                float dL = depthMap[GetIdx(x - 1, y, width)];
                float dR = depthMap[GetIdx(x + 1, y, width)];
                float dT = depthMap[GetIdx(x, y + 1, width)];
                float dB = depthMap[GetIdx(x, y - 1, width)];

                float grad_d = std::abs(dL - dR) + std::abs(dT - dB);

                // --- 2. 计算法线梯度 ---
                // 注意：假设传入的 normalMap 是 [r, g, b, r, g, b...] 格式
                // 且数值可能在 [0,1] (纹理读取) 或 [-1,1] (原始数据)
                // 我们这里取向量差的模长
                auto GetN = [&](int px, int py) -> glm::vec3 {
                    int i = GetIdx(px, py, width) * 3;
                    return glm::vec3(normalMap[i], normalMap[i+1], normalMap[i+2]);
                };

                glm::vec3 nL = GetN(x - 1, y);
                glm::vec3 nR = GetN(x + 1, y);
                glm::vec3 nT = GetN(x, y + 1);
                glm::vec3 nB = GetN(x, y - 1);

                // 如果法线存储的是 [0,1]，是否需要解码回 [-1,1] 对梯度幅值影响不大，
                // 只要 Ref 和 Opt 处理方式一致即可。这里直接计算 RGB 空间的距离。
                float grad_n = glm::length(nL - nR) + glm::length(nT - nB);

                // --- 3. 阈值判定 ---
                if (grad_d > depthThresh || grad_n > normalThresh) {
                    silhouette[idx] = 255; // 标记为轮廓
                } else {
                    silhouette[idx] = 0;
                }
            }
        }
        return silhouette;
    }

    float ImageUtils::LinearizeDepth(float d, float zNear, float zFar) {
        // 标准 OpenGL 深度线性化公式
        // ndc = d * 2.0 - 1.0;
        // linear = (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));
        float ndc = d * 2.0f - 1.0f;
        return (2.0f * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));
    }
}