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
        size_t validPixels = 0;
        size_t totalPixels = nMap1.size() / 3;

        for (size_t i = 0; i < totalPixels; ++i) {
            float r1 = nMap1[i * 3 + 0];
            float g1 = nMap1[i * 3 + 1];
            float b1 = nMap1[i * 3 + 2];

            float r2 = nMap2[i * 3 + 0];
            float g2 = nMap2[i * 3 + 1];
            float b2 = nMap2[i * 3 + 2];

            // 【修改点】由于 shader 中法线执行了 N*0.5+0.5，一个合法的几何法线转换后不可能出现绝对的(0,0,0)
            // 所以，出现 0,0,0 一定是我们刚刚在 glClearBufferfv 中强制刷新的背景。跳过它，防止背景拉低均值。
            if (r1 == 0.0f && g1 == 0.0f && b1 == 0.0f &&
                r2 == 0.0f && g2 == 0.0f && b2 == 0.0f) {
                continue;
            }

            double dr = static_cast<double>(r1 - r2);
            double dg = static_cast<double>(g1 - g2);
            double db = static_cast<double>(b1 - b2);

            sumSqDiff += (dr*dr + dg*dg + db*db);
            validPixels++;
        }

        if (validPixels == 0) return 0.0;

        // 计算 MSE，因为各通道差值在 [0,1]，计算出的均值严格处于 [0, 1] 范围
        return sumSqDiff / (static_cast<double>(validPixels) * 3.0);
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
            double v1 = (sil1[i] > 0) ? 1.0 : 0.0;
            double v2 = (sil2[i] > 0) ? 1.0 : 0.0;
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
        std::vector<unsigned char> heatmap(width * height * 4);

        for (int i = 0; i < width * height; ++i) {
            bool isBackground = false;

            if (mode == 1) { // Normal
                if (refFloats[i*3] == 0.0f && refFloats[i*3+1] == 0.0f && refFloats[i*3+2] == 0.0f &&
                    optFloats[i*3] == 0.0f && optFloats[i*3+1] == 0.0f && optFloats[i*3+2] == 0.0f) {
                    isBackground = true;
                }
            } else { // Color/Silhouette
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
                diff *= 5.0f;
            }
            else if (mode == 1) { // Normal
                float n1x = refFloats[i * 3 + 0];
                float n1y = refFloats[i * 3 + 1];
                float n1z = refFloats[i * 3 + 2];

                float n2x = optFloats[i * 3 + 0];
                float n2y = optFloats[i * 3 + 1];
                float n2z = optFloats[i * 3 + 2];

                auto toNormal = [](float v) { return v * 2.0f - 1.0f; };

                // 还原至 [-1, 1]
                float nx1 = toNormal(n1x), ny1 = toNormal(n1y), nz1 = toNormal(n1z);
                float nx2 = toNormal(n2x), ny2 = toNormal(n2y), nz2 = toNormal(n2z);

                float dot = nx1 * nx2 + ny1 * ny2 + nz1 * nz2;
                dot = std::max(-1.0f, std::min(1.0f, dot));

                // 【修改点】映射到 [0, 1] 区间。(1 - dot)/2，完全一致为0，完全相反为1
                diff = (1.0f - dot) / 2.0f;
            }
            else if (mode == 2) { // Silhouette
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