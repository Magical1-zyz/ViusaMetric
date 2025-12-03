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

        // numChannels 之前被计算但没使用，直接删掉或用于检查
        // size_t numChannels = img1.size() / (width * height);

        // 1. 计算 MSE
        for (size_t i = 0; i < img1.size(); ++i) {
            double diff = static_cast<double>(img1[i]) - static_cast<double>(img2[i]);
            sumSqDiff += diff * diff;
        }

        double mse = sumSqDiff / (double)img1.size();

        // 2. 计算 PSNR (MAX=255.0)
        if (mse < 1e-9) return {0.0, 99.99};

        double maxVal = 255.0;
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
        for (size_t i = 0; i < nMap1.size(); ++i) {
            double diff = nMap1[i] - nMap2[i];
            sumSqDiff += diff * diff;
        }

        return sumSqDiff / (double)nMap1.size();
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