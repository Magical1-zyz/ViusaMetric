#include "Evaluator.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace Metrics {

    std::pair<double, double> Evaluator::ComputePSNR(
            const std::vector<unsigned char>& img1,
            const std::vector<unsigned char>& img2,
            int width, int height
    ) {
        (void)width;
        (void)height;

        if (img1.size() != img2.size()) {
            std::cerr << "[Metric] Error: Image sizes do not match for PSNR!" << std::endl;
            return {0.0, 0.0};
        }

        double sumSqDiff = 0.0;
        size_t totalPixels = img1.size();

        for (size_t i = 0; i < totalPixels; ++i) {
            double v1 = static_cast<double>(img1[i]) / 255.0;
            double v2 = static_cast<double>(img2[i]) / 255.0;

            double diff = v1 - v2;
            sumSqDiff += diff * diff;
        }

        double mse = sumSqDiff / (double)totalPixels;

        if (mse < 1e-10) return {0.0, 99.99};

        double maxVal = 1.0;
        double psnr = 10.0 * std::log10((maxVal * maxVal) / mse);

        return {mse, psnr};
    }

    double Evaluator::ComputeNormalError(
            const std::vector<float>& nMap1,
            const std::vector<float>& nMap2,
            int width, int height
    ) {
        (void)width;
        (void)height;

        if (nMap1.size() != nMap2.size()) {
            std::cerr << "[Metric] Error: Normal map sizes do not match!" << std::endl;
            return 0.0;
        }

        double sumSqDiff = 0.0;
        size_t totalComponents = nMap1.size();

        for (size_t i = 0; i < totalComponents; ++i) {
            double v1 = static_cast<double>(nMap1[i]);
            double v2 = static_cast<double>(nMap2[i]);

            double diff = v1 - v2;
            sumSqDiff += diff * diff;
        }

        return sumSqDiff / static_cast<double>(totalComponents);
    }

    double Evaluator::ComputeSilhouetteError(
            const std::vector<unsigned char>& sil1,
            const std::vector<unsigned char>& sil2,
            int width, int height
    ) {
        (void)width;
        (void)height;

        if (sil1.size() != sil2.size()) return 0.0;

        double sumSqDiff = 0.0;
        for (size_t i = 0; i < sil1.size(); ++i) {
            // 确保二值化：有模型为1.0，无模型为0.0
            double v1 = (sil1[i] > 0) ? 1.0 : 0.0;
            double v2 = (sil2[i] > 0) ? 1.0 : 0.0;

            // [修改点1] 核心修复：使用绝对差值计算误差
            // 只有当两者完全一致(1,1 或 0,0)时 diff 为 0
            // 只要有一个不一样(1,0 或 0,1)，diff 就为 1 (误差)
            double diff = std::abs(v1 - v2);

            sumSqDiff += diff * diff;
        }

        return sumSqDiff / (double)sil1.size();
    }

    void Evaluator::ValueToColor(float value, unsigned char& r, unsigned char& g, unsigned char& b) {
        value = std::max(0.0f, std::min(1.0f, value));

        float floatR = 0.0f;
        float floatG = 0.0f;
        float floatB = 0.0f;

        if (value < 0.5f) {
            float t = value * 2.0f;
            floatR = 0.0f;
            floatG = t;
            floatB = 1.0f - t;
        } else {
            float t = (value - 0.5f) * 2.0f;
            floatR = t;
            floatG = 1.0f - t;
            floatB = 0.0f;
        }

        // 使用平滑函数增强视觉效果
        float pi = 3.1415926f;
        auto smoothstep = [](float edge0, float edge1, float x) {
            x = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));
            return x * x * (3 - 2 * x);
        };

        floatR = smoothstep(0.5f, 0.8f, value);
        floatG = std::sin(value * pi);
        floatB = smoothstep(0.5f, 0.2f, value);

        r = static_cast<unsigned char>(floatR * 255.0f);
        g = static_cast<unsigned char>(floatG * 255.0f);
        b = static_cast<unsigned char>(floatB * 255.0f);
    }

    std::vector<unsigned char> Evaluator::GenerateHeatmap(
            const std::vector<unsigned char>& refBytes,
            const std::vector<float>& refFloats,
            const std::vector<unsigned char>& optBytes,
            const std::vector<float>& optFloats,
            int width, int height,
            int mode
    ) {
        std::vector<unsigned char> heatmap(width * height * 4); // RGBA

        for (int i = 0; i < width * height; ++i) {
            bool isBackground = false;

            if (mode == 1) { // Normal
                if (refFloats[i*3] == 0.0f && refFloats[i*3+1] == 0.0f && refFloats[i*3+2] == 0.0f) isBackground = true;
                if (optFloats[i*3] == 0.0f && optFloats[i*3+1] == 0.0f && optFloats[i*3+2] == 0.0f) isBackground = true;
            } else { // Color/Silhouette
                // 背景判定：只有当 Ref 和 Opt 都在该像素位置为 0 (黑色/无内容) 时，才视为背景
                // 如果 Ref 有内容 (255) 而 Opt 没内容 (0)，则 isBackground = false，会进入误差计算逻辑
                bool refIsBlack = (refBytes[i*3] == 0 && refBytes[i*3+1] == 0 && refBytes[i*3+2] == 0);
                bool optIsBlack = (optBytes[i*3] == 0 && optBytes[i*3+1] == 0 && optBytes[i*3+2] == 0);

                if (refIsBlack && optIsBlack) {
                    isBackground = true;
                }
            }

            if (isBackground) {
                heatmap[i * 4 + 0] = 0;
                heatmap[i * 4 + 1] = 0;
                heatmap[i * 4 + 2] = 0;
                heatmap[i * 4 + 3] = 255;
                continue;
            }

            float diff = 0.0f;

            if (mode == 0) { // Color / PSNR
                float r1 = refBytes[i * 3 + 0] / 255.0f;
                float g1 = refBytes[i * 3 + 1] / 255.0f;
                float b1 = refBytes[i * 3 + 2] / 255.0f;

                float r2 = optBytes[i * 3 + 0] / 255.0f;
                float g2 = optBytes[i * 3 + 1] / 255.0f;
                float b2 = optBytes[i * 3 + 2] / 255.0f;

                float dr = r1 - r2;
                float dg = g1 - g2;
                float db = b1 - b2;

                diff = std::sqrt(dr*dr + dg*dg + db*db);
                diff *= 5.0f; // 放大误差
            }
            else if (mode == 1) { // Normal
                float n1x = refFloats[i * 3 + 0];
                float n1y = refFloats[i * 3 + 1];
                float n1z = refFloats[i * 3 + 2];

                float n2x = optFloats[i * 3 + 0];
                float n2y = optFloats[i * 3 + 1];
                float n2z = optFloats[i * 3 + 2];

                auto toNormal = [](float v) { return v * 2.0f - 1.0f; };
                float dot = toNormal(n1x) * toNormal(n2x) +
                            toNormal(n1y) * toNormal(n2y) +
                            toNormal(n1z) * toNormal(n2z);

                diff = (1.0f - dot);
                diff *= 2.0f;
            }
            else if (mode == 2) { // Silhouette
                // [修改点1] 轮廓热力图生成
                // 使用绝对差值：
                // Ref=1(255), Opt=0(0) -> abs(1-0) = 1.0 -> 红色 (漏缺)
                // Ref=0(0), Opt=1(255) -> abs(0-1) = 1.0 -> 红色 (多余)
                float v1 = refBytes[i * 3 + 0] / 255.0f;
                float v2 = optBytes[i * 3 + 0] / 255.0f;
                diff = std::abs(v1 - v2);
            }

            unsigned char r, g, b;
            ValueToColor(diff, r, g, b);

            heatmap[i * 4 + 0] = r;
            heatmap[i * 4 + 1] = g;
            heatmap[i * 4 + 2] = b;
            heatmap[i * 4 + 3] = 255;
        }

        return heatmap;
    }
}