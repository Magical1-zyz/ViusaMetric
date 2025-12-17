#include "App/Config.h"
#include "App/Application.h"
#include "App/BatchProcessor.h"

int main() {
    // 1. 配置阶段
    AppConfig config;

    // 2. 核心对象实例化
    Application app(config);

    // 系统初始化 (窗口、OpenGL等)
    if (!app.InitSystem()) {
        std::cerr << "[Fatal] System init failed." << std::endl;
        return -1;
    }

    // 3. 业务对象实例化
    BatchProcessor processor(config, app);

    // 4. 执行
    processor.RunBatch();

    return 0;
}