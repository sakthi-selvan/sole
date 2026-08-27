#include "overlay.hpp"
#include "utf8.hpp"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <unistd.h>

namespace {

constexpr int kPad = 10;
constexpr int kCtrlH = 44;
constexpr int kTitleH = 34;
constexpr int kInputH = 54;
constexpr int kHideW = 56;
constexpr int kChatW = 380;
constexpr int kChatH = 508;
constexpr int kCollapsedW = 272;
constexpr int kMargin = 18;
constexpr int kSliderH = 16;
constexpr float kMinOpacity = 0.0f;

uint32_t argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

bool Overlay::init(const AppConfig& cfg) {
    cfg_ = cfg;
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) return false;
    screen_ = DefaultScreen(dpy_);
    root_ = RootWindow(dpy_, screen_);
    Window root = root_;
    sw_ = DisplayWidth(dpy_, screen_);
    sh_ = DisplayHeight(dpy_, screen_);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(dpy_, screen_, 32, TrueColor, &vinfo)) {
        if (!XMatchVisualInfo(dpy_, screen_, DefaultDepth(dpy_, screen_), TrueColor, &vinfo)) {
            return false;
        }
    }
    visual_ = vinfo.visual;
    depth_ = vinfo.depth;
    cmap_ = XCreateColormap(dpy_, root, visual_, AllocNone);

    layout();
    wx_ = sw_ - ww_ - kMargin;
    wy_ = sh_ - wh_ - kMargin - 28;
    if (wx_ < kMargin) wx_ = kMargin;
    if (wy_ < kMargin) wy_ = kMargin;

    XSetWindowAttributes swa{};
    swa.colormap = cmap_;
    swa.border_pixel = 0;
    swa.background_pixel = 0;
    swa.override_redirect = True;
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask
        | PointerMotionMask | KeyPressMask | StructureNotifyMask | FocusChangeMask
        | VisibilityChangeMask;

    win_ = XCreateWindow(dpy_, root, wx_, wy_, static_cast<unsigned>(ww_), static_cast<unsigned>(wh_), 0,
                         depth_, InputOutput, visual_,
                         CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect | CWEventMask, &swa);

    XStoreName(dpy_, win_, "overlay-chat");
    Atom net_state = XInternAtom(dpy_, "_NET_WM_STATE", False);
    Atom atoms[4];
    atoms[0] = XInternAtom(dpy_, "_NET_WM_STATE_ABOVE", False);
    atoms[1] = XInternAtom(dpy_, "_NET_WM_STATE_STICKY", False);
    atoms[2] = XInternAtom(dpy_, "_NET_WM_STATE_SKIP_TASKBAR", False);
    atoms[3] = XInternAtom(dpy_, "_NET_WM_STATE_SKIP_PAGER", False);
    XChangeProperty(dpy_, win_, net_state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(atoms), 4);
    Atom wtype = XInternAtom(dpy_, "_NET_WM_WINDOW_TYPE", False);
    Atom utility = XInternAtom(dpy_, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    XChangeProperty(dpy_, win_, wtype, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&utility), 1);

    XSelectInput(dpy_, root, SubstructureNotifyMask);

    gc_ = XCreateGC(dpy_, win_, 0, nullptr);
    set_opacity_atom();

    std::string font_path = default_font_path();
    if (font_path.empty() || !font_ui_.load(font_path, 14.f) || !font_sm_.load(font_path, 11.5f)
        || !font_title_.load(font_path, 15.f)) {
        return false;
    }

    im_ = XOpenIM(dpy_, nullptr, nullptr, nullptr);
    if (im_) {
        ic_ = XCreateIC(im_, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win_,
                        XNFocusWindow, win_, nullptr);
    }

    msgs_.push_back({"assistant",
                     "Overlay chat is ready. Type a message and press Enter.\n\n"
                     "Opacity slider stays on screen. Hide collapses the chat; the controls stay in the "
                     "bottom-right corner.\n\n"
                     "Stop with: overlay-chat --quit",
                     "", false});

    pix_.assign(static_cast<size_t>(ww_ * wh_), 0);
    image_ = XCreateImage(dpy_, visual_, static_cast<unsigned>(depth_), ZPixmap, 0,
                          reinterpret_cast<char*>(pix_.data()), static_cast<unsigned>(ww_),
                          static_cast<unsigned>(wh_), 32, ww_ * 4);
    if (image_) {
        image_->byte_order = LSBFirst;
        image_->bitmap_bit_order = LSBFirst;
    }

    if (ic_) XSetICFocus(ic_);
    XMapRaised(dpy_, win_);
    mapped_ = true;
    XFlush(dpy_);
    ignore_clicks_until_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(700);
    redraw();
    return true;
}

