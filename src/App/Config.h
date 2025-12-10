#pragma once

#include <string>

struct AppConfig {
    struct Window {
        int width = 1800;
        int height = 600;
        const char* title = "VisualMetrics";
    } window;

    struct Render {
        int width = 1024;
        int height = 1024;
        bool useIBL = true;
    } render;

    struct Paths {
        std::string assetRoot = "assets/";
        std::string hdrDir = "hdrtextures";
        std::string refModelDir = "refmodel";
    } paths;
};