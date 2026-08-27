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
#include <sys/wait.h>
#include <unistd.h>

#include <X11/Xlib.h>

namespace {

volatile sig_atomic_t g_stop = 0;
volatile sig_atomic_t g_restore = 0;
volatile sig_atomic_t g_toggle = 0;

void on_signal(int) { g_stop = 1; }
void on_restore(int) { g_restore = 1; }
void on_toggle(int) { g_toggle = 1; }

void trap(int sig, void (*fn)(int)) {
    struct sigaction sa{};
    sa.sa_handler = fn;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(sig, &sa, nullptr);
}

void usage() {
    std::fprintf(stdout,
                 "overlay-chat — always-on-top transparent chatbot overlay\n\n"
                 "START:\n"
                 "  overlay-chat              (detaches; survives closing the terminal)\n"
                 "  overlay-chat --foreground (stay in this terminal)\n\n"
                 "END:\n"
                 "  overlay-chat --quit\n\n"
                 "Options:\n"
                 "  -h, --help        show this help\n"
                 "  --foreground      do not daemonize\n"
                 "  --quit            stop a running overlay-chat\n"
                 "  --status          print whether it is running\n"
                 "  --show            show the overlay\n"
                 "  --toggle          hide/show (Super+Shift+O)\n\n"
                 "Shortcut: Super+Shift+O  hide / open\n"
                 "          Ctrl+Alt+O     hide / open (backup)\n\n"
                 "API key file (put GROQ_API_KEY here):\n"
                 "  .env\n"
                 "  ~/.config/overlay-chat/env\n");
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

int cmd_toggle(const std::string& path) {
    pid_t pid = read_pid(path);
    if (pid_alive(pid)) {
        if (kill(pid, SIGUSR2) != 0) {
            std::fprintf(stderr, "failed to toggle overlay-chat (pid %d): %s\n", pid, std::strerror(errno));
            return 1;
        }
        return 0;
    }
    char self[4096];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n <= 0) {
        std::fprintf(stderr, "overlay-chat is not running\n");
        return 1;
    }
    self[n] = 0;
    execl(self, "overlay-chat", static_cast<char*>(nullptr));
    std::fprintf(stderr, "failed to start overlay-chat\n");
    return 1;
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

void close_stdio() {
    int z = open("/dev/null", O_RDWR);
    if (z < 0) return;
    dup2(z, STDIN_FILENO);
    dup2(z, STDOUT_FILENO);
    dup2(z, STDERR_FILENO);
    if (z > 2) close(z);
}

void daemonize(const std::string& pidf) {
    pid_t first = fork();
    if (first < 0) {
        std::fprintf(stderr, "fork failed: %s\n", std::strerror(errno));
        _exit(1);
    }
    if (first > 0) {
        int status = 0;
        waitpid(first, &status, 0);
        for (int i = 0; i < 50; ++i) {
            pid_t pid = read_pid(pidf);
            if (pid_alive(pid)) {
                std::printf("overlay-chat started in background (pid %d)\n", pid);
                std::printf("Keeps running after this terminal is closed. END: overlay-chat --quit\n");
                std::fflush(stdout);
                _exit(0);
            }
            usleep(50000);
        }
        std::fprintf(stderr, "overlay-chat failed to stay running. Try: overlay-chat --foreground\n");
        _exit(1);
    }

    if (setsid() < 0) _exit(1);
    pid_t second = fork();
    if (second < 0) _exit(1);
    if (second > 0) _exit(0);

    std::signal(SIGHUP, SIG_IGN);
    std::signal(SIGINT, SIG_IGN);
    close_stdio();
    if (chdir("/") != 0) { /* keep running even if chdir fails */ }
}

}  // namespace

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");

    AppConfig cfg = load_config();
    std::string pidf = pid_path(cfg);
    bool foreground = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            usage();
            return 0;
        }
        if (a == "--quit" || a == "--stop" || a == "--end") return cmd_quit(pidf);
        if (a == "--status") return cmd_status(pidf);
        if (a == "--show") return cmd_show(pidf);
        if (a == "--toggle" || a == "--hide") return cmd_toggle(pidf);
        if (a == "--foreground" || a == "-f") {
            foreground = true;
            continue;
        }
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

    if (!foreground) daemonize(pidf);

    if (!write_pid(pidf)) _exit(1);

    XInitThreads();
    trap(SIGTERM, on_signal);
    trap(SIGUSR1, on_restore);
    trap(SIGUSR2, on_toggle);
    trap(SIGPIPE, SIG_IGN);
    trap(SIGHUP, SIG_IGN);
    if (foreground) trap(SIGINT, on_signal);
    else trap(SIGINT, SIG_IGN);

    Overlay overlay;
    if (!overlay.init(cfg)) {
        unlink(pidf.c_str());
        _exit(1);
    }

    if (foreground) {
        std::printf("overlay-chat started (pid %d)\n", getpid());
        std::printf("END: overlay-chat --quit\n");
        std::fflush(stdout);
    }

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
        const int sel_err = errno;
        if (g_toggle) {
            g_toggle = 0;
            overlay.toggle_shortcut();
        }
        if (g_restore) {
            g_restore = 0;
            overlay.restore_visible();
        }
        if (rc < 0) {
            if (sel_err == EINTR) continue;
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
