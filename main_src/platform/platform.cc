// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "platform/platform.h"

#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace prosophor {
namespace platform {

bool IsUtf8(const std::string& input) {
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c < 0x80) continue;
        if (c >= 0xC2 && c <= 0xDF) {
            if (++i >= input.size() || (input[i] & 0xC0) != 0x80) return false;
        } else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= input.size() || (input[++i] & 0xC0) != 0x80 || (input[++i] & 0xC0) != 0x80) return false;
        } else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 >= input.size() || (input[++i] & 0xC0) != 0x80 || (input[++i] & 0xC0) != 0x80 || (input[++i] & 0xC0) != 0x80) return false;
        } else return false;
    }
    return true;
}

std::string NativeToUtf8(const std::string& input) {
    if (IsUtf8(input)) {
        return input;
    }

#ifdef _WIN32
    int wide_len = MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, nullptr, 0);
    if (wide_len > 0) {
        std::wstring wide(wide_len, 0);
        MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, &wide[0], wide_len);

        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8_len > 0) {
            std::string utf8(utf8_len, 0);
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8_len, nullptr, nullptr);
            return utf8;
        }
    }
#endif

    return input;
}

std::string ReadLine() {
#ifdef _WIN32
    HANDLE h_input = GetStdHandle(STD_INPUT_HANDLE);
    std::string line;

    if (h_input != INVALID_HANDLE_VALUE) {
        wchar_t wbuf[4096];
        DWORD chars_read = 0;
        if (ReadConsoleW(h_input, wbuf, sizeof(wbuf) / sizeof(wchar_t) - 1, &chars_read, nullptr)) {
            while (chars_read > 0 && (wbuf[chars_read - 1] == L'\r' || wbuf[chars_read - 1] == L'\n')) {
                chars_read--;
            }
            if (chars_read > 0) {
                std::wstring wline(wbuf, chars_read);
                int size = WideCharToMultiByte(CP_UTF8, 0, wline.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    line.resize(size - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wline.c_str(), -1, &line[0], size, nullptr, nullptr);
                }
            }
        } else {
            std::getline(std::cin, line);
            line = NativeToUtf8(line);
        }
    } else {
        std::getline(std::cin, line);
        line = NativeToUtf8(line);
    }
    return line;
#else
    std::string line;
    std::getline(std::cin, line);
    return line;
#endif
}

std::string HomeDir() {
    const char* home = std::getenv("HOME");
#ifdef _WIN32
    if (!home) {
        home = std::getenv("USERPROFILE");
    }
#endif
    return home ? std::string(home) : "";
}

std::tm LocalTime(std::time_t t) {
    std::tm tm_result{};
#ifdef _WIN32
    localtime_s(&tm_result, &t);
#else
    localtime_r(&t, &tm_result);
#endif
    return tm_result;
}

std::wstring Utf8ToWide(const std::string& utf8_str) {
#ifdef _WIN32
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
    if (wide_len <= 1) return L"";
    std::wstring wide(wide_len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &wide[0], wide_len);
    return wide;
#else
    (void)utf8_str;
    return L"";
#endif
}

void SetConsoleUtf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Enable ANSI/VT escape sequence processing (Windows 10+)
    HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_out != INVALID_HANDLE_VALUE) {
        DWORD out_mode = 0;
        if (GetConsoleMode(h_out, &out_mode)) {
            out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(h_out, out_mode);
        }
    }
#endif
}

int GetPid() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

std::string ShellEscape(const std::string& arg) {
#ifdef _WIN32
    return "\"" + arg + "\"";
#else
    std::string result = "\"";
    for (char c : arg) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') result += '\\';
        result += c;
    }
    result += "\"";
    return result;
#endif
}

std::string ReadConsoleLine() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    char buf[256] = {};
    DWORD charsRead = 0;
    if (ReadConsoleA(hConsole, buf, sizeof(buf) - 1, &charsRead, nullptr)) {
        while (charsRead > 0 && (buf[charsRead - 1] == '\r' || buf[charsRead - 1] == '\n')) {
            charsRead--;
        }
        return std::string(buf, charsRead);
    }
    return {};
#else
    std::string line;
    std::getline(std::cin, line);
    return line;
#endif
}

