#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "font.hpp"
#include "utf8.hpp"

#include <fstream>
#include <stdexcept>

UiFont::~UiFont() {
    delete static_cast<stbtt_fontinfo*>(info_);
    info_ = nullptr;
}

bool UiFont::load(const std::string& path, float pixel_height) {
    delete static_cast<stbtt_fontinfo*>(info_);
    info_ = nullptr;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    ttf_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (ttf_.empty()) return false;

    auto* info = new stbtt_fontinfo();
    if (!stbtt_InitFont(info, ttf_.data(), stbtt_GetFontOffsetForIndex(ttf_.data(), 0))) {
        delete info;
        return false;
    }
    info_ = info;
    size_ = pixel_height;
    scale_ = stbtt_ScaleForPixelHeight(info, pixel_height);
    int a, d, g;
    stbtt_GetFontVMetrics(info, &a, &d, &g);
    ascent_ = static_cast<int>(a * scale_ + 0.5f);
    descent_ = static_cast<int>(d * scale_ - 0.5f);
    line_height_ = ascent_ - descent_ + static_cast<int>(g * scale_ + 0.5f);
    if (line_height_ < 8) line_height_ = static_cast<int>(pixel_height + 4);
    cache_.clear();
    return true;
}

const Glyph& UiFont::glyph(uint32_t cp) {
    auto it = cache_.find(cp);
    if (it != cache_.end()) return it->second;

    Glyph g;
    auto* info = static_cast<stbtt_fontinfo*>(info_);
    if (!info) {
        cache_[cp] = g;
        return cache_[cp];
    }
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(info, static_cast<int>(cp), &adv, &lsb);
    g.advance = static_cast<int>(adv * scale_ + 0.5f);
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(info, static_cast<int>(cp), scale_, scale_, &x0, &y0, &x1, &y1);
    g.w = x1 - x0;
    g.h = y1 - y0;
    g.xoff = x0;
    g.yoff = y0;
    if (g.w > 0 && g.h > 0) {
        g.cover.resize(static_cast<size_t>(g.w * g.h));
        stbtt_MakeCodepointBitmap(info, g.cover.data(), g.w, g.h, g.w, scale_, scale_, static_cast<int>(cp));
    }
    auto [ins, _] = cache_.emplace(cp, std::move(g));
    return ins->second;
}

int UiFont::measure(const std::string& text) {
    int w = 0;
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = utf8_next(text, i);
        if (cp == 0) break;
        w += glyph(cp).advance;
    }
    return w;
}
