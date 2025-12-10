#pragma once


namespace Scene {

    struct CameraSample {
        int index;
        glm::vec3 position;
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
    };

    class CameraSampler {
    public:
        // 生成采样点
        // sampleCount: 采样点数量 (如 64)
        // sphereRadius: 相机所在的球半径 (如 2.0)
        // aspect: 屏幕宽高比
        // jitterStrength: 随机抖动强度 (0.0 表示规则分布)
        static std::vector<CameraSample> GenerateSamples(
                int sampleCount,
                float sphereRadius,
                float aspect,
                float jitterStrength = 0.0f
        );

    private:
        // 计算刚好包围模型的 FOV
        static float CalculateAdaptiveFOV(float modelRadius, float cameraDistance);
    };
}