#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct Glyph {
    int w = 0, h = 0, xoff = 0, yoff = 0, advance = 0;
    std::vector<unsigned char> cover;
};

class UiFont {
public:
    UiFont() = default;
    ~UiFont();
    UiFont(const UiFont&) = delete;
    UiFont& operator=(const UiFont&) = delete;
    bool load(const std::string& path, float pixel_height);
    const Glyph& glyph(uint32_t cp);
    int measure(const std::string& text);
    int ascent() const { return ascent_; }
    int descent() const { return descent_; }
    int line_height() const { return line_height_; }
    float size() const { return size_; }

private:
    std::vector<unsigned char> ttf_;
    void* info_ = nullptr;
    float scale_ = 1.f;
    float size_ = 14.f;
    int ascent_ = 0, descent_ = 0, line_height_ = 18;
    std::unordered_map<uint32_t, Glyph> cache_;
};
