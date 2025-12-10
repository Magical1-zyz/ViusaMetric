#include "MetricVisualizer.h"
#include "GeometryUtils.h" // 引入几何工具类
#include <iostream>

namespace Core {

    // ---------------------------------------------------------
    // 1. 构造函数实现 (修复 LNK2019 错误)
    // ---------------------------------------------------------
    MetricVisualizer::MetricVisualizer(int scrWidth, int scrHeight)
            : width(scrWidth), height(scrHeight)
    {
        // 加载三个必要的 Shader
        // 注意：这里假设 quad.vert 是通用的屏幕四边形顶点着色器

        // 1. 误差热力图 Shader
        errorVisShader = std::make_unique<Shader>(
                "assets/shaders/metrics/quad.vert",
                "assets/shaders/metrics/error_vis.frag"
        );

        // 2. 普通纹理显示 Shader (刚刚新建的那个)
        simpleTextureShader = std::make_unique<Shader>(
                "assets/shaders/metrics/quad.vert",
                "assets/shaders/metrics/textures.frag" // <--- 加个 's'，与你实际文件名保持一致
        );

        // 3. 图例绘制 Shader
        legendShader = std::make_unique<Shader>(
                "assets/shaders/metrics/quad.vert",
                "assets/shaders/metrics/legend.frag"
        );

        // 4. 合成Shader
        compositeShader = std::make_unique<Shader>("assets/shaders/metrics/quad.vert", "assets/shaders/metrics/composite.frag");

        compositeShader->use();
        errorVisShader->setInt("texRef", 0);
        errorVisShader->setInt("texBase", 1);


        // 配置纹理单元 (如果不配置，默认为0也行，但显式配置更安全)
        errorVisShader->use();
        errorVisShader->setInt("texRef", 0);
        errorVisShader->setInt("texBase", 1);

        simpleTextureShader->use();
        simpleTextureShader->setInt("tex", 0);
    }

    // ---------------------------------------------------------
    // 2. 渲染比较图逻辑
    // ---------------------------------------------------------
    void MetricVisualizer::RenderComparison(unsigned int texRef, unsigned int texBase, int mode) {
        int panelW = width / 3;

        // --- A. 左侧视口：Ref 模型原图 ---
        glViewport(0, 0, panelW, height);
        simpleTextureShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texRef);
        RenderQuad(); // 调用私有函数绘制

        // --- B. 中间视口：Base 模型原图 ---
        glViewport(panelW, 0, panelW, height);
        simpleTextureShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texBase);
        RenderQuad();

        // --- C. 右侧视口：误差热力图 ---
        glViewport(panelW * 2, 0, panelW, height);
        errorVisShader->use();
        errorVisShader->setInt("errorType", mode); // 0=Color, 1=Normal, 2=Silhouette

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texRef);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texBase);
        RenderQuad();

        // --- D. 绘制图例 (右下角覆盖) ---
        // 在右侧面板的右边缘绘制一个长条
        int legendW = 20;
        int legendH = 300;
        // 计算绝对坐标：最右侧 - 边距
        int legendX = (panelW * 3) - legendW - 30;
        int legendY = (height - legendH) / 2;

        glViewport(legendX, legendY, legendW, legendH);
        legendShader->use();
        RenderQuad();

        // 恢复整个视口，以免影响后续操作
        glViewport(0, 0, width, height);
    }

    // ---------------------------------------------------------
    // 3. RenderQuad 实现 (修复 LNK2019 错误)
    // ---------------------------------------------------------
    void MetricVisualizer::RenderQuad() {
        // 直接复用 GeometryUtils 中的静态方法
        // 这样避免了重复创建 VAO/VBO
        GeometryUtils::RenderQuad();
    }

    void MetricVisualizer::RenderComposite(unsigned int refTex, unsigned int optTex, int mode) {
        compositeShader->use();
        compositeShader->setInt("viewMode", mode);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, refTex);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, optTex);

        // 绘制全屏四边形
        GeometryUtils::RenderQuad();
    }
}