#include "Evaluator.h"

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
            const std::vector<float>& nMap1, // 输入数据已经是 [0, 1] 范围的 RGB
            const std::vector<float>& nMap2, // 输入数据已经是 [0, 1] 范围的 RGB
            int width, int height
    ) {
        // 消除未使用参数警告
        (void)width;
        (void)height;

        // 检查数据大小一致性
        if (nMap1.size() != nMap2.size()) {
            std::cerr << "[Metric] Error: Normal map sizes do not match!" << std::endl;
            return 0.0;
        }

        double sumSqDiff = 0.0;
        size_t totalComponents = nMap1.size();

        // 根据公式计算 MSE: Mean Squared Error
        for (size_t i = 0; i < totalComponents; ++i) {
            double v1 = static_cast<double>(nMap1[i]);
            double v2 = static_cast<double>(nMap2[i]);

            double diff = v1 - v2;
            sumSqDiff += diff * diff;
        }

        // MSE = Sum(diff^2) / Count
        return sumSqDiff / static_cast<double>(totalComponents);
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

    // 颜色映射函数 (Jet Colormap: Blue -> Green -> Red)
    void Evaluator::ValueToColor(float value, unsigned char& r, unsigned char& g, unsigned char& b) {
        value = std::max(0.0f, std::min(1.0f, value));

        // 简单的 Jet 映射逻辑 (模拟 Shader 中的 smoothstep)
        // 0.0 (Blue) -> 0.5 (Green) -> 1.0 (Red)

        float floatR = 0.0f;
        float floatG = 0.0f;
        float floatB = 0.0f;

        if (value < 0.5f) {
            // Blue -> Green
            float t = value * 2.0f; // 0~1
            floatR = 0.0f;
            floatG = t;
            floatB = 1.0f - t;
        } else {
            // Green -> Red
            float t = (value - 0.5f) * 2.0f; // 0~1
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
        std::vector<unsigned char> heatmap(width * height * 4); // RGBA

        for (int i = 0; i < width * height; ++i) {
            bool isBackground = false;

            // 针对不同模式的背景判定
            if (mode == 1) { // Normal (Float)
                // 法线图背景通常是 (0,0,0) [因为我们清屏是0]
                // 正常的法线映射后至少是 (0.5, 0.5, 1.0) 附近
                if (refFloats[i*3] == 0.0f && refFloats[i*3+1] == 0.0f && refFloats[i*3+2] == 0.0f) isBackground = true;
                if (optFloats[i*3] == 0.0f && optFloats[i*3+1] == 0.0f && optFloats[i*3+2] == 0.0f) isBackground = true;
            } else { // Color/Silhouette (Byte)
                // 检查 RGB 是否全 0
                if (refBytes[i*3] == 0 && refBytes[i*3+1] == 0 && refBytes[i*3+2] == 0) isBackground = true;
                if (optBytes[i*3] == 0 && optBytes[i*3+1] == 0 && optBytes[i*3+2] == 0) isBackground = true;
            }

            if (isBackground) {
                // 背景色：黑色，且完全透明 (或者不透明黑色，看你需求)
                heatmap[i * 4 + 0] = 0;
                heatmap[i * 4 + 1] = 0;
                heatmap[i * 4 + 2] = 0;
                heatmap[i * 4 + 3] = 255; // Alpha 255 (显示为黑块)
                continue; // 跳过后续热力计算
            }

            float diff = 0.0f;

            if (mode == 0) { // Color / PSNR
                // 使用 Byte 数据
                float r1 = refBytes[i * 3 + 0] / 255.0f;
                float g1 = refBytes[i * 3 + 1] / 255.0f;
                float b1 = refBytes[i * 3 + 2] / 255.0f;

                float r2 = optBytes[i * 3 + 0] / 255.0f;
                float g2 = optBytes[i * 3 + 1] / 255.0f;
                float b2 = optBytes[i * 3 + 2] / 255.0f;

                float dr = r1 - r2;
                float dg = g1 - g2;
                float db = b1 - b2;

                // 距离
                diff = std::sqrt(dr*dr + dg*dg + db*db);

                // 放大误差以便观察 (与 Shader 保持一致 * 5.0)
                // 或者使用 PSNR 映射逻辑
                // 这里为了直观，先用线性放大
                diff *= 5.0f;
            }
            else if (mode == 1) { // Normal
                // 使用 Float 数据
                float n1x = refFloats[i * 3 + 0]; // 已经是 [0,1]
                float n1y = refFloats[i * 3 + 1];
                float n1z = refFloats[i * 3 + 2];

                float n2x = optFloats[i * 3 + 0];
                float n2y = optFloats[i * 3 + 1];
                float n2z = optFloats[i * 3 + 2];

                // 还原到 [-1, 1] 进行点积计算更准确
                auto toNormal = [](float v) { return v * 2.0f - 1.0f; };
                float dot = toNormal(n1x) * toNormal(n2x) +
                            toNormal(n1y) * toNormal(n2y) +
                            toNormal(n1z) * toNormal(n2z);

                // 角度差近似
                diff = (1.0f - dot);
                diff *= 2.0f; // 放大
            }
            else if (mode == 2) { // Silhouette
                // Byte 数据, 只有 R 通道有用
                float v1 = refBytes[i * 3 + 0] / 255.0f;
                float v2 = optBytes[i * 3 + 0] / 255.0f;
                diff = std::abs(v1 - v2);
            }

            unsigned char r, g, b;
            ValueToColor(diff, r, g, b);

            heatmap[i * 4 + 0] = r;
            heatmap[i * 4 + 1] = g;
            heatmap[i * 4 + 2] = b;
            heatmap[i * 4 + 3] = 255; // Alpha
        }

        return heatmap;
    }
}