std::string GetSelfExePath() {
    std::string self_path;
#ifdef __linux__
    char self_buf[4096];
    ssize_t len = readlink("/proc/self/exe", self_buf, sizeof(self_buf) - 1);
    if (len > 0) {
        self_buf[len] = '\0';
        self_path = self_buf;
    }
#elif defined(__APPLE__)
    char self_buf[4096];
    uint32_t size = sizeof(self_buf);
    if (_NSGetExecutablePath(self_buf, &size) == 0) {
        self_path = self_buf;
    }
#elif defined(_WIN32)
    char self_buf[4096];
    GetModuleFileNameA(nullptr, self_buf, sizeof(self_buf));
    self_path = self_buf;
#endif
    return self_path;
}

std::string NormalizePath(const std::string& path) {
#ifdef _WIN32
    // Step 1: Convert POSIX-style /x/... to Windows-native X:\... format.
    // MinGW may supply paths like /e/ai_ws/... which GetFullPathNameA
    // treats as relative (not starting with X:\) and prepends CWD.
    std::string converted = path;
    if (converted.size() >= 3 && converted[0] == '/'
        && std::isalpha(static_cast<unsigned char>(converted[1]))
        && converted[2] == '/') {
        converted[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(converted[1])));
        converted[1] = ':';
    }

    // Step 2: Use Windows API to resolve relative paths (containing .. etc.)
    // to absolute. GetFullPathNameA uses the actual Windows CWD, not MSYS2's.
    char full[MAX_PATH];
    DWORD len = GetFullPathNameA(converted.c_str(), MAX_PATH, full, nullptr);
    if (len > 0 && len < MAX_PATH) {
        std::string result(full, len);
        for (auto& c : result) {
            if (c == '/') c = '\\';
        }
        return result;
    }
    return converted;
#else
    return path;
#endif
}

bool PathExists(const std::string& path) {
    return std::filesystem::exists(NormalizePath(path));
}

std::string SelectPlatformPath(const std::string& default_path, const std::string& win_path) {
#ifdef _WIN32
    return win_path.empty() ? default_path : win_path;
#else
    (void)win_path;
    return default_path;
#endif
}

bool CheckPortOpen(int port) {
    std::string url = "http://127.0.0.1:" + std::to_string(port);
    std::string result = RunShellCommand(
        ("curl -s -o " + std::string(NullDevice()) + " -w \"%{http_code}\" --connect-timeout 2 "
         + url + " 2>" + NullDevice()).c_str());
    return !result.empty();
}

std::string RunShellCommand(const char* cmd) {
#ifdef _WIN32
    FILE* fp = _popen(cmd, "r");
#else
    FILE* fp = popen(cmd, "r");
#endif
    if (!fp) return {};

    std::string result;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        result += buf;
    }
#ifdef _WIN32
    _pclose(fp);
#else
    pclose(fp);
#endif
    return result;
}

const char* NullDevice() {
#ifdef _WIN32
    return "NUL";
#else
    return "/dev/null";
#endif
}

Subprocess LaunchProcess(const std::vector<std::string>& args) {
    if (args.empty()) return {};

#ifdef _WIN32
    std::string cmd_line;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmd_line += " ";
        if (args[i].find(' ') != std::string::npos) {
            cmd_line += "\"" + args[i] + "\"";
        } else {
            cmd_line += args[i];
        }
    }

    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    // Prepend MinGW bin directory to PATH so child processes (llama-server)
    // can find MinGW runtime DLLs at runtime.
    char saved_path[32768] = {};
    GetEnvironmentVariableA("PATH", saved_path, sizeof(saved_path));
    bool found_mingw = false;

    char module_path[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, module_path, sizeof(module_path))) {
        auto p = std::filesystem::path(module_path).parent_path();
        while (p.has_parent_path()) {
            auto candidate = p / "mingw64" / "bin";
            if (std::filesystem::exists(candidate)) {
                SetEnvironmentVariableA("PATH", (candidate.string() + ";" + saved_path).c_str());
                found_mingw = true;
                break;
            }
            p = p.parent_path();
        }
    }
    if (!found_mingw) {
        char ml_buf[MAX_PATH] = {};
        DWORD ml = GetEnvironmentVariableA("MINGW_PREFIX", ml_buf, sizeof(ml_buf));
        if (ml > 0 && ml < MAX_PATH) {
            SetEnvironmentVariableA("PATH", (std::string(ml_buf) + "/bin;" + saved_path).c_str());
        }
    }

    if (!CreateProcessA(nullptr, cmd_line.data(), nullptr, nullptr, FALSE,
                        CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                        nullptr,
                        nullptr, &si, &pi)) {
        if (saved_path[0]) SetEnvironmentVariableA("PATH", saved_path);
        return {};
    }
    Subprocess proc{ static_cast<int>(pi.dwProcessId) };
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (saved_path[0]) SetEnvironmentVariableA("PATH", saved_path);
    return proc;