void Overlay::shutdown() {
    api_.cancel();
    if (ic_) XDestroyIC(ic_);
    ic_ = nullptr;
    if (im_) XCloseIM(im_);
    im_ = nullptr;
    if (image_) {
        image_->data = nullptr;
        XDestroyImage(image_);
        image_ = nullptr;
    }
    if (gc_) XFreeGC(dpy_, gc_);
    gc_ = 0;
    if (win_) XDestroyWindow(dpy_, win_);
    win_ = 0;
    if (cmap_) XFreeColormap(dpy_, cmap_);
    cmap_ = 0;
    if (dpy_) XCloseDisplay(dpy_);
    dpy_ = nullptr;
}

int Overlay::x_fd() const { return dpy_ ? ConnectionNumber(dpy_) : -1; }

void Overlay::layout() {
    if (chat_open_) {
        ww_ = kChatW;
        wh_ = kChatH;
    } else {
        ww_ = kCollapsedW;
        wh_ = kCtrlH;
    }
}

void Overlay::resize_window() {
    int old_w = ww_, old_h = wh_;
    layout();
    wx_ += old_w - ww_;
    wy_ += old_h - wh_;
    wx_ = clampi(wx_, kMargin, std::max(kMargin, sw_ - ww_ - kMargin));
    wy_ = clampi(wy_, kMargin, std::max(kMargin, sh_ - wh_ - kMargin));
    pix_.assign(static_cast<size_t>(ww_ * wh_), 0);
    if (image_) {
        image_->data = nullptr;
        XDestroyImage(image_);
    }
    image_ = XCreateImage(dpy_, visual_, static_cast<unsigned>(depth_), ZPixmap, 0,
                          reinterpret_cast<char*>(pix_.data()), static_cast<unsigned>(ww_),
                          static_cast<unsigned>(wh_), 32, ww_ * 4);
    if (image_) {
        image_->byte_order = LSBFirst;
        image_->bitmap_bit_order = LSBFirst;
    }
    XMoveResizeWindow(dpy_, win_, wx_, wy_, static_cast<unsigned>(ww_), static_cast<unsigned>(wh_));
    set_opacity_atom();
    redraw();
}

void Overlay::set_opacity_atom() {
    const float o = std::clamp(opacity_, 0.f, 1.f);
    unsigned long op = (o <= 0.0005f) ? 0ul : static_cast<unsigned long>(o * 0xffffffffu);
    Atom a = XInternAtom(dpy_, "_NET_WM_WINDOW_OPACITY", False);
    XChangeProperty(dpy_, win_, a, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&op), 1);
    const bool vanish = o <= 0.0005f && drag_ != Drag::Slider;
    if (vanish) {
        XUnmapWindow(dpy_, win_);
        mapped_ = false;
    } else if (!mapped_) {
        XMapRaised(dpy_, win_);
        mapped_ = true;
    }
    XFlush(dpy_);
}

void Overlay::restore_visible() {
    opacity_ = 0.88f;
    drag_ = Drag::Idle;
    set_opacity_atom();
    redraw();
}

void Overlay::blend(int x, int y, uint32_t src) {
    if (x < 0 || y < 0 || x >= ww_ || y >= wh_) return;
    const uint32_t sa = src >> 24;
    if (sa == 0) return;
    uint32_t& dst = pix_[static_cast<size_t>(y * ww_ + x)];
    if (sa == 255) {
        dst = src;
        return;
    }
    const uint32_t da = dst >> 24;
    const uint32_t inv = 255 - sa;
    const uint32_t oa = sa + da * inv / 255;
    auto ch = [&](int sshift, int dshift) {
        uint32_t s = (src >> sshift) & 255;
        uint32_t d = (dst >> dshift) & 255;
        uint32_t o = (s * sa + d * da * inv / 255) / (oa ? oa : 1);
        return o;
    };
    dst = (oa << 24) | (ch(16, 16) << 16) | (ch(8, 8) << 8) | ch(0, 0);
}

