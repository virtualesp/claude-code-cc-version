// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/noncopyable.h"
#include "config/config.h"
#include "core/messages_schema.h"
#include "core/agent_types.h"

namespace prosophor {

// Forward declarations
class MemoryManager;
class ToolRegistry;
class AgentSessionManager;
class ProviderRouter;
class CommandRegistry;

/// AgentEngine: core business logic shared by all frontends (Terminal, SDL, etc.)
/// Manages sessions, tools, providers, and commands.
/// Frontends register callbacks to receive output and handle permissions.
class AgentEngine : public Noncopyable {
 public:
    static AgentEngine& GetInstance();

    /// Called by the frontend when an agent response state changes.
    using OutputCallback = std::function<void(
        AgentRuntimeState state,
        const std::string& state_msg,
        const std::optional<MessageSchema>& reply)>;

    /// Called by the engine when a tool requires user permission.
    /// Returns true to allow, false to deny.
    using PermissionCallback = std::function<bool(
        const std::string& tool_name,
        const nlohmann::json& input,
        const std::string& reason)>;

    /// Register the output callback (replaces any previous registration).
    void SetOutputCallback(OutputCallback cb);

    /// Register the permission callback (replaces any previous registration).
    void SetPermissionCallback(PermissionCallback cb);

    /// Send a user message to the current session.
    void ProcessUserMessage(const std::string& text);

    /// Execute a slash command. Returns true if the command was handled.
    bool HandleCommand(const std::string& line);

    /// Switch the current session to a different role.
    void SwitchRole(const std::string& role_id);

    /// Request the current session to stop.
    void StopCurrentSession();

    /// List available role IDs.
    std::vector<std::string> ListRoles() const;

    /// List active session IDs.
    std::vector<std::string> ListSessions() const;

    const ProsophorConfig& GetConfig() const { return config_; }
    const std::string& GetCurrentSessionId() const { return current_session_id_; }

 private:
    AgentEngine();
    ~AgentEngine();

    void InitializeComponents();

    ProsophorConfig config_;
    AgentConfig agent_config_;
    std::string workspace_path_;
    std::string current_session_id_;
    std::atomic<int64_t> last_interaction_time_{0};

    std::shared_ptr<MemoryManager> memory_manager_;
    ToolRegistry*        tool_registry_   = nullptr;
    AgentSessionManager* session_manager_ = nullptr;
    ProviderRouter*      provider_router_ = nullptr;
    CommandRegistry*     command_registry_ = nullptr;

    OutputCallback     output_callback_;
    PermissionCallback permission_callback_;
};

}  // namespace prosophor
