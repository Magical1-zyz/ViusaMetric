#include "App/Application.h"
#include "App/Config.h"


int main() {
    // 1. 初始化配置
    AppConfig config;

    // 2. 实例化应用
    Application app(config);

    // 3. 初始化
    if (!app.Init()) {
        std::cerr << "[Fatal Error] Application initialization failed." << std::endl;
        return -1;
    }

    // 4. 运行
    app.Run();

    return 0;
}