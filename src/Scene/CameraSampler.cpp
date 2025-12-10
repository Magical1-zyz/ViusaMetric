#include "Scene/CameraSampler.h"


namespace Scene {

    float CameraSampler::CalculateAdaptiveFOV(float modelRadius, float cameraDistance) {
        if (cameraDistance <= modelRadius) return 90.0f;
        float halfFovRad = std::asin(modelRadius / cameraDistance);
        return glm::degrees(halfFovRad) * 2.0f;
    }

    std::vector<CameraSample> CameraSampler::GenerateSamples(int sampleCount, float sphereRadius, float aspect, float jitterStrength) {
        std::vector<CameraSample> samples;
        samples.reserve(sampleCount);

        const float phi = glm::pi<float>() * (3.0f - std::sqrt(5.0f));

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> distJitter(-jitterStrength, jitterStrength);

        // 计算 FOV (+5度余量)
        float fovDegrees = CalculateAdaptiveFOV(1.0f, sphereRadius) + 5.0f;
        glm::mat4 projMatrix = glm::perspective(glm::radians(fovDegrees), aspect, 0.1f, 100.0f);

        for (int i = 0; i < sampleCount; ++i) {
            // 斐波那契采样 (y: 1 -> 0)
            float y = 1.0f - (float)i / (float)(sampleCount - 1);
            float theta = phi * i;

            // 抖动
            float jitterY = (jitterStrength > 0) ? distJitter(gen) : 0.0f;
            float jitterTheta = (jitterStrength > 0) ? distJitter(gen) * 2.0f * glm::pi<float>() : 0.0f;

            float finalY = glm::clamp(y + jitterY, 0.01f, 1.0f);
            float finalRadiusAtY = std::sqrt(1.0f - finalY * finalY);
            float finalTheta = theta + jitterTheta;

            // 笛卡尔坐标
            float x = std::cos(finalTheta) * finalRadiusAtY;
            float z = std::sin(finalTheta) * finalRadiusAtY;

            glm::vec3 position = glm::vec3(x, finalY, z) * sphereRadius;

            // 构建 View Matrix (LookAt)
            // 目标: (0,0,0)
            // 增加稳健的 Up 向量检测，防止在极点翻转
            glm::vec3 target(0.0f);
            glm::vec3 forward = glm::normalize(target - position);
            glm::vec3 globalUp(0.0f, 1.0f, 0.0f);
            if (glm::abs(glm::dot(forward, globalUp)) > 0.99f) {
                globalUp = glm::vec3(0.0f, 0.0f, 1.0f); // 视线平行Y轴时，改用Z轴为Up
            }

            glm::mat4 viewMatrix = glm::lookAt(position, target, globalUp);

            samples.push_back({ i, position, viewMatrix, projMatrix });
        }

        return samples;
    }
}