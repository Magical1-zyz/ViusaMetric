#include "MetricVisualizer.h"
#include "Utils/GeometryUtils.h"

namespace Metrics {

    MetricVisualizer::MetricVisualizer(int scrWidth, int scrHeight)
            : width(scrWidth), height(scrHeight)
    {
        // 1. 误差热力图 Shader
        errorVisShader = std::make_unique<Renderer::Shader>(
                "assets/shaders/metrics/quad.vert",
                "assets/shaders/metrics/error_vis.frag"
        );

        // 2. 普通纹理显示 Shader
        simpleTextureShader = std::make_unique<Renderer::Shader>(
                "assets/shaders/metrics/quad.vert",
                "assets/shaders/metrics/textures.frag"
        );

        // 3. 图例绘制 Shader
        legendShader = std::make_unique<Renderer::Shader>(
                "assets/shaders/metrics/quad.vert",
                "assets/shaders/metrics/legend.frag"
        );

        // 4. 合成Shader
        compositeShader = std::make_unique<Renderer::Shader>("assets/shaders/metrics/quad.vert", "assets/shaders/metrics/composite.frag");

        // [修复] 这里之前写成了 errorVisShader，导致 composite shader 无法正确采样
        compositeShader->use();
        compositeShader->setInt("texRef", 0);
        compositeShader->setInt("texOpt", 1);

        errorVisShader->use();
        errorVisShader->setInt("texRef", 0);
        errorVisShader->setInt("texBase", 1);

        simpleTextureShader->use();
        simpleTextureShader->setInt("tex", 0);
    }

    void MetricVisualizer::RenderComparison(unsigned int texRef, unsigned int texBase, unsigned int texHeatmap) {
        int panelW = width / 3;

        // A. 左侧
        glViewport(0, 0, panelW, height);
        simpleTextureShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texRef);
        RenderQuad();

        // B. 中间
        glViewport(panelW, 0, panelW, height);
        simpleTextureShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texBase);
        RenderQuad();

        // C. Heatmap (现在直接当作普通纹理绘制)
        glViewport(panelW * 2, 0, panelW, height);
        simpleTextureShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texHeatmap);
        RenderQuad();

        // D. 图例
        int legendW = 20;
        int legendH = 300;
        int legendX = (panelW * 3) - legendW - 30;
        int legendY = (height - legendH) / 2;
        glViewport(legendX, legendY, legendW, legendH);
        legendShader->use();
        RenderQuad();

        glViewport(0, 0, width, height);
    }

    void MetricVisualizer::RenderQuad() {
        Utils::GeometryUtils::RenderQuad();
    }

    // 该函数在当前 RenderPasses 中未使用，但保留作为接口
    void MetricVisualizer::RenderComposite(unsigned int refTex, unsigned int optTex, int mode) {
        compositeShader->use();
        compositeShader->setInt("viewMode", mode);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, refTex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, optTex);
        Utils::GeometryUtils::RenderQuad();
    }
}