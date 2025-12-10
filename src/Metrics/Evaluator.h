#pragma once

namespace Metrics {

    // 存储单个视角的评估结果
    struct MetricResult {
        double psnr;
        double mse_color;
        double mse_normal;    // L_n (法线误差)
        double mse_silhouette;// L_sil (轮廓误差)
    };

    class Evaluator {
    public:
        /**
         * @brief 计算两幅 RGB 图像 (uint8) 的 MSE 和 PSNR
         * @param img1 参考图像数据
         * @param img2 优化图像数据
         * @return std::pair<mse, psnr>
         */
        static std::pair<double, double> ComputePSNR(
                const std::vector<unsigned char>& img1,
                const std::vector<unsigned char>& img2,
                int width, int height
        );

        /**
         * @brief 计算法线一致性误差 (MSE of Normal Maps)
         * @param nMap1 参考法线数据 (float RGB)
         * @param nMap2 优化法线数据
         */
        static double ComputeNormalError(
                const std::vector<float>& nMap1,
                const std::vector<float>& nMap2,
                int width, int height
        );

        /**
         * @brief 计算轮廓误差 (MSE of Binary Silhouette Maps)
         * @param sil1 参考轮廓图 (0 or 255)
         * @param sil2 优化轮廓图
         */
        static double ComputeSilhouetteError(
                const std::vector<unsigned char>& sil1,
                const std::vector<unsigned char>& sil2,
                int width, int height
        );
    };
}