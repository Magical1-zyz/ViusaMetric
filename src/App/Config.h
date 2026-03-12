#pragma once

#include <string>

struct AppConfig {
    // 窗口显示配置
    struct Window {
        int width = 1800;
        int height = 600;
        std::string title = "VisualMetrics: PBR Evaluation System";
    } window;

    // 离屏渲染与画质配置
    struct Render {
        int width = 1024;    // FBO 分辨率 (影响 Metrics 计算精度)
        int height = 1024;
        bool display = true; // 展示窗口运行
        float delayTime = 0.2f; // 每帧的延迟时间
        float exposure = 1.0f; // PBR 曝光度
        float roughnessDefault = 0.5f;
        float metallicDefault = 0.0f;
        bool refPBR = true;
        bool optPBR = true;

        bool showSkyboxPSNR = false;
        bool showSkyBoxSilhouette = false;
        bool showSkyBoxNormal = false;
        glm::vec3 background = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::vec3 heatmapBackground = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::vec3 silhouetteColor = glm::vec3(0.0f, 0.0f, 0.0f);

        // 颜色误差放大倍率 (用于 PSNR 阶段的热力图展示)
        // 备注: 控制热力图对颜色误差的视觉灵敏度。
        // 倍数为 2.5 时，意味着 25% 的 RGB 相对颜色差异就会在热力图上显示为最高误差(纯红)。
        // 调大此值会让微小的误差显得更严重(飘红)，调小则会增加视觉宽容度。
        float colorErrorMultiplier = 2.5f;
    } render;

    // 采样配置
    struct Sampling {
        int viewCount = 64;   // 斐波那契采样点数量
        float radius = 2.0f;  // 摄像机球体半径
    } sampling;

    // 路径配置
    struct Paths {
        std::string assetsRoot = "assets";
        std::string outputRoot = "output";

        // 子目录 (相对于 assetsRoot)
        std::string hdrDir = "hdrtextures";
        std::string refDir = "refmodel";
        std::string optDir = "optmodel";
    } paths;
};