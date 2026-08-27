#pragma once

#include "config.hpp"
#include "font.hpp"
#include "nvidia_api.hpp"

#include <X11/Xlib.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class Overlay {
public:
    bool init(const AppConfig& cfg);
    void shutdown();
    bool should_quit() const { return running_quit_; }

    Display* display() const { return dpy_; }
    int x_fd() const;
    int api_fd() const { return api_.fd(); }
    void on_x11();
    void on_api();
    void blink();
    void restore_visible();
    void toggle_shortcut();

private:
    enum class Drag { Idle, Slider, Move };

    struct Msg {
        std::string role;
        std::string text;
        std::string reasoning;
        bool streaming = false;
    };

    void layout();
    void resize_window();
    void set_opacity_atom();
    void redraw();
    void apply_transparency();
    void present();
    void keep_on_top();
    void apply_input_shape();
    void claim_stack();
    void fill_rect(int x, int y, int w, int h, uint32_t argb);
    void fill_round(int x, int y, int w, int h, int r, uint32_t argb);
    void outlined_round(int x, int y, int w, int h, int r, uint32_t fill, int ring = 2);
    void blend(int x, int y, uint32_t src);
    void draw_text(UiFont& font, int x, int y, const std::string& text, uint32_t argb, int max_w = 0);
    std::vector<std::string> wrap(UiFont& font, const std::string& text, int max_w);
    int text_block_h(UiFont& font, const std::string& text, int max_w);
    void handle_key(XKeyEvent& ev);
    void handle_button(XButtonEvent& ev, bool press);
    void handle_motion(XMotionEvent& ev);
    void send_message();
    void new_chat();
    void append_token(const std::string& reasoning, const std::string& content);
    void grab_hotkeys();
    void ungrab_hotkeys();
    bool is_toggle_hotkey(const XKeyEvent& ev) const;
    float display_opacity() const;
    void set_type_capture(bool on, Time time = CurrentTime);
    void grab_keyboard_for_type(Time time);
    void apply_size_hints();
    bool in_type_button(int x, int y) const;
    bool in_new_button(int x, int y) const;
    bool in_hide_button(int x, int y) const;
    void hide_overlay();

    AppConfig cfg_;
    Display* dpy_ = nullptr;
    Window win_ = 0;
    Window root_ = 0;
    GC gc_ = 0;
    Visual* visual_ = nullptr;
    Colormap cmap_ = 0;
    XImage* image_ = nullptr;
    int screen_ = 0;
    int depth_ = 32;
    int sw_ = 0, sh_ = 0;
    int wx_ = 0, wy_ = 0, ww_ = 380, wh_ = 520;
    std::vector<uint32_t> pix_;

    UiFont font_ui_;
    UiFont font_sm_;
    UiFont font_title_;
    XIC ic_ = nullptr;
    XIM im_ = nullptr;

    bool chat_open_ = true;
    bool mapped_ = false;
    bool shortcut_hidden_ = false;
    bool type_mode_ = false;
    float opacity_ = 0.88f;
    float saved_opacity_ = 0.88f;
    bool saved_chat_open_ = true;
    std::string input_;
    size_t caret_ = 0;
    bool input_focus_ = true;
    bool caret_on_ = true;
    int scroll_ = 0;
    int content_h_ = 0;
    Drag drag_ = Drag::Idle;
    int drag_off_x_ = 0, drag_off_y_ = 0;
    std::vector<Msg> msgs_;
    NvidiaApi api_;
    bool running_quit_ = false;
    std::chrono::steady_clock::time_point ignore_clicks_until_{};
    std::chrono::steady_clock::time_point last_raise_{};
    std::chrono::steady_clock::time_point last_toggle_{};
    Time last_event_time_ = CurrentTime;
    Atom wm_delete_ = 0;
    Atom wm_take_focus_ = 0;
    bool type_grabbed_ = false;
};
