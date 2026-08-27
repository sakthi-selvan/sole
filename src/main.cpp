#include "config.hpp"
#include "overlay.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <locale.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/Xlib.h>

namespace {

volatile sig_atomic_t g_stop = 0;
volatile sig_atomic_t g_restore = 0;

void on_signal(int) { g_stop = 1; }
void on_restore(int) { g_restore = 1; }

void usage() {
    std::fprintf(stdout,
                 "overlay-chat — always-on-top transparent chatbot overlay\n\n"
                 "START:\n"
                 "  overlay-chat\n"
                 "  NVIDIA_API_KEY=... overlay-chat\n\n"
                 "END:\n"
                 "  overlay-chat --quit\n"
                 "  overlay-chat --stop\n\n"
                 "Options:\n"
                 "  -h, --help     show this help\n"
                 "  --quit         stop a running overlay-chat\n"
                 "  --status       print whether it is running\n"
                 "  --show         show again if opacity was set to 0\n\n"
                 "API key (first match wins at runtime via env override):\n"
                 "  environment  NVIDIA_API_KEY\n"
                 "  file         ~/.config/overlay-chat/env\n");
}

pid_t read_pid(const std::string& path) {
    std::ifstream in(path);
    if (!in) return -1;
    pid_t pid = -1;
    in >> pid;
    return pid;
}

bool pid_alive(pid_t pid) { return pid > 0 && kill(pid, 0) == 0; }

int cmd_quit(const std::string& path) {
    pid_t pid = read_pid(path);
    if (!pid_alive(pid)) {
        std::fprintf(stderr, "overlay-chat is not running\n");
        unlink(path.c_str());
        return 1;
    }
    if (kill(pid, SIGTERM) != 0) {
        std::fprintf(stderr, "failed to stop overlay-chat (pid %d): %s\n", pid, std::strerror(errno));
        return 1;
    }
    for (int i = 0; i < 50; ++i) {
        if (!pid_alive(pid)) break;
        usleep(100000);
    }
    unlink(path.c_str());
    std::printf("overlay-chat stopped (pid %d)\n", pid);
    return 0;
}

int cmd_show(const std::string& path) {
    pid_t pid = read_pid(path);
    if (!pid_alive(pid)) {
        std::fprintf(stderr, "overlay-chat is not running\n");
        return 1;
    }
    if (kill(pid, SIGUSR1) != 0) {
        std::fprintf(stderr, "failed to show overlay-chat (pid %d): %s\n", pid, std::strerror(errno));
        return 1;
    }
    std::printf("overlay-chat shown (pid %d)\n", pid);
    return 0;
}

int cmd_status(const std::string& path) {
    pid_t pid = read_pid(path);
    if (pid_alive(pid)) {
        std::printf("running (pid %d)\n", pid);
        return 0;
    }
    std::printf("stopped\n");
    return 1;
}

bool write_pid(const std::string& path) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) return false;
    std::string s = std::to_string(getpid()) + "\n";
    ssize_t n = write(fd, s.data(), s.size());
    close(fd);
    return n == static_cast<ssize_t>(s.size());
}

}  // namespace

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    XInitThreads();

    AppConfig cfg = load_config();
    std::string pidf = pid_path(cfg);

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            usage();
            return 0;
        }
        if (a == "--quit" || a == "--stop" || a == "--end") return cmd_quit(pidf);
        if (a == "--status") return cmd_status(pidf);
        if (a == "--show") return cmd_show(pidf);
        std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
        usage();
        return 2;
    }

    pid_t existing = read_pid(pidf);
    if (pid_alive(existing)) {
        kill(existing, SIGUSR1);
        std::printf("overlay-chat already running (pid %d) — restored visibility\n", existing);
        return 0;
    }
    unlink(pidf.c_str());
    if (!write_pid(pidf)) {
        std::fprintf(stderr, "could not write pid file %s\n", pidf.c_str());
        return 1;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGHUP, on_signal);
    std::signal(SIGUSR1, on_restore);
    std::signal(SIGPIPE, SIG_IGN);

    Overlay overlay;
    if (!overlay.init(cfg)) {
        std::fprintf(stderr, "failed to open display / load font. Is DISPLAY set?\n");
        unlink(pidf.c_str());
        return 1;
    }

    std::printf("overlay-chat started (pid %d)\n", getpid());
    std::printf("START already done. END: overlay-chat --quit\n");
    std::fflush(stdout);

    while (!g_stop && !overlay.should_quit()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int xfd = overlay.x_fd();
        int afd = overlay.api_fd();
        int maxfd = -1;
        if (xfd >= 0) {
            FD_SET(xfd, &rfds);
            maxfd = xfd;
        }
        if (afd >= 0) {
            FD_SET(afd, &rfds);
            if (afd > maxfd) maxfd = afd;
        }
        timeval tv{};
        tv.tv_usec = 400000;
        int rc = select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (g_restore) {
            g_restore = 0;
            overlay.restore_visible();
        }
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (rc == 0) {
            overlay.blink();
            continue;
        }
        if (afd >= 0 && FD_ISSET(afd, &rfds)) overlay.on_api();
        if (xfd >= 0 && FD_ISSET(xfd, &rfds)) overlay.on_x11();
        if (XPending(overlay.display())) overlay.on_x11();
    }

    overlay.shutdown();
    unlink(pidf.c_str());
    return 0;
}