void Overlay::fill_rect(int x, int y, int w, int h, uint32_t color) {
    int x1 = clampi(x, 0, ww_);
    int y1 = clampi(y, 0, wh_);
    int x2 = clampi(x + w, 0, ww_);
    int y2 = clampi(y + h, 0, wh_);
    for (int py = y1; py < y2; ++py)
        for (int px = x1; px < x2; ++px) blend(px, py, color);
}

void Overlay::fill_round(int x, int y, int w, int h, int r, uint32_t color) {
    if (r <= 0) {
        fill_rect(x, y, w, h, color);
        return;
    }
    int x1 = x, y1 = y, x2 = x + w, y2 = y + h;
    for (int py = y1; py < y2; ++py) {
        for (int px = x1; px < x2; ++px) {
            int dx = 0, dy = 0;
            if (px < x1 + r && py < y1 + r) {
                dx = x1 + r - px;
                dy = y1 + r - py;
            } else if (px >= x2 - r && py < y1 + r) {
                dx = px - (x2 - r - 1);
                dy = y1 + r - py;
            } else if (px < x1 + r && py >= y2 - r) {
                dx = x1 + r - px;
                dy = py - (y2 - r - 1);
            } else if (px >= x2 - r && py >= y2 - r) {
                dx = px - (x2 - r - 1);
                dy = py - (y2 - r - 1);
            }
            if (dx || dy) {
                if (dx * dx + dy * dy > r * r) continue;
            }
            blend(px, py, color);
        }
    }
}

void Overlay::outlined_round(int x, int y, int w, int h, int r, uint32_t fill, int ring) {
    if (w <= 0 || h <= 0) return;
    const uint32_t outline = argb(140, 248, 248, 250);
    const int t = std::max(1, ring);
    fill_round(x, y, w, h, r, outline);
    fill_round(x + t, y + t, w - 2 * t, h - 2 * t, std::max(0, r - t), fill);
}

void Overlay::keep_on_top() {
    if (!dpy_ || !win_ || !mapped_ || opacity_ <= 0.0005f) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - last_raise_ < std::chrono::milliseconds(50)) {
        XRaiseWindow(dpy_, win_);
        return;
    }
    last_raise_ = now;
    XRaiseWindow(dpy_, win_);
    XClientMessageEvent cm{};
    cm.type = ClientMessage;
    cm.window = win_;
    cm.message_type = XInternAtom(dpy_, "_NET_WM_STATE", False);
    cm.format = 32;
    cm.data.l[0] = 1;
    cm.data.l[1] = static_cast<long>(XInternAtom(dpy_, "_NET_WM_STATE_ABOVE", False));
    cm.data.l[2] = 0;
    cm.data.l[3] = 1;
    XSendEvent(dpy_, root_ ? root_ : DefaultRootWindow(dpy_), False,
               SubstructureRedirectMask | SubstructureNotifyMask, reinterpret_cast<XEvent*>(&cm));
    XFlush(dpy_);
}

void Overlay::draw_text(UiFont& font, int x, int y, const std::string& text, uint32_t color, int max_w) {
    int cx = x;
    size_t i = 0;
    const int baseline = y + font.ascent();
    while (i < text.size()) {
        uint32_t cp = utf8_next(text, i);
        if (cp == 0) break;
        const Glyph& g = font.glyph(cp);
        if (max_w > 0 && cx + g.advance > x + max_w) break;
        for (int gy = 0; gy < g.h; ++gy) {
            for (int gx = 0; gx < g.w; ++gx) {
                unsigned char c = g.cover[static_cast<size_t>(gy * g.w + gx)];
                if (!c) continue;
                uint32_t a = ((color >> 24) * c) / 255;
                uint32_t src = (a << 24) | (color & 0x00FFFFFF);
                blend(cx + g.xoff + gx, baseline + g.yoff + gy, src);
            }
        }
        cx += g.advance;
    }
}

std::vector<std::string> Overlay::wrap(UiFont& font, const std::string& text, int max_w) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        std::string para = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (para.empty()) {
            lines.push_back({});
        } else {
            size_t i = 0;
            std::string line;
            int w = 0;
            size_t last_space = std::string::npos;
            int last_space_w = 0;
            while (i < para.size()) {
                size_t prev = i;
                uint32_t cp = utf8_next(para, i);
                const Glyph& g = font.glyph(cp);
                if (w + g.advance > max_w && !line.empty()) {
                    if (last_space != std::string::npos) {
                        lines.push_back(line.substr(0, last_space));
                        line = line.substr(last_space + 1);
                        w -= last_space_w + font.glyph(' ').advance;
                        last_space = std::string::npos;
                    } else {
                        lines.push_back(line);
                        line.clear();
                        w = 0;
                    }
                }
                if (cp == ' ') {
                    last_space = line.size();
                    last_space_w = w;
                }
                line.append(para, prev, i - prev);
                w += g.advance;
            }
            lines.push_back(line);
        }
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return lines;
}

