// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <string>

#include "core/agent_types.h"

namespace prosophor {

/// 聊天消息结构
struct ChatMessage {
    std::string role;
    std::string name;
    std::string content;
    double timestamp;
};

/// 状态视觉属性
struct StateVisualProps {
    uint8_t r, g, b, a;
    std::string name;
};

}  // namespace prosophor
