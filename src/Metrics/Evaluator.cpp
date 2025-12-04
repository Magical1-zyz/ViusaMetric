#include "Evaluator.h"
#include <algorithm>
#include <iostream>
#include <cmath>

namespace Metrics {

    std::pair<double, double> Evaluator::ComputePSNR(
            const std::vector<unsigned char>& img1,
            const std::vector<unsigned char>& img2,
            int width, int height
    ) {
        // 消除未使用参数警告
        (void)width;
        (void)height;

        if (img1.size() != img2.size()) {
            std::cerr << "[Metric] Error: Image sizes do not match for PSNR!" << std::endl;
            return {0.0, 0.0};
        }

        double sumSqDiff = 0.0;
        size_t totalPixels = img1.size();

        // 1. 计算 MSE
        for (size_t i = 0; i < totalPixels; ++i) {
            double v1 = static_cast<double>(img1[i]) / 255.0;
            double v2 = static_cast<double>(img2[i]) / 255.0;

            double diff = v1 - v2;
            sumSqDiff += diff * diff;
        }

        double mse = sumSqDiff / (double)totalPixels;

        // 2. 计算 PSNR
        if (mse < 1e-10) return {0.0, 99.99}; // 避免除以零，表示完全相同

        double maxVal = 1.0;
        double psnr = 10.0 * std::log10((maxVal * maxVal) / mse);

        return {mse, psnr};
    }

    double Evaluator::ComputeNormalError(
            const std::vector<float>& nMap1,
            const std::vector<float>& nMap2,
            int width, int height
    ) {
        // 消除未使用参数警告
        (void)width;
        (void)height;

        if (nMap1.size() != nMap2.size()) return 0.0;

        double sumSqDiff = 0.0;
        size_t totalComponents = nMap1.size();

        for (size_t i = 0; i < totalComponents; ++i) {
            // 假设 nMap 中的原始数据是世界空间法线 (范围 -1 到 1)
            double n1 = static_cast<double>(nMap1[i]);
            double n2 = static_cast<double>(nMap2[i]);

            double rgb1 = (n1 + 1.0) * 0.5;
            double rgb2 = (n2 + 1.0) * 0.5;

            // 钳制在 [0,1] 范围内，防止浮点误差导致的轻微越界
            // (虽然理论上 n 在 -1~1 之间，但安全起见可以 clamp，这里为了性能暂略，假设数据合法)

            double diff = rgb1 - rgb2;
            sumSqDiff += diff * diff;
        }

        return sumSqDiff / totalComponents;
    }

    double Evaluator::ComputeSilhouetteError(
            const std::vector<unsigned char>& sil1,
            const std::vector<unsigned char>& sil2,
            int width, int height
    ) {
        // 消除未使用参数警告
        (void)width;
        (void)height;

        if (sil1.size() != sil2.size()) return 0.0;

        double sumSqDiff = 0.0;
        for (size_t i = 0; i < sil1.size(); ++i) {
            double v1 = (sil1[i] > 0) ? 1.0 : 0.0;
            double v2 = (sil2[i] > 0) ? 1.0 : 0.0;

            double diff = v1 - v2;
            sumSqDiff += diff * diff;
        }

        return sumSqDiff / (double)sil1.size();
    }
}