int Overlay::text_block_h(UiFont& font, const std::string& text, int max_w) {
    auto lines = wrap(font, text, max_w);
    return std::max(1, static_cast<int>(lines.size())) * font.line_height();
}

void Overlay::redraw() {
    if (!dpy_ || pix_.empty()) return;
    std::fill(pix_.begin(), pix_.end(), 0);

    const uint32_t fill = argb(248, 255, 255, 255);
    const uint32_t ink = argb(230, 42, 42, 46);
    const uint32_t muted = argb(170, 150, 150, 156);

    outlined_round(0, 0, ww_, wh_, 16, fill, 3);

    auto slider_and_hide = [&](int y) {
        const int hide_x = ww_ - kPad - kHideW;
        const int hide_y = y + 6;
        const int hide_h = kCtrlH - 12;
        outlined_round(hide_x, hide_y, kHideW, hide_h, 8, fill, 2);
        const char* label = chat_open_ ? "Hide" : "Show";
        int tw = font_sm_.measure(label);
        draw_text(font_sm_, hide_x + (kHideW - tw) / 2, hide_y + (hide_h - font_sm_.line_height()) / 2 + 1, label,
                  ink);

        int sx = kPad + 8;
        int sw = hide_x - sx - 12;
        int sy = y + kCtrlH / 2 - 2;
        outlined_round(sx, sy - 3, sw, 10, 5, fill, 2);
        const float span = 1.f - kMinOpacity;
        int knob_x = sx + static_cast<int>((std::clamp(opacity_, kMinOpacity, 1.f) - kMinOpacity) / span * sw);
        fill_rect(sx + 3, sy, std::max(4, knob_x - sx - 3), 4, ink);
        outlined_round(knob_x - 8, sy - 8, 16, 18, 8, fill, 2);
    };

    if (chat_open_) {
        outlined_round(8, 8, ww_ - 16, kTitleH, 10, fill, 2);
        draw_text(font_title_, kPad + 6, 14, "Overlay Chat", ink);
        std::string op = std::to_string(static_cast<int>(opacity_ * 100)) + "%";
        draw_text(font_sm_, ww_ - kPad - 6 - font_sm_.measure(op), 16, op, muted);

        const int msg_y = kTitleH;
        const int msg_h = wh_ - kTitleH - kInputH - kCtrlH;
        const int msg_x = kPad;
        const int msg_w = ww_ - kPad * 2;

        int y = msg_y + kPad - scroll_;
        for (const auto& m : msgs_) {
            bool user = m.role == "user";
            UiFont& body = font_ui_;
            int bubble_w = msg_w - 18;
            int inner_w = bubble_w - 16;
            int bh = 12;
            if (!m.reasoning.empty()) bh += text_block_h(font_sm_, m.reasoning, inner_w) + 6;
            if (!m.text.empty()) bh += text_block_h(body, m.text, inner_w);
            else bh += body.line_height();
            int bx = user ? msg_x + 18 : msg_x;
            if (y + bh > msg_y && y < msg_y + msg_h) {
                outlined_round(bx, y, bubble_w, bh, 10, fill, 2);
                int ty = y + 6;
                if (!m.reasoning.empty()) {
                    auto lines = wrap(font_sm_, m.reasoning, inner_w);
                    for (const auto& ln : lines) {
                        draw_text(font_sm_, bx + 8, ty, ln, muted, inner_w);
                        ty += font_sm_.line_height();
                    }
                    ty += 4;
                }
                auto lines = wrap(body, m.text.empty() && m.streaming ? "..." : m.text, inner_w);
                for (const auto& ln : lines) {
                    draw_text(body, bx + 8, ty, ln, ink, inner_w);
                    ty += body.line_height();
                }
            }
            y += bh + 8;
        }
        content_h_ = (y + scroll_) - msg_y + kPad;
        int max_scroll = std::max(0, content_h_ - msg_h);
        scroll_ = clampi(scroll_, 0, max_scroll);

        int iy = wh_ - kCtrlH - kInputH;
        outlined_round(kPad, iy + 6, ww_ - kPad * 2 - 64, kInputH - 12, 10, fill, 2);
        outlined_round(ww_ - kPad - 56, iy + 10, 56, kInputH - 20, 8, fill, 2);
        draw_text(font_sm_, ww_ - kPad - 56 + 12, iy + 18, "Send", ink);

        std::string shown = input_;
        int field_w = ww_ - kPad * 2 - 64 - 16;
        while (font_ui_.measure(shown) > field_w && !shown.empty()) {
            size_t i = 0;
            utf8_next(shown, i);
            shown.erase(0, i);
        }
        draw_text(font_ui_, kPad + 16, iy + 16, shown.empty() ? "Message..." : shown, shown.empty() ? muted : ink,
                  field_w);
        if (input_focus_ && caret_on_) {
            int cx = kPad + 16 + font_ui_.measure(shown);
            fill_rect(cx, iy + 14, 2, 20, ink);
        }

        slider_and_hide(wh_ - kCtrlH);
    } else {
        slider_and_hide(0);
    }

    present();
}

