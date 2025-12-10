#pragma once


namespace Renderer {
    // 简单的结构体存储 IBL 结果
    struct IBLMaps {
        unsigned int envCubemap = 0;
        unsigned int irradianceMap = 0;
        unsigned int prefilterMap = 0;
        unsigned int brdfLUT = 0;
    };

    class IBLBaker {
    public:
        // 静态函数：读取 HDR 并生成所有 IBL 贴图
        static IBLMaps BakeIBL(const std::string& hdrPath);
    };
}