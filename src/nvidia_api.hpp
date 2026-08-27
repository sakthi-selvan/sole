#pragma once

#include <functional>
#include <string>
#include <sys/types.h>
#include <vector>

struct ChatTurn {
    std::string role;
    std::string content;
};

class NvidiaApi {
public:
    using TokenFn = std::function<void(const std::string& reasoning, const std::string& content)>;
    using DoneFn = std::function<void(const std::string& error)>;

    ~NvidiaApi();

    bool streaming() const { return child_pid_ > 0; }
    int fd() const { return out_fd_; }

    void start(const std::string& api_base,
               const std::string& api_key,
               const std::string& model,
               const std::vector<ChatTurn>& history,
               TokenFn on_token,
               DoneFn on_done);
    void cancel();
    void pump();

private:
    void finish(const std::string& error);
    void handle_line(const std::string& line);

    pid_t child_pid_ = -1;
    int out_fd_ = -1;
    std::string body_path_;
    std::string buf_;
    std::string err_accum_;
    TokenFn on_token_;
    DoneFn on_done_;
};
