// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "managers/local_model_manager.h"

#include <filesystem>
#include <sstream>
#include <thread>

#include "common/local_model_utils.h"
#include "common/log_wrapper.h"
#include "platform/platform.h"

namespace prosophor {

LocalModelManager& LocalModelManager::GetInstance() {
    static LocalModelManager instance;
    return instance;
}

LocalModelManager::~LocalModelManager() {
    if (running_.load()) {
        Stop();
    }
}

bool LocalModelManager::Start(const LocalModelConfig& config) {
    if (running_.load()) {
        LOG_DEBUG("Local model server already running on port {}", config_.port);
        return true;
    }

    std::string server_path = config.server_path;
    if (server_path.empty()) {
        server_path = FindServerBinary();
    }
    if (server_path.empty()) {
        LOG_ERROR("llama-server binary not found. Use /setup to configure or set server_path in settings.json.");
        return false;
    }
    if (!platform::PathExists(server_path)) {
        LOG_ERROR("llama-server not found at: {}", server_path);
        return false;
    }
    if (!platform::PathExists(config.model_path)) {
        LOG_ERROR("Model file not found: {}", config.model_path);
        return false;
    }

    LOG_INFO("Starting llama-server: {} -> {}:{}", server_path, config.model_path, config.port);

    std::ostringstream cmd;
    cmd << platform::ShellEscape(server_path);
    cmd << " -m " << platform::ShellEscape(config.model_path);
    cmd << " --port " << config.port;
    cmd << " --host 127.0.0.1";

    if (config.n_gpu_layers > 0) {
        cmd << " -ngl " << config.n_gpu_layers;
    } else if (config.n_gpu_layers == -1) {
        cmd << " -ngl 999";
    }

    if (config.n_threads > 0) {
        cmd << " -t " << config.n_threads;
    }

    cmd << " -c 4096";

    int pid = platform::LaunchDetachedCommand(cmd.str());
    if (pid < 0) {
        LOG_ERROR("Failed to start llama-server");
        return false;
    }

    pid_ = pid;
    config_ = config;
    running_.store(true);

    if (!WaitForPort(config.port, config.start_timeout_ms)) {
        LOG_ERROR("llama-server failed to start (timeout waiting for port {})", config.port);
        Stop();
        return false;
    }

    // Wait for model to finish loading via /health (200 = ready, 503 = loading)
    if (!WaitForHealth(config.port, config.start_timeout_ms)) {
        LOG_ERROR("llama-server failed to become ready within {}ms (health endpoint)",
                  config.start_timeout_ms);
        Stop();
        return false;
    }

    LOG_INFO("llama-server ready on port {} (PID: {}, model loaded)", config.port, pid_);
    return true;
}

void LocalModelManager::Stop() {
    if (!running_.load()) return;

    LOG_INFO("Stopping llama-server (PID: {})", pid_);

    if (pid_ > 0) {
        if (!platform::KillProcess(pid_, false)) {
            LOG_WARN("Failed to stop llama-server gracefully, forcing...");
            platform::KillProcess(pid_, true);
        }
    }

    pid_ = -1;
    running_.store(false);
    LOG_INFO("llama-server stopped");
}

bool LocalModelManager::IsRunning() const {
    if (!running_.load()) return false;

    // Check process first (reliable), then port (best-effort)
    if (pid_ > 0 && !platform::IsProcessAlive(pid_)) {
        running_.store(false);
        return false;
    }

    if (pid_ > 0) return true;

    // No PID tracked — fall back to port check (may false-positive on WSL2)
    if (platform::CheckPortOpen(config_.port)) return true;

    running_.store(false);
    return false;
}

bool LocalModelManager::WaitForPort(int port, int timeout_ms) const {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        if (platform::CheckPortOpen(port)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

bool LocalModelManager::WaitForHealth(int port, int timeout_ms) const {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/health";
    int log_throttle = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        // /health returns 503 while model loads, 200 when ready
        std::string result = platform::RunShellCommand(
            ("curl -s -o /dev/null -w \"%{http_code}\" " + url + " 2>/dev/null").c_str());
        if (result == "200") {
            LOG_INFO("llama-server is ready");
            return true;
        }
        if (++log_throttle % 5 == 0) {
            LOG_INFO("Waiting for model to finish loading (health={}, timeout={}ms)...",
                     result, timeout_ms);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    return false;
}

}  // namespace prosophor
