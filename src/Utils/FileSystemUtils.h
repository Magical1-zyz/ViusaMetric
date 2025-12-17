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
    inline std::filesystem::path FindFirstModelFile(const std::filesystem::path& dir) {
        namespace fs = std::filesystem;
        if (!fs::exists(dir) || !fs::is_directory(dir)) return {};

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                // 简单的大小写判断，如有需要可转小写
                if (ext == ".gltf" || ext == ".glb") {
                    return entry.path();
                }
            }
        }
        return {};
    }
}