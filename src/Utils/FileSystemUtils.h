#pragma once

namespace Utils {
    namespace fs = std::filesystem;

    inline bool SetupWorkingDirectory() {
        fs::path current = fs::current_path();
        for (int i = 0; i < 5; ++i) {
            if (fs::exists(current / "assets/shaders/pbr/pbr.vert")) {
                fs::current_path(current);
                return true;
            }
            if (current.has_parent_path()) current = current.parent_path();
            else break;
        }
        return false;
    }

    inline std::string FindFirstFileByExt(const std::string& folder, const std::vector<std::string>& extensions) {
        if (!fs::exists(folder)) return "";
        for (const auto& entry : fs::recursive_directory_iterator(folder)) {
            if (entry.is_directory()) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            for (const auto& target : extensions) {
                if (ext == target) return entry.path().string();
            }
        }
        return "";
    }
    inline std::filesystem::path FindFirstModelFile(const std::filesystem::path& dir, const std::string& targetExt) {
        namespace fs = std::filesystem;
        if (!fs::exists(dir) || !fs::is_directory(dir)) return {};

        // 统一格式化期望的后缀名（转小写，并确保有 "." 前缀）
        std::string expectedExt = targetExt;
        std::transform(expectedExt.begin(), expectedExt.end(), expectedExt.begin(), ::tolower);
        if (!expectedExt.empty() && expectedExt[0] != '.') {
            expectedExt = "." + expectedExt;
        }

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                // 统一将找到的文件后缀转为小写进行比较
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                // 根据 Config 传入的扩展名进行精确匹配
                if (ext == expectedExt) {
                    return entry.path();
                }
            }
        }
        return {};
    }
}