void Overlay::present() {
    if (!image_ || !dpy_) return;
    image_->data = reinterpret_cast<char*>(pix_.data());
    XPutImage(dpy_, win_, gc_, image_, 0, 0, 0, 0, static_cast<unsigned>(ww_), static_cast<unsigned>(wh_));
    keep_on_top();
}

void Overlay::send_message() {
    while (!input_.empty() && (input_.back() == ' ' || input_.back() == '\n')) input_.pop_back();
    if (input_.empty()) return;
    if (api_.streaming()) return;

    msgs_.push_back({"user", input_, "", false});
    msgs_.push_back({"assistant", "", "", true});
    std::string prompt = input_;
    input_.clear();
    caret_ = 0;
    scroll_ = 100000;

    std::vector<ChatTurn> hist;
    for (const auto& m : msgs_) {
        if (m.streaming) continue;
        if (m.text.empty()) continue;
        hist.push_back({m.role, m.text});
    }
    if (hist.size() > 16) hist.erase(hist.begin(), hist.end() - 16);

    api_.start(cfg_.api_base, cfg_.api_key, cfg_.model, hist,
               [this](const std::string& r, const std::string& c) { append_token(r, c); },
               [this](const std::string& err) {
                   if (!msgs_.empty() && msgs_.back().streaming) {
                       msgs_.back().streaming = false;
                       if (!err.empty()) {
                           if (!msgs_.back().text.empty()) msgs_.back().text += "\n";
                           msgs_.back().text += err;
                       } else if (msgs_.back().text.empty() && msgs_.back().reasoning.empty()) {
                           msgs_.back().text = "No response. Check NVIDIA_API_KEY and network.";
                       }
                   }
                   redraw();
               });
    redraw();
}

void Overlay::append_token(const std::string& reasoning, const std::string& content) {
    if (msgs_.empty()) return;
    auto& m = msgs_.back();
    m.reasoning += reasoning;
    m.text += content;
    scroll_ = 100000;
    redraw();
}

void Overlay::handle_key(XKeyEvent& ev) {
    if (!chat_open_) return;
    KeySym ks = 0;
    char buf[64]{};
    int n = 0;
    if (ic_) {
        Status st = 0;
        n = Xutf8LookupString(ic_, &ev, buf, sizeof(buf) - 1, &ks, &st);
        if (n < 0) n = 0;
    } else {
        n = XLookupString(&ev, buf, sizeof(buf) - 1, &ks, nullptr);
    }
    buf[n] = 0;

    if ((ev.state & ControlMask) && (ks == XK_q || ks == XK_c)) {
        if (ks == XK_q) {
            api_.cancel();
            if (dpy_) XUnmapWindow(dpy_, win_);
            running_quit_ = true;
        }
        return;
    }
    if (ks == XK_Return) {
        send_message();
        return;
    }
    if (ks == XK_BackSpace) {
        if (caret_ == 0) return;
        size_t end = caret_;
        utf8_prev(input_, caret_);
        input_.erase(caret_, end - caret_);
        redraw();
        return;
    }
    if (ks == XK_Escape) {
        chat_open_ = false;
        resize_window();
        return;
    }
    if (n > 0 && (unsigned char)buf[0] >= 32) {
        input_.insert(caret_, buf, static_cast<size_t>(n));
        caret_ += static_cast<size_t>(n);
        redraw();
    }
}

