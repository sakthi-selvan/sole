#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

inline uint32_t utf8_next(const std::string& s, size_t& i) {
    if (i >= s.size()) return 0;
    const unsigned char c = static_cast<unsigned char>(s[i]);
    ++i;
    if (c < 0x80) return c;
    auto take = [&]() -> unsigned char {
        if (i >= s.size()) return 0;
        unsigned char n = static_cast<unsigned char>(s[i]);
        ++i;
        return n;
    };
    if ((c & 0xE0) == 0xC0 && i < s.size()) {
        return ((c & 0x1F) << 6) | (take() & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && i + 1 < s.size()) {
        unsigned char b1 = take();
        unsigned char b2 = take();
        return ((c & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && i + 2 < s.size()) {
        unsigned char b1 = take();
        unsigned char b2 = take();
        unsigned char b3 = take();
        return ((c & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
    }
    return 0xFFFD;
}

inline void utf8_prev(const std::string& s, size_t& i) {
    if (i == 0) return;
    --i;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
}

inline std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

inline std::string json_unescape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) {
            o += s[i];
            continue;
        }
        char n = s[++i];
        switch (n) {
            case 'n': o += '\n'; break;
            case 'r': o += '\r'; break;
            case 't': o += '\t'; break;
            case '"': o += '"'; break;
            case '\\': o += '\\'; break;
            case '/': o += '/'; break;
            case 'u':
                if (i + 4 < s.size()) {
                    o += '?';
                    i += 4;
                }
                break;
            default: o += n; break;
        }
    }
    return o;
}

inline std::string extract_json_string(const std::string& json, const std::string& key) {
    const std::string pat = "\"" + key + "\"";
    size_t pos = 0;
    while ((pos = json.find(pat, pos)) != std::string::npos) {
        size_t i = pos + pat.size();
        while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) ++i;
        if (i >= json.size() || json[i] != ':') {
            pos += 1;
            continue;
        }
        ++i;
        while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) ++i;
        if (i < json.size() && json.compare(i, 4, "null") == 0) return {};
        if (i >= json.size() || json[i] != '"') return {};
        ++i;
        std::string raw;
        while (i < json.size()) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                raw += json[i];
                raw += json[i + 1];
                i += 2;
                continue;
            }
            if (json[i] == '"') break;
            raw += json[i++];
        }
        return json_unescape(raw);
    }
    return {};
}
