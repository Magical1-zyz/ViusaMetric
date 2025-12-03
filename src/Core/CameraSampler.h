#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Core {

    // 单个采样点的数据结构
    struct CameraSample {
        int id;                 // 视点序号 (0-63)
        glm::vec3 position;     // 世界坐标位置
        glm::mat4 view;         // View Matrix
        glm::mat4 projection;   // Projection Matrix
    };

    class CameraSampler {
    public:
        /**
         * @brief 生成斐波那契半球采样的相机数据
         * * @param sampleCount 采样点数量 (默认 64)
         * @param sphereRadius 摄像机所在的球体半径 (默认 2.0)
         * @param jitterStrength 随机抖动强度 (0.0 - 0.1, 推荐 0.02)
         * @param aspect 屏幕宽高比 (width / height)
         * @return std::vector<CameraSample> 
         */
        static std::vector<CameraSample> GenerateSamples(
                int sampleCount,
                float sphereRadius,
                float aspect,
                float jitterStrength = 0.02f
        );

        /**
         * @brief 根据模型包围球计算合适的 FOV (确保完全容纳)
         * 对于 R=1, Dist=2 的情况，理论最小 FOV 为 60度
         */
        static float CalculateAdaptiveFOV(float modelRadius, float cameraDistance);
    };

}