// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace prosophor {

/// Agent 运行时状态
enum class AgentRuntimeState {
    IDLE,
    BEGINNING,
    EXECUTING_TOOL,
    TOOL_USE,
    WAITING_PERMISSION,
    STATE_ERROR,
    COMPLETE,
    STREAM_CONTENT_TYPING,    // 流式响应中
    STREAM_MODE_COMPLETE,     // 流式响应完成
    STREAM_THINKING_START,    // 流式思考开始
    STREAM_THINKING,          // 流式思考中
    STREAM_THINKING_END,      // 流式思考结束
    STREAM_CONTENT_START,     // 流式内容开始
    STREAM_CONTENT_END        // 流式内容结束
};

}  // namespace prosophor
