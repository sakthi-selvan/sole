#pragma once

#include <string>

struct AppConfig {
    std::string api_key;
    std::string api_base = "https://api.groq.com/openai/v1";
    std::string model = "openai/gpt-oss-20b";
    std::string data_dir;
    std::string config_dir;
};

AppConfig load_config();
std::string pid_path(const AppConfig& cfg);
std::string default_font_path();
