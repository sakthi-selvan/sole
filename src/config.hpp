#pragma once

#include <string>

struct AppConfig {
    std::string api_key;
    std::string api_base = "https://integrate.api.nvidia.com/v1";
    std::string model = "deepseek-ai/deepseek-v4-flash-0731";
    std::string data_dir;
    std::string config_dir;
};

AppConfig load_config();
std::string pid_path(const AppConfig& cfg);
std::string default_font_path();