#else
    pid_t pid = fork();
    if (pid < 0) return {};

    if (pid == 0) {
        std::vector<const char*> argv;
        for (const auto& a : args) {
            argv.push_back(a.c_str());
        }
        argv.push_back(nullptr);

        signal(SIGHUP, SIG_IGN);

        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null >= 0) {
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }

        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    return Subprocess{ static_cast<int>(pid) };
#endif
}

int LaunchDetachedCommand(const std::string& command) {
    // Run command and capture PID (script must background and echo PID via "&; echo $!")
    std::string pid_str = RunShellCommand(command.c_str());
    if (pid_str.empty()) return -1;
    return std::atoi(pid_str.c_str());
}

bool IsProcessAlive(int pid) {
    if (pid <= 0) return false;

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) return false;
    DWORD exit_code;
    BOOL alive = GetExitCodeProcess(hProcess, &exit_code) && exit_code == STILL_ACTIVE;
    CloseHandle(hProcess);
    return alive != FALSE;
#else
    return kill(pid, 0) == 0;
#endif
}

bool KillProcess(int pid, bool force) {
    if (pid <= 0) return true;

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    bool ok = TerminateProcess(hProcess, force ? 1 : 0) != 0;
    if (ok) WaitForSingleObject(hProcess, force ? 5000 : 3000);
    CloseHandle(hProcess);
    return ok;
#else
    if (force) {
        kill(pid, SIGKILL);
    } else {
        kill(pid, SIGTERM);
        int status;
        waitpid(pid, &status, WNOHANG);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (kill(pid, 0) == 0) {
            kill(pid, SIGKILL);
        }
    }
    int status;
    waitpid(pid, &status, 0);
    return true;
#endif
}

CommandOutput RunCommandWithOutput(const std::string& command,
                                    int timeout_seconds,
                                    const std::string& workdir) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr, write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return {"Failed to create pipe", -1};
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE input_read = nullptr, input_write = nullptr;
    if (!CreatePipe(&input_read, &input_write, &sa, 0)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return {"Failed to create input pipe", -1};
    }
    SetHandleInformation(input_read, HANDLE_FLAG_INHERIT, 0);
    CloseHandle(input_write);

    std::wstring wcmd_line = Utf8ToWide(command);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = input_read;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring wworkdir = workdir.empty() ? L"" : Utf8ToWide(workdir);

    BOOL ok = CreateProcessW(nullptr, wcmd_line.data(), nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW,
                             nullptr,
                             wworkdir.empty() ? nullptr : wworkdir.c_str(),
                             &si, &pi);
    CloseHandle(write_pipe);

    if (!ok) {
        CloseHandle(input_read);
        CloseHandle(read_pipe);
        return {"Failed to create process", -1};
    }

    std::string result;
    char buffer[1024];
    auto start = std::chrono::steady_clock::now();

    for (;;) {
        DWORD wait_ms = 100;
        if (timeout_seconds > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            int64_t timeout_ms = timeout_seconds * 1000;
            if (elapsed_ms >= timeout_ms) {
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(input_read);
                CloseHandle(read_pipe);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return {"Command timeout", -2};
            }
            wait_ms = std::min(wait_ms, static_cast<DWORD>(timeout_ms - elapsed_ms));
        }

        DWORD avail = 0;
        if (!PeekNamedPipe(read_pipe, nullptr, 0, nullptr, &avail, nullptr)) {
            if (WaitForSingleObject(pi.hProcess, wait_ms) == WAIT_OBJECT_0) break;
            continue;
        }
        if (avail == 0) {
            if (WaitForSingleObject(pi.hProcess, wait_ms) == WAIT_OBJECT_0) break;
            continue;
        }

        DWORD bytes_read = 0;
        if (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
            buffer[bytes_read] = '\0';
            result += NativeToUtf8(std::string(buffer, bytes_read));
        }
    }

    DWORD bytes_read = 0;
    while (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        result += NativeToUtf8(std::string(buffer, bytes_read));
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(input_read);
    CloseHandle(read_pipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return {result, static_cast<int>(exit_code)};

#else
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return {"Failed to create pipe", -1};
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return {"Failed to fork", -1};
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        if (!workdir.empty()) {
            if (chdir(workdir.c_str()) != 0) {
                _exit(127);
            }
        }

        execl("/bin/sh", "/bin/sh", "-c", command.c_str(), (char*)nullptr);
        _exit(127);
    }

    close(pipefd[1]);

    std::string result;
    char buffer[4096];
    auto start = std::chrono::steady_clock::now();

    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(pipefd[0], &readfds);

        int select_result = select(pipefd[0] + 1, &readfds, nullptr, nullptr, &tv);

        if (select_result > 0 && FD_ISSET(pipefd[0], &readfds)) {
            ssize_t n = read(pipefd[0], buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                result += std::string(buffer, n);
            } else if (n == 0) {
                break;
            }
        }

        if (timeout_seconds > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= timeout_seconds) {
                kill(pid, SIGKILL);
                close(pipefd[0]);
                waitpid(pid, nullptr, 0);
                return {"Command timeout", -2};
            }
        }

        int stat;
        int wait_result = waitpid(pid, &stat, WNOHANG);
        if (wait_result > 0) {
            while (read(pipefd[0], buffer, sizeof(buffer) - 1) > 0) {}
            close(pipefd[0]);
            return {result, WIFEXITED(stat) ? WEXITSTATUS(stat) : -1};
        } else if (wait_result < 0) {
            close(pipefd[0]);
            return {result, -1};
        }
    }

    close(pipefd[0]);

    int stat;
    waitpid(pid, &stat, 0);
    return {result, WIFEXITED(stat) ? WEXITSTATUS(stat) : -1};
