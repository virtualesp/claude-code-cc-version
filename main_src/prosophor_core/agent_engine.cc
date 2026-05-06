// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "agent_engine.h"

#include <iostream>

#include "common/log_wrapper.h"
#include "common/string_utils.h"
#include "common/file_utils.h"
#include <filesystem>
#include "managers/memory_manager.h"
#include "managers/agent_session_manager.h"
#include "managers/agent_role_loader.h"
#include "managers/local_model_manager.h"
#include "command_registry.h"
#include "tools/tool_registry.h"
#include "providers/provider_router.h"
#include "services/lsp_manager.h"

namespace prosophor {

AgentEngine& AgentEngine::GetInstance() {
    static AgentEngine instance;
    return instance;
}

AgentEngine::AgentEngine()
    : workspace_path_(std::filesystem::current_path().string()) {
    InitializeComponents();
}

AgentEngine::~AgentEngine() {
    if (memory_manager_) {
        memory_manager_->StopFileWatcher();
    }
    if (LocalModelManager::GetInstance().IsRunning()) {
        LocalModelManager::GetInstance().Stop();
    }
}

void AgentEngine::InitializeComponents() {
    LOG_DEBUG("Initializing AgentEngine components...");

    config_ = prosophor::ProsophorConfig::GetInstance();

    EnsureDirectory(workspace_path_);

    memory_manager_ = std::make_shared<MemoryManager>(workspace_path_);
    memory_manager_->LoadWorkspaceFiles();
    memory_manager_->StartFileWatcher();

    tool_registry_ = &ToolRegistry::GetInstance();
    tool_registry_->SetWorkspace(workspace_path_);

    // Permission callback: frontends override this via SetPermissionCallback().
    // Default denies all to avoid silent approvals when no frontend is attached.
    tool_registry_->SetPermissionConfirmCallback(
        [this](const std::string& tool_name, const nlohmann::json& input,
               const std::string& reason) -> bool {
            if (permission_callback_) {
                return permission_callback_(tool_name, input, reason);
            }
            LOG_WARN("No permission callback registered; denying tool '{}'", tool_name);
            return false;
        });

    session_manager_ = &AgentSessionManager::GetInstance();

    ToolExecutorCallback tool_executor =
        [this](const std::string& tool_name, const nlohmann::json& args) -> std::string {
            return tool_registry_->ExecuteTool(tool_name, args);
        };

    session_manager_->Initialize(memory_manager_, tool_executor);

    // Route session output to the registered output callback.
    session_manager_->SetOutputCallback(
        [this](const std::string& /*session_id*/, const std::string& /*role_id*/,
               AgentRuntimeState state, const std::string& state_msg,
               const std::optional<MessageSchema>& reply) {
            if (output_callback_) {
                output_callback_(state, state_msg, reply);
            }
        });

    provider_router_ = &ProviderRouter::GetInstance();
    provider_router_->Initialize(config_);

    auto& lsp_manager = prosophor::LspManager::GetInstance();
    lsp_manager.Initialize();
    LOG_DEBUG("LSP integration initialized with {} servers",
              lsp_manager.GetRegisteredServers().size());

    command_registry_ = &CommandRegistry::GetInstance();
    command_registry_->Initialize();

    std::string default_role_id = config_.default_role;
    if (!session_manager_->GetRole(default_role_id)) {
        LOG_WARN("Default role '{}' not found, using first available role", default_role_id);
        auto roles = session_manager_->ListRoles();
        if (!roles.empty()) {
            default_role_id = roles[0];
        } else {
            LOG_ERROR("No roles available, using hardcoded 'default'");
            default_role_id = "default";
        }
    }

    current_session_id_ = session_manager_->CreateSession(default_role_id, "Default session");

    LOG_DEBUG("AgentEngine initialized, session={} role={}", current_session_id_, default_role_id);

    if (!config_.local_models.empty() && config_.local_models[0].auto_start) {
        auto& lm = config_.local_models[0];
        if (!LocalModelManager::GetInstance().Start(lm)) {
            LOG_WARN("Failed to auto-start local model server.");
        } else {
            LOG_INFO("Local model server started on port {}", lm.port);
        }
    }
}

void AgentEngine::SetOutputCallback(OutputCallback cb) {
    output_callback_ = std::move(cb);
}

void AgentEngine::SetPermissionCallback(PermissionCallback cb) {
    permission_callback_ = std::move(cb);
}

void AgentEngine::ProcessUserMessage(const std::string& text) {
    if (!text.empty() && text[0] == '/') {
        HandleCommand(text);
        return;
    }
    try {
        last_interaction_time_ = std::time(nullptr);
        session_manager_->SendToSessionAsync(current_session_id_, text);
    } catch (const std::exception& e) {
        LOG_ERROR("ProcessUserMessage error: {}", e.what());
    }
}

bool AgentEngine::HandleCommand(const std::string& line) {
    if (line.empty() || line[0] != '/') {
        return false;
    }

    std::vector<std::string> args = CommandRegistry::ParseCommandLine(line);
    if (args.empty()) {
        return false;
    }

    std::string cmd_name = args[0].substr(1);
    std::vector<std::string> cmd_args(args.begin() + 1, args.end());

    if (cmd_name == "role" && !cmd_args.empty()) {
        SwitchRole(cmd_args[0]);
        return true;
    }

    CommandContext ctx;
    ctx.workspace   = workspace_path_;
    ctx.session_id  = current_session_id_;
    ctx.user_data   = this;
    ctx.agent_session = session_manager_->GetSession(current_session_id_);

    auto result = CommandRegistry::GetInstance().ExecuteCommand(cmd_name, cmd_args, ctx);
    if (!result.output.empty()) {
        std::cout << result.output << std::endl;
    } else if (!result.success) {
        std::cout << result.error << "\n";
    }
    return true;
}

void AgentEngine::SwitchRole(const std::string& role_id) {
    const AgentRole* role = session_manager_->GetRole(role_id);
    if (!role) {
        std::cout << "Role not found: " << role_id << "\nAvailable: ";
        for (const auto& r : session_manager_->ListRoles()) {
            std::cout << r << " ";
        }
        std::cout << "\n";
        return;
    }
    current_session_id_ = session_manager_->CreateSession(role->id, "Switched to " + role->name);
    LOG_INFO("Switched to role: {} (session: {})", role->id, current_session_id_);
}

void AgentEngine::StopCurrentSession() {
    auto* session = session_manager_->GetSession(current_session_id_);
    if (session) {
        session->stop_requested = true;
    }
}

std::vector<std::string> AgentEngine::ListRoles() const {
    return session_manager_->ListRoles();
}

std::vector<std::string> AgentEngine::ListSessions() const {
    return session_manager_->ListSessions();
}

}  // namespace prosophor
