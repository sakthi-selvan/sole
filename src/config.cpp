#include "config.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>

#include <vector>

static std::string home_dir() {
    const char* h = std::getenv("HOME");
    if (h && *h) return h;
    passwd* pw = getpwuid(getuid());
    return pw && pw->pw_dir ? pw->pw_dir : ".";
}

static void mkdir_p(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/' && cur.size() > 1) mkdir(cur.c_str(), 0755);
    }
    mkdir(path.c_str(), 0755);
}

static void parse_kv_file(const std::string& path, AppConfig& cfg) {
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("export ", 0) == 0) line = line.substr(7);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        if (!v.empty() && (v.front() == '"' || v.front() == '\'') && v.back() == v.front())
            v = v.substr(1, v.size() - 2);
        if (k == "NVIDIA_API_KEY" || k == "api_key") cfg.api_key = v;
        else if (k == "NVIDIA_API_BASE" || k == "api_base") cfg.api_base = v;
        else if (k == "NVIDIA_MODEL" || k == "model") cfg.model = v;
    }
}

AppConfig load_config() {
    AppConfig cfg;
    cfg.config_dir = home_dir() + "/.config/overlay-chat";
    cfg.data_dir = home_dir() + "/.local/share/overlay-chat";
    mkdir_p(cfg.config_dir);
    mkdir_p(cfg.data_dir);
    parse_kv_file(cfg.config_dir + "/env", cfg);
    parse_kv_file(cfg.config_dir + "/config", cfg);
    if (const char* k = std::getenv("NVIDIA_API_KEY"); k && *k) cfg.api_key = k;
    if (const char* b = std::getenv("NVIDIA_API_BASE"); b && *b) cfg.api_base = b;
    if (const char* m = std::getenv("NVIDIA_MODEL"); m && *m) cfg.model = m;
    return cfg;
}

std::string pid_path(const AppConfig& cfg) {
    return cfg.data_dir + "/overlay-chat.pid";
}

std::string default_font_path() {
    const char* candidates[] = {
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    };
    for (const char* p : candidates) {
        if (access(p, R_OK) == 0) return p;
    }
    return {};
}
