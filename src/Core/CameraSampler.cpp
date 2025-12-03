#include "CameraSampler.h"

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <random>
#include <iostream>

namespace Core {

    std::vector<CameraSample> CameraSampler::GenerateSamples(int sampleCount, float sphereRadius, float aspect, float jitterStrength) {
        std::vector<CameraSample> samples;
        samples.reserve(sampleCount);

        // 黄金角度 (Golden Angle in radians)
        // pi * (3 - sqrt(5))
        const float phi = glm::pi<float>() * (3.0f - std::sqrt(5.0f));

        // 初始化随机数生成器 (用于抖动)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> distJitter(-jitterStrength, jitterStrength);

        // 计算 FOV: 
        // 模型半径视为1.0 (归一化后), 相机距离设为 sphereRadius
        // 增加 5 度余量防止边缘裁剪
        float fovDegrees = CalculateAdaptiveFOV(1.0f, sphereRadius) + 5.0f;
        glm::mat4 projMatrix = glm::perspective(glm::radians(fovDegrees), aspect, 0.1f, 100.0f);

        for (int i = 0; i < sampleCount; ++i) {
            // 1. 斐波那契格点算法 (Fibonacci Lattice)

            // y 坐标从 1 (顶部) 均匀分布到 0 (赤道)
            // 注意：i 从 0 到 sampleCount-1
            float y = 1.0f - (float)i / (float)(sampleCount - 1);

            // 角度 theta
            float theta = phi * i;

            // 2. 应用随机抖动 (在球面坐标系上抖动，保持 R 不变)
            float jitterY = distJitter(gen);
            float jitterTheta = distJitter(gen) * 2.0f * glm::pi<float>(); // 角度抖动稍微大一点没关系

            // 限制 y+jitter 仍在 [0, 1] 范围内 (保持在上半球)
            float finalY = glm::clamp(y + jitterY, 0.01f, 1.0f); // 0.01f 防止完全在赤道导致 lookAt 奇异
            float finalRadiusAtY = std::sqrt(1.0f - finalY * finalY);
            float finalTheta = theta + jitterTheta;

            // 3. 转换回笛卡尔坐标 (局部单位球)
            float x = std::cos(finalTheta) * finalRadiusAtY;
            float z = std::sin(finalTheta) * finalRadiusAtY;

            // 4. 扩展到目标半径 (Radius = 2.0)
            glm::vec3 position = glm::vec3(x, finalY, z) * sphereRadius;

            // 5. 构建 View Matrix
            // 目标: (0,0,0) 模型中心
            // 上向量: (0,1,0) 全局向上
            glm::mat4 viewMatrix = glm::lookAt(position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            // 6. 存入结果
            samples.push_back({ i, position, viewMatrix, projMatrix });
        }

        return samples;
    }

    float CameraSampler::CalculateAdaptiveFOV(float modelRadius, float cameraDistance) {
        // sin(half_fov) = radius / distance
        // fov = 2 * asin(radius / distance)
        if (cameraDistance <= modelRadius) return 90.0f; // 相机在模型内部，给个大角度
        float halfFovRad = std::asin(modelRadius / cameraDistance);
        return glm::degrees(halfFovRad) * 2.0f;
    }
}