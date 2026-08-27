#include "nvidia_api.hpp"
#include "utf8.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

std::string build_body(const std::string& model, const std::vector<ChatTurn>& history) {
    std::string body;
    body += "{\"model\":\"" + json_escape(model) + "\",";
    body += "\"messages\":[";
    for (size_t i = 0; i < history.size(); ++i) {
        if (i) body += ",";
        body += "{\"role\":\"" + json_escape(history[i].role) + "\",";
        body += "\"content\":\"" + json_escape(history[i].content) + "\"}";
    }
    body += "],";
    body += "\"temperature\":1,\"max_tokens\":1024,\"stream\":true}";
    return body;
}

std::string extract_delta_field(const std::string& json, const char* key) {
    size_t d = json.find("\"delta\"");
    std::string scope = (d == std::string::npos) ? json : json.substr(d);
    return extract_json_string(scope, key);
}

}  // namespace

NvidiaApi::~NvidiaApi() { cancel(); }

void NvidiaApi::cancel() {
    if (out_fd_ >= 0) {
        close(out_fd_);
        out_fd_ = -1;
    }
    if (child_pid_ > 0) {
        kill(child_pid_, SIGTERM);
        int status = 0;
        waitpid(child_pid_, &status, 0);
        child_pid_ = -1;
    }
    if (!body_path_.empty()) {
        unlink(body_path_.c_str());
        body_path_.clear();
    }
    buf_.clear();
    on_token_ = {};
    on_done_ = {};
}

void NvidiaApi::finish(const std::string& error) {
    auto done = std::move(on_done_);
    cancel();
    if (done) done(error);
}

void NvidiaApi::handle_line(const std::string& line) {
    if (line.empty()) return;
    if (line.rfind("data:", 0) == 0) {
        std::string payload = line.substr(5);
        while (!payload.empty() && payload[0] == ' ') payload.erase(payload.begin());
        if (payload == "[DONE]") {
            finish({});
            return;
        }
        std::string reasoning = extract_delta_field(payload, "reasoning");
        if (reasoning.empty()) reasoning = extract_delta_field(payload, "reasoning_content");
        std::string content = extract_delta_field(payload, "content");
        if ((reasoning.size() || content.size()) && on_token_) on_token_(reasoning, content);
        std::string err = extract_json_string(payload, "message");
        if (err.empty()) err = extract_json_string(payload, "error");
        if (!err.empty() && payload.find("\"error\"") != std::string::npos) err_accum_ = err;
        return;
    }
    if (line[0] == '{') {
        std::string err = extract_json_string(line, "message");
        if (err.empty()) err = extract_json_string(line, "error");
        if (!err.empty()) err_accum_ = err;
        else err_accum_ += line;
    }
}

void NvidiaApi::pump() {
    if (out_fd_ < 0) return;
    char tmp[4096];
    while (true) {
        ssize_t n = read(out_fd_, tmp, sizeof(tmp));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            finish("read failed");
            return;
        }
        if (n == 0) {
            if (!buf_.empty()) handle_line(buf_);
            finish(err_accum_);
            return;
        }
        buf_.append(tmp, static_cast<size_t>(n));
        size_t pos;
        while ((pos = buf_.find('\n')) != std::string::npos) {
            std::string line = buf_.substr(0, pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            buf_.erase(0, pos + 1);
            handle_line(line);
            if (child_pid_ < 0) return;
        }
    }
}

void NvidiaApi::start(const std::string& api_base,
                      const std::string& api_key,
                      const std::string& model,
                      const std::vector<ChatTurn>& history,
                      TokenFn on_token,
                      DoneFn on_done) {
    cancel();
    on_token_ = std::move(on_token);
    on_done_ = std::move(on_done);
    err_accum_.clear();

    if (api_key.empty()) {
        finish("GROQ_API_KEY is not set. Put it in ~/.config/overlay-chat/env");
        return;
    }

    char tmpl[] = "/tmp/overlay-chat-XXXXXX";
    int tfd = mkstemp(tmpl);
    if (tfd < 0) {
        finish("could not create temp request file");
        return;
    }
    body_path_ = tmpl;
    std::string body = build_body(model, history);
    if (write(tfd, body.data(), body.size()) != static_cast<ssize_t>(body.size())) {
        close(tfd);
        finish("could not write request body");
        return;
    }
    close(tfd);

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        finish("pipe failed");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        finish("fork failed");
        return;
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        std::string url = api_base;
        if (!url.empty() && url.back() == '/') url.pop_back();
        url += "/chat/completions";
        std::string auth = "Authorization: Bearer " + api_key;
        std::string data = std::string("@") + body_path_;
        execlp("curl", "curl", "-sS", "-N", "--max-time", "600",
               "-X", "POST", url.c_str(),
               "-H", auth.c_str(),
               "-H", "Content-Type: application/json",
               "-H", "Accept: text/event-stream",
               "--data-binary", data.c_str(),
               static_cast<char*>(nullptr));
        _exit(127);
    }

    close(pipefd[1]);
    child_pid_ = pid;
    out_fd_ = pipefd[0];
    int flags = fcntl(out_fd_, F_GETFL, 0);
    fcntl(out_fd_, F_SETFL, flags | O_NONBLOCK);
}
