#include "ImageUtils.h"

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

        // 遍历图像 (跳过边缘 1 像素)
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int idx = GetIdx(x, y, width);

                // --- 1. 深度梯度 (十字差分) ---
                float dL = depthMap[GetIdx(x - 1, y, width)];
                float dR = depthMap[GetIdx(x + 1, y, width)];
                float dT = depthMap[GetIdx(x, y + 1, width)];
                float dB = depthMap[GetIdx(x, y - 1, width)];

                float grad_d = std::abs(dL - dR) + std::abs(dT - dB);

                // --- 2. 法线梯度 ---
                auto GetN = [&](int px, int py) -> glm::vec3 {
                    int i = GetIdx(px, py, width) * 3;
                    return glm::vec3(normalMap[i], normalMap[i+1], normalMap[i+2]);
                };

                glm::vec3 nL = GetN(x - 1, y);
                glm::vec3 nR = GetN(x + 1, y);
                glm::vec3 nT = GetN(x, y + 1);
                glm::vec3 nB = GetN(x, y - 1);

                float grad_n = glm::length(nL - nR) + glm::length(nT - nB);

                // --- 3. 阈值判定 ---
                if (grad_d > depthThresh || grad_n > normalThresh) {
                    silhouette[idx] = 255;
                } else {
                    silhouette[idx] = 0;
                }
            }
        }
        return silhouette;
    }

    float ImageUtils::LinearizeDepth(float d, float zNear, float zFar) {
        // [0,1] -> NDC [-1, 1] -> View Space Linear Depth
        float ndc = d * 2.0f - 1.0f;
        return (2.0f * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));
    }
}