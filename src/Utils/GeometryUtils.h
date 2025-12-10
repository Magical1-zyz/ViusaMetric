#pragma once

namespace Utils {
    class GeometryUtils {
    public:
        // 绘制单位立方体 (用于天空盒、IBL卷积)
        static void RenderCube();
        // 绘制全屏四边形 (用于后处理、BRDF LUT)
        static void RenderQuad();

    private:
        static unsigned int cubeVAO;
        static unsigned int cubeVBO;
        static unsigned int quadVAO;
        static unsigned int quadVBO;
    };
}