#include "ImageUtils.h"
#include <algorithm>
#include <cmath>

namespace Metrics {

    inline int GetIdx(int x, int y, int w) {
        return y * w + x;
    }

    float ImageUtils::LinearizeDepth(float d, float zNear, float zFar) {
        // [0,1] -> NDC [-1, 1] -> View Space Linear Depth
        float ndc = d * 2.0f - 1.0f;
        return (2.0f * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));
    }

    static inline bool IsBackgroundDepth(float d01) {
        // OpenGL 默认远平面深度接近 1.0
        return d01 >= 0.99999f;
    }

    static inline glm::vec3 DecodeNormal01ToWorld(const std::vector<float>& nmap, int idx3) {
        glm::vec3 n(nmap[idx3 + 0], nmap[idx3 + 1], nmap[idx3 + 2]);
        // [0,1] -> [-1,1]
        n = n * 2.0f - 1.0f;
        float len2 = glm::dot(n, n);
        if (len2 < 1e-12f) return glm::vec3(0.0f);
        return n / std::sqrt(len2);
    }

    std::vector<unsigned char> ImageUtils::GenerateSilhouetteCPU(
            const std::vector<float>& depthMap,
            const std::vector<float>& normalMap,
            int width, int height,
            float depthThresh,
            float normalThresh,
            float zNear,
            float zFar
    ) {
        std::vector<unsigned char> silhouette(width * height, 0);

        // 简单健壮性检查
        if ((int)depthMap.size() != width * height) return silhouette;
        if ((int)normalMap.size() != width * height * 3) return silhouette;

        // 遍历图像（跳过边缘 1 像素）
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int idx = GetIdx(x, y, width);

                float dc01 = depthMap[idx];
                if (IsBackgroundDepth(dc01)) {
                    // 背景像素不产出轮廓（让“轮廓”由前景像素触发）
                    silhouette[idx] = 0;
                    continue;
                }

                // 邻域深度（[0,1]）
                float dL01 = depthMap[GetIdx(x - 1, y, width)];
                float dR01 = depthMap[GetIdx(x + 1, y, width)];
                float dT01 = depthMap[GetIdx(x, y + 1, width)];
                float dB01 = depthMap[GetIdx(x, y - 1, width)];

                // 线性化（单位：视空间距离）
                float dc = LinearizeDepth(dc01, zNear, zFar);
                float dL = IsBackgroundDepth(dL01) ? (dc + 1e6f) : LinearizeDepth(dL01, zNear, zFar);
                float dR = IsBackgroundDepth(dR01) ? (dc + 1e6f) : LinearizeDepth(dR01, zNear, zFar);
                float dT = IsBackgroundDepth(dT01) ? (dc + 1e6f) : LinearizeDepth(dT01, zNear, zFar);
                float dB = IsBackgroundDepth(dB01) ? (dc + 1e6f) : LinearizeDepth(dB01, zNear, zFar);

                // 深度梯度：使用中心与邻居差的 max（比 |dL-dR| 更贴近“轮廓/遮挡边”）
                float gd = 0.0f;
                gd = std::max(gd, std::abs(dc - dL));
                gd = std::max(gd, std::abs(dc - dR));
                gd = std::max(gd, std::abs(dc - dT));
                gd = std::max(gd, std::abs(dc - dB));

                // 法线差：用角度差 (1 - dot)，并取 4 邻域最大值
                int c3 = idx * 3;
                glm::vec3 nC = DecodeNormal01ToWorld(normalMap, c3);
                if (glm::length2(nC) < 1e-12f) {
                    silhouette[idx] = 0;
                    continue;
                }

                auto nAt = [&](int px, int py) -> glm::vec3 {
                    int i = GetIdx(px, py, width) * 3;
                    return DecodeNormal01ToWorld(normalMap, i);
                };

                glm::vec3 nL = nAt(x - 1, y);
                glm::vec3 nR = nAt(x + 1, y);
                glm::vec3 nT = nAt(x, y + 1);
                glm::vec3 nB = nAt(x, y - 1);

                auto angDiff = [&](const glm::vec3& a, const glm::vec3& b) -> float {
                    if (glm::length2(b) < 1e-12f) return 0.0f;
                    float d = glm::clamp(glm::dot(a, b), -1.0f, 1.0f);
                    return 1.0f - d; // 0 表示完全一致
                };

                float gn = 0.0f;
                gn = std::max(gn, angDiff(nC, nL));
                gn = std::max(gn, angDiff(nC, nR));
                gn = std::max(gn, angDiff(nC, nT));
                gn = std::max(gn, angDiff(nC, nB));

                // 判定策略：
                // - 深度突变：强轮廓（遮挡边）
                // - 法线突变：结构线/硬边（避免 seam：你的法线被修复后 seam 不会乱跳）
                if (gd > depthThresh || gn > normalThresh) {
                    silhouette[idx] = 255;
                } else {
                    silhouette[idx] = 0;
                }
            }
        }

        return silhouette;
    }
}
