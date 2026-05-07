// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/noncopyable.h"
#include "common/input_event.h"
#include "scene/home_screen.h"
#include "core/agent_types.h"
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace prosophor {

/// Per-session render state cached by the OutputCallback.
struct SessionRenderState {
    std::string role_id;
    AgentRuntimeState agent_state = AgentRuntimeState::IDLE;
};

/// VirtualSprite: SDL-based graphical interface entry point
class VirtualSprite : public Noncopyable {
 public:
    static VirtualSprite& GetInstance();

    int Run();
    void Stop();

    void HandleTextInput(const char* text);
    void HandleKeyDown(int key_code);
    void HandleMouseButtonDown(int x, int y);

    using InputCallback = std::function<void(const InputEvent&)>;
    void SetInputCallback(InputCallback callback);

    UIMode GetCurrentMode() const { return current_scene_; }

 private:
    VirtualSprite();
    ~VirtualSprite();

    void Initialize();
    void Shutdown();
    void SwitchMode(UIMode mode);

    void RenderHome();
    void RenderVirtualHuman();
    void RenderGalgame();
    void RenderTerminal();

    /// Register OutputCallback: writes into session_states_ (called once at mode switch)
    void RegisterAgentOutputCallback();

    /// Register UIRenderer message submit callback
    void RegisterMessageSubmitCallback();

    InputCallback input_callback_;
    UIMode current_scene_ = UIMode::HOME;
    InputCallback saved_callback_;

    // SDL-side global state: keyed by session_id, written by OutputCallback (worker thread),
    // read by render loop (main thread). Mutex guards concurrent access.
    std::unordered_map<std::string, SessionRenderState> session_states_;
    std::mutex session_states_mutex_;
};

}  // namespace prosophor
