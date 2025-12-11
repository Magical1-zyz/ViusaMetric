#pragma once
#include "Renderer/Shader.h"

namespace Metrics {
    class MetricVisualizer {
    public:
        MetricVisualizer(int scrWidth, int scrHeight);

        // 计算并绘制最终的组合图
        // texRef/texBase: 从 PBRRenderer 得到的纹理 ID
        // mode: 0=PSNR(Color), 1=ND(Normal), 2=SD(Depth+Normal)
        void RenderComparison(unsigned int texRef, unsigned int texBase, unsigned int texHeatmap);
        void RenderComposite(unsigned int refTex, unsigned int optTex, int mode);

    private:
        int width, height;
        unsigned int quadVAO = 0;

        std::unique_ptr<Renderer::Shader> errorVisShader;
        std::unique_ptr<Renderer::Shader> simpleTextureShader;
        std::unique_ptr<Renderer::Shader> legendShader;
        std::unique_ptr<Renderer::Shader> compositeShader;

        void RenderQuad();
    };
}