void Overlay::handle_button(XButtonEvent& ev, bool press) {
    if (press && std::chrono::steady_clock::now() < ignore_clicks_until_) return;
    const int hide_x = ww_ - kPad - kHideW;
    const int hide_y = chat_open_ ? (wh_ - kCtrlH + 6) : 6;
    const int hide_h = kCtrlH - 12;
    auto in_hide = [&](int x, int y) {
        return x >= hide_x && x < hide_x + kHideW && y >= hide_y && y < hide_y + hide_h;
    };

    if (ev.button == Button4 && press && chat_open_) {
        scroll_ = std::max(0, scroll_ - 36);
        redraw();
        return;
    }
    if (ev.button == Button5 && press && chat_open_) {
        scroll_ += 36;
        redraw();
        return;
    }
    if (ev.button != Button1) return;

    if (!press) {
        drag_ = Drag::Idle;
        set_opacity_atom();
        return;
    }

    XSetInputFocus(dpy_, win_, RevertToParent, CurrentTime);

    if (in_hide(ev.x, ev.y)) {
        chat_open_ = !chat_open_;
        resize_window();
        return;
    }

    int ctrl_y = chat_open_ ? (wh_ - kCtrlH) : 0;
    int sx = kPad + 8;
    int sw = hide_x - sx - 12;
    int sy = ctrl_y + kCtrlH / 2 - 10;
    if (ev.x >= sx - 4 && ev.x <= sx + sw + 8 && ev.y >= sy && ev.y <= sy + 20) {
        drag_ = Drag::Slider;
        float t = std::clamp((ev.x - sx) / float(std::max(1, sw)), 0.f, 1.f);
        opacity_ = kMinOpacity + t * (1.f - kMinOpacity);
        set_opacity_atom();
        redraw();
        return;
    }

    if (chat_open_) {
        int iy = wh_ - kCtrlH - kInputH;
        if (ev.x >= ww_ - kPad - 56 && ev.x <= ww_ - kPad && ev.y >= iy + 10 && ev.y <= iy + kInputH - 10) {
            send_message();
            return;
        }
        if (ev.y < kTitleH) {
            drag_ = Drag::Move;
            drag_off_x_ = ev.x;
            drag_off_y_ = ev.y;
            return;
        }
        input_focus_ = ev.y >= iy;
    } else {
        drag_ = Drag::Move;
        drag_off_x_ = ev.x;
        drag_off_y_ = ev.y;
    }
}

void Overlay::handle_motion(XMotionEvent& ev) {
    if (drag_ == Drag::Slider) {
        const int hide_x = ww_ - kPad - kHideW;
        int sx = kPad + 8;
        int sw = hide_x - sx - 12;
        float t = std::clamp((ev.x - sx) / float(std::max(1, sw)), 0.f, 1.f);
        opacity_ = kMinOpacity + t * (1.f - kMinOpacity);
        set_opacity_atom();
        redraw();
    } else if (drag_ == Drag::Move) {
        wx_ = clampi(wx_ + ev.x - drag_off_x_, 0, sw_ - ww_);
        wy_ = clampi(wy_ + ev.y - drag_off_y_, 0, sh_ - wh_);
        XMoveWindow(dpy_, win_, wx_, wy_);
    }
}

void Overlay::on_x11() {
    while (dpy_ && XPending(dpy_)) {
        XEvent ev;
        XNextEvent(dpy_, &ev);
        if (ic_ && XFilterEvent(&ev, win_)) continue;
        switch (ev.type) {
            case Expose:
                if (ev.xexpose.count == 0) present();
                break;
            case VisibilityNotify:
                if (ev.xvisibility.state != VisibilityUnobscured) keep_on_top();
                break;
            case MapNotify:
                if (ev.xmap.window != win_) keep_on_top();
                break;
            case ConfigureNotify:
                if (ev.xconfigure.window != win_) keep_on_top();
                break;
            case CirculateNotify:
                keep_on_top();
                break;
            case KeyPress:
                handle_key(ev.xkey);
                break;
            case ButtonPress:
                handle_button(ev.xbutton, true);
                break;
            case ButtonRelease:
                handle_button(ev.xbutton, false);
                break;
            case MotionNotify:
                handle_motion(ev.xmotion);
                break;
            default:
                break;
        }
    }
}

void Overlay::on_api() { api_.pump(); }

void Overlay::blink() {
    if (!mapped_ || opacity_ <= 0.0005f) return;
    caret_on_ = !caret_on_;
    if (chat_open_ && input_focus_) redraw();
    else keep_on_top();
}