#endif
}

// CreatePipe/ClosePipe/ReadPipe/WritePipe/Dup2Pipe/ForkAndExec/WaitProcess
// are in pipe_handler_posix.cc / pipe_handler_win32.cc

bool SetPipeNonBlocking(int fd) {
    if (fd < 0) return false;
#ifndef _WIN32
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#else
    (void)fd;
    return false;
#endif
}

bool IsPipeWouldBlock() {
#ifndef _WIN32
    return errno == EAGAIN || errno == EWOULDBLOCK;
#else
    return false;
#endif
}

std::string GetPipeErrorString() {
#ifndef _WIN32
    return strerror(errno);
#else
    LPVOID buf = nullptr;
    DWORD err = GetLastError();
    if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                       nullptr, err, 0, (LPSTR)&buf, 0, nullptr) > 0) {
        std::string msg((LPSTR)buf);
        LocalFree(buf);
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) {
            msg.pop_back();
        }
        return msg;
    }
    return "Unknown error";
#endif
}

// ForkAndExec/WaitProcess are in pipe_handler_posix.cc / pipe_handler_win32.cc

ScriptResult ExecuteScriptWithTimeout(const std::string& script_path, int timeout_ms) {
    ScriptResult result;

    if (!PathExists(script_path)) {
        result.return_code = -1;
        result.error_output = "Script not found: " + script_path;
        return result;
    }

    std::string command = script_path;

#ifdef _WIN32
    if (script_path.size() >= 3 && script_path.substr(script_path.size() - 3) == ".py") {
        command = "python \"" + script_path + "\"";
    } else if (script_path.size() >= 3 && script_path.substr(script_path.size() - 3) == ".sh") {
        command = "bash \"" + script_path + "\"";
    }
#else
    std::filesystem::permissions(script_path,
        std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec,
        std::filesystem::perm_options::add);
#endif

    int timeout_sec = timeout_ms / 1000;
    if (timeout_sec < 1) timeout_sec = 1;

    auto cmd_result = RunCommandWithOutput(command, timeout_sec);

    result.return_code = cmd_result.exit_code;
    result.output = cmd_result.output;

    if (cmd_result.exit_code == -2) {
        result.timeout = true;
        result.error_output = "Script timeout after " + std::to_string(timeout_ms) + "ms";
    }

    return result;
}

}  // namespace platform
}  // namespace prosophor
