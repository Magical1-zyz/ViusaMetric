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
        float exposure = 1.0f; // PBR 曝光度
        bool useIBL = true;
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