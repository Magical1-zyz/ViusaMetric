// src/main.cpp
#include "App/Application.h"

// 可以通过命令行参数传入配置，这里简化处理
const int SCR_WIDTH = 1800;
const int SCR_HEIGHT = 600;

int main() {
    // 实例化应用对象
    // 可以在这里传入 Config 结构体，而不是散乱的参数
    Application app(SCR_WIDTH, SCR_HEIGHT, "Ref(Lit) vs Opt(Unlit) vs Heatmap");

    // 初始化失败则退出
    if (!app.Init()) {
        std::cerr << "[Fatal Error] Application initialization failed." << std::endl;
        return -1;
    }

    // 进入主循环
    app.Run();

    return 0;
}