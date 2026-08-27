#include "config.hpp"

#include <algorithm>
#include <cctype>
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

static void trim(std::string& s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

static void apply_kv(AppConfig& cfg, const std::string& k, const std::string& v) {
    if (k == "GROQ_API_KEY" || k == "NVIDIA_API_KEY" || k == "api_key") cfg.api_key = v;
    else if (k == "GROQ_API_BASE" || k == "NVIDIA_API_BASE" || k == "api_base") cfg.api_base = v;
    else if (k == "GROQ_MODEL" || k == "NVIDIA_MODEL" || k == "model") cfg.model = v;
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
        trim(k);
        trim(v);
        if (!v.empty() && (v.front() == '"' || v.front() == '\'') && v.back() == v.front())
            v = v.substr(1, v.size() - 2);
        trim(v);
        if (v.empty()) continue;
        apply_kv(cfg, k, v);
    }
}

static std::string exe_dir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
    std::string p = buf;
    auto slash = p.rfind('/');
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
}

static void ensure_env_template(const std::string& path) {
    if (access(path.c_str(), F_OK) == 0) return;
    std::ofstream out(path);
    if (!out) return;
    out << "# Put your Groq API key on the next line\n"
        << "GROQ_API_KEY=\n"
        << "GROQ_API_BASE=https://api.groq.com/openai/v1\n"
        << "GROQ_MODEL=groq/compound\n";
    out.close();
    chmod(path.c_str(), 0600);
}

AppConfig load_config() {
    AppConfig cfg;
    cfg.config_dir = home_dir() + "/.config/overlay-chat";
    cfg.data_dir = home_dir() + "/.local/share/overlay-chat";
    mkdir_p(cfg.config_dir);
    mkdir_p(cfg.data_dir);
    const std::string user_env = cfg.config_dir + "/env";
    ensure_env_template(user_env);
    parse_kv_file(user_env, cfg);
    parse_kv_file(cfg.config_dir + "/config", cfg);
    parse_kv_file(".env", cfg);
    parse_kv_file(exe_dir() + "/.env", cfg);
    if (const char* k = std::getenv("GROQ_API_KEY"); k && *k) cfg.api_key = k;
    else if (const char* k = std::getenv("NVIDIA_API_KEY"); k && *k) cfg.api_key = k;
    if (const char* b = std::getenv("GROQ_API_BASE"); b && *b) cfg.api_base = b;
    else if (const char* b = std::getenv("NVIDIA_API_BASE"); b && *b) cfg.api_base = b;
    if (const char* m = std::getenv("GROQ_MODEL"); m && *m) cfg.model = m;
    else if (const char* m = std::getenv("NVIDIA_MODEL"); m && *m) cfg.model = m;
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
