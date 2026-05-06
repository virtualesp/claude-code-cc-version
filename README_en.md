# Prosophor

<div align="right">

**English** | [**中文**](README.md)

</div>

<div align="center">

**The Proactive Agentic CLI — from passive response to proactive interaction**

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus&logoColor=white)](https://en.cppreference.com/)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)]()

</div>

---

## 📋 Table of Contents

- [Overview](#overview)
- [White Paper: Core Architecture](#white-paper-core-architecture)
- [Proactive Trigger Architecture — Core Innovation](#proactive-trigger-architecture--core-innovation)
- [Role System](#role-system)
- [LLM Provider System & Model Switching](#llm-provider-system--model-switching)
- [Tool System](#tool-system)
- [Configuration Guide](#configuration-guide)
- [MCP Protocol](#mcp-protocol)
- [SDL Graphical UI](#sdl-graphical-ui)
- [Context Compression & Memory](#context-compression--memory)
- [Permission Management](#permission-management)
- [Skill System](#skill-system)
- [LSP Integration](#lsp-integration)
- [Cron Scheduler](#cron-scheduler)
- [Slash Commands](#slash-commands)
- [Quick Start](#quick-start)
- [Build from Source](#build-from-source)
- [Project Structure](#project-structure)
- [License](#license)

---

## Overview

Prosophor is a **proactive Agentic CLI** built with **native C++17**. It transcends the traditional command-response paradigm by implementing a **plugin-based proactive trigger architecture** that perceives context, predicts needs, and initiates interaction autonomously.

| Dimension | Traditional CLI | Prosophor |
|-----------|----------------|-----------|
| **Interaction** | Passive response | **Proactive trigger** (periodic / idle / idle_once) |
| **Architecture** | Monolithic | **Plugin-based** (hot-swappable trigger plugins) |
| **LLM** | Single provider | **Multi-LLM** (Claude, Ollama, OpenAI-compatible, local GGUF) |
| **Runtime** | Node.js / interpreted | **Native C++** (zero runtime dependency) |
| **License** | Proprietary | **Apache 2.0** |

### Core Positioning

| Aspect | Description |
|--------|-------------|
| **Runtime** | C++ native, zero runtime dependencies |
| **Core Capabilities** | Proactive perception, autonomous planning, tool invocation, environment feedback |
| **Interaction** | Passive response + Proactive trigger dual mode |
| **Supported LLMs** | Anthropic (Claude), Ollama (local models), OpenAI-compatible (any API), llama.cpp (local GGUF) |
| **Frontends** | Terminal TUI + SDL Graphical UI (ImGui, animated characters, galgame mode) |

---

## White Paper: Core Architecture

### Architectural Philosophy

Prosophor's architecture is built on a **clean separation between engine and presentation**. The `prosophor_core` static library contains all business logic with zero UI dependencies, while frontends (Terminal TUI and SDL graphical UI) register callbacks to receive output and handle permissions.

```
Frontend Layer:
  ┌───────────────────────┐      ┌──────────────────────────────┐
  │   Terminal TUI        │      │   SDL Graphical UI           │
  │   (AiCoding)          │      │   VirtualSprite              │
  │   - Line input        │      │   - HOME / VIRTUAL_HUMAN     │
  │   - Stream output     │      │   - GALGAME / TERMINAL       │
  │   - Permission prompts│      │                              │
  └──────────┬────────────┘      └────────────┬─────────────────┘
             │ SetOutputCallback              │
             │ SetPermissionCallback          │
             ▼                                ▼
  ┌────────────────────────────────────────────────────────────┐
  │  AgentEngine (Singleton)                                   │
  │  ┌──────────┬──────────┬──────────┬──────────┐             │
  │  │ Tool     │ Session  │ Provider │ Command  │             │
  │  │ Registry │ Manager  │  Router  │ Registry │             │
  │  ├──────────┼──────────┼──────────┼──────────┤             │
  │  │ Memory   │  LSP     │  MCP     │  Cron    │             │
  │  │ Manager  │ Manager  │  Client  │Scheduler │             │
  │  └──────────┴──────────┴──────────┴──────────┘             │
  └────────────────────────────────────────────────────────────┘
```

### AgentEngine — Central Orchestrator

`AgentEngine` is the singleton heart of Prosophor, providing several key methods to frontends: an output callback for receiving agent response state changes, a permission callback for tool authorization, methods to process user messages (which auto-detect slash commands and route accordingly), execute slash commands directly, switch the active role, and stop the current session.

**Initialization sequence**:
1. Load config from `~/.prosophor/settings.json`
2. Initialize MemoryManager (load workspace files, start file watcher)
3. Initialize ToolRegistry (register all built-in tools)
4. Initialize AgentSessionManager (create default session)
5. Initialize ProviderRouter (load provider configurations)
6. Initialize LspManager (start language servers)
7. Initialize CommandRegistry (register all slash commands)
8. Create the initial session with the default role
9. Auto-start local model server if `auto_start: true` is set

### REACT Agent Loop

`AgentCore::Loop()` implements the **REACT paradigm** (Reasoning + Acting):

```
User Message
    │
    ▼
  1. Process @file references
  2. Maybe compact context
  3. Build ChatRequest from session state + role config
    │
    ▼
  4. Call LLM (stream or block)
     If thinking mode: emit kThinkingStart → kThinkingDelta → kThinkingEnd
     Emit kContentStart → kContentDelta → kContentEnd
    │
    ▼
  ┌─ Has tool calls? ─┐
  │                   │
  Yes                 No
  │                   │
  ▼                   ▼
  Execute each tool:  Has text response?
  - Check stop flag      │           │
  - Execute tool       Yes          No
  - Truncate result      │           │
  - Append to history    ▼           ▼
  iterations++         Return     Error: unexpected
  if < max_iterations:  response   response format
    → back to step 3
  else: stop
```

**Key design decisions**:
- **Streaming-first**: When streaming is enabled, the loop pushes fine-grained stream events (`kThinkingStart/Delta/End`, `kContentStart/Delta/End`) for real-time UI rendering
- **Tool result truncation**: Large tool outputs are intelligently truncated, preserving head and tail lines with a `[... N lines omitted]` indicator
- **Stop-anytime**: A `stop_requested` atomic flag allows immediate interruption with proper error propagation
- **Automatic compaction**: Context compaction triggers before each LLM call when messages exceed configurable thresholds

### Provider System & Routing

The `ProviderRouter` dynamically routes requests based on role configuration, mapping roles to their bound providers either by role ID or by provider name.

**Four provider types**, all implementing a unified `LLMProvider` interface:
- `AnthropicProvider` — Anthropic Messages API with SSE streaming
- `OpenAIProvider` — OpenAI-compatible API (also DeepSeek, any OpenAI-format)
- `OllamaProvider` — Local models via Ollama
- `llama.cpp` — Local GGUF models via llama-server (reuses OpenAI format)

**Session-level provider override**: When a user switches provider or model at runtime (via `/model` or `/provider`), the session creates a mutable copy of the role, updates the provider instance, base URL, API key, and model parameters — all without affecting the shared role definition. This allows concurrent sessions with different overrides sharing the same role template.

### Memory Architecture

Prosophor implements a **dual-layer memory architecture**:

```
                         Memory Architecture
  ┌──────────────────────────────┬──────────────────────────────┐
  │      Role Memory             │      Session History         │
  │      (Long-term)             │      (Short-term)            │
  ├──────────────────────────────┼──────────────────────────────┤
  │ ~/.prosophor/memories/       │ ~/.prosophor/sessions/       │
  │   {role_id}/                 │   {session_id}/history/      │
  ├──────────────────────────────┼──────────────────────────────┤
  │ Habits & preferences         │ Full conversation trace      │
  │ Learned patterns             │ Tool execution results       │
  │ Key decisions                │ Intermediate decisions       │
  │ Daily summaries              │ Workspace context            │
  ├──────────────────────────────┼──────────────────────────────┤
  │ Persists across projects     │ Within project session       │
  │ and sessions                 │ (compacted or cleared)       │
  └──────────────────────────────┴──────────────────────────────┘
```

**MemoryConsolidationService**:
- Extracts **key decisions** from conversations in four categories: design decisions, code changes, unresolved issues, and lessons learned
- Triggers every N messages (configurable threshold, default 30)
- Saves consolidated summaries to role memory directory
- Generates session exit summaries for permanent record and cross-session continuity

---

## Proactive Trigger Architecture — Core Innovation

Traditional tools wait for commands. Prosophor proactively perceives and responds through a **three-layer plugin architecture**:

```
Plugin Community (Upload → Audit → Distribute → Update)
                          │
                          ▼
Plugin Layer ─ trigger script + mode config + ACTIVE.md
Scheduling   ─ periodic / idle / idle_once · priority
Execution    ─ AgentCore + ToolRegistry + LLM linkage
```

### Trigger Modes

| Mode | Trigger | Use Case |
|------|---------|----------|
| `periodic` | Every N seconds | Critical alerts (hardware temp, errors) |
| `idle` | After N seconds idle | Reminders, suggestions |
| `idle_once` | Once per idle session | One-time guidance |

### Plugin Structure

Each plugin lives in its own directory under `~/.prosophor/active/{plugin_name}/`:

- `cpu_temperature_monitor/` — `trigger_mode.cfg` (mode=periodic, interval=60), `trigger.py` (detection script), `prompt.md` (LLM prompt with {variables})
- `file_organizer/` — `trigger_mode.cfg` (mode=idle, threshold=300), `trigger.py`, `prompt.md`
- `new_user_guide/` — `trigger_mode.cfg` (mode=idle_once, threshold=120), `trigger.py`, `prompt.md`

### ActiveInteractionManager — Post-Interaction Proactivity

Beyond scheduled triggers, post-interaction proactivity adds two behaviors:
- **5 minutes after user question**: Proactively recommend related questions
- **10 minutes after user question**: Summarize conversation and save to changelog

This closes the loop: not only does Prosophor proactively initiate, it also reflects on its own interactions to deepen context.

---

## Role System

Roles are defined as **Markdown files with YAML frontmatter** at `~/.prosophor/roles/`. Each role is a complete agent persona with its own provider binding, model, personality, tools, skills, and long-term memory.

### Built-in Roles

| Role | ID | Provider | Model | Personality |
|------|----|----------|-------|-------------|
| **Default Assistant** | `default` | openai | google_gemma-4-E4B-it | Balanced, all-purpose |
| **Code Expert** | `coder` | ollama | qwen3:8b | Concise, coding-focused |
| **Architect** | `architect` | ollama | qwen3:8b | Structured, design-focused |
| **Reviewer** | `reviewer` | ollama | qwen3:8b | Detail-oriented, critical |
| **Teacher** | `teacher` | ollama | qwen3:8b | Patient, educational |

### Role Fields

A role definition includes: identity fields (ID, name, description, avatar emoji); provider binding (which provider protocol to use, default model, temperature, max tokens, streaming toggle, thinking toggle); personality configuration (personality type and detailed prompt); system instructions; capability configuration (whitelisted skills and tools); behavioral constraints (max tool call iterations, auto-confirm tools); and a dedicated memory directory for long-term storage.

### Runtime Override Mechanism

When a user switches provider or model at runtime (via `/model` or `/provider` command), the system creates a mutable copy of the role, redirects the session's role pointer to this copy, updates the provider instance, base URL, API key, and timeout, searches provider entries for the matching model to find the correct endpoint, and leaves the shared role definition untouched. This design allows concurrent sessions with different overrides sharing the same role template.

---

## LLM Provider System & Model Switching

Prosophor abstracts all LLM providers behind a unified interface and supports dynamic switching at session, role, and command level — all with a single command, no restart needed.

### Simplicity First

Unlike other agent frameworks that require editing config files or restarting to change models, Prosophor lets you switch models with just **`/model [name]`** or select by index with **`/model [idx]`** (e.g., `/model 2` picks the second available model). The same simplicity applies to switching entire providers (`/provider ollama`) or swapping roles (`/role coder`) — each creates a new session with the new configuration automatically resolved.

### Supported Providers

| Provider | Protocol | Best For |
|----------|----------|----------|
| **Anthropic** | anthropic-messages (SSE streaming) | Claude models with thinking/reasoning |
| **OpenAI-compatible** | openai-chat-completions | DeepSeek, GPT, any OpenAI-format API |
| **Ollama** | openai-completions (via Ollama) | Local models (Qwen, Gemma, etc.) |
| **llama.cpp** | openai-chat-completions (via llama-server) | GGUF local inference |

The abstract provider interface defines four core operations: a non-streaming chat method, a streaming chat method with per-event callbacks, and serialization/deserialization that each provider implements for its own wire format.

### Model Switching Commands

| Command | Operation |
|---------|-----------|
| `/model [name]` | Switch model by name (e.g., `/model gpt-4o`) |
| `/model [idx]`  | Switch model by index (e.g., `/model 2`) |
| `/provider [name]` | Switch provider (e.g., `/provider ollama`) |
| `/role [id]` | Switch role (creates new session with the role's default model) |
| `/setup` | Auto-detect hardware, scan GGUF models, generate config |

**Switching behavior**: `/model deepseek-v4-pro` or `/model 2` searches all provider entries for the model, finds the matching agent config, and updates the role's mutable copy. `/role coder` creates a fresh session using the coder role's provider/model. Provider-specific API keys and base URLs are automatically resolved from config — no file editing required.

### Configuration Structure

Each provider supports **multiple entries** (array), each with its own api_key and base_url. Each entry contains multiple **agent configs** with model, temperature, max_tokens, and context window settings. For example, an Anthropic provider entry might define a "default" agent using `claude-sonnet-4-6` with temperature 0.7 and a "fast" agent using `claude-haiku-4-5` with temperature 0.1. An OpenAI-compatible entry for DeepSeek might define "default" as `deepseek-v4-pro` and "fast" as `deepseek-v4-flash` with thinking enabled.

### Request Building Pipeline

The request assembler collects from session state: the model (from role, possibly overridden); the base URL (resolved from provider entry via agent config lookup); the API key (from provider entry); the full conversation history; the system prompt (role system prompt + personality + loaded memory files); the tool list (filtered by role's tool whitelist); and the thinking flag (enabled/disabled per request based on role config).

---

## Tool System

`ToolRegistry` centrally manages all tools with a schema-based registration system.

### Built-in Tools

| Category | Tools |
|----------|-------|
| **File Operations** | `read_file`, `write_file`, `edit_file`, `file_search` |
| **Shell Execution** | `bash`, `background_run` |
| **Search** | `web_search`, `web_fetch` |
| **Git** | `git_status`, `git_diff`, `git_log`, `git_commit`, `git_add`, `git_branch`, `git_checkout`, `git_push` |
| **MCP** | `mcp_list_tools`, `mcp_call_tool`, `mcp_list_resources`, `mcp_read_resource` |
| **Agent** | `agent` (sub-task decomposition and delegation) |
| **Planning** | `plan`, `task` |
| **Token** | `token_count`, `token_usage` |
| **Memory** | `memory_search`, `memory_get` |
| **Patch** | `apply_patch` |

### Tool Execution Flow

When the LLM responds with tool calls, each call goes through permission checking (Allow → continue, Deny → return error, Ask → user callback), then execution. Successful results are truncated to head and tail lines with a `[... N lines omitted]` indicator; errors are returned in full so the LLM has complete context for diagnosis. Results are appended to message history, and the loop continues.

### Permission Integration

Tool execution is gated by a permission manager supporting pattern matching by tool name, command pattern, and path pattern; three modes (Allow auto-approve, Deny auto-deny, Ask interactive); fallback logic that auto-allows after N sequential denials (default 3); and configurable levels: auto, default, and bypass.

---

## Configuration Guide

### Config File Location

First launch auto-generates config at **`~/.prosophor/settings.json`**.

### Configuration Overview

The configuration file has these top-level sections:

- **default_role**: The role to activate on startup (e.g., "default")
- **log_level**: Logging verbosity (debug, info, warn, error)
- **providers**: Each provider type (anthropic, openai, ollama) maps to an **array** of entries. Each entry has its own api_key, base_url, timeout, and a map of agent configs keyed by name. Each agent config specifies model name, temperature, max_tokens, context_window, and whether thinking is enabled.
- **local_models**: Array of local GGUF model configurations. Each entry specifies model_path (Linux/macOS), model_path_for_win (Windows alternative), port (default 8080), n_gpu_layers (-1 = all layers on GPU, 0 = CPU only), n_threads (0 = auto-detect), auto_start (whether to launch on startup), and start_timeout_ms.
- **security**: Contains permission_level (auto, default, or bypass) and allow_local_execute toggle.

### Local Model Setup

The `/setup` command automates local model configuration:
1. Detects **NVIDIA GPU** via nvidia-smi and sets GPU layers to all
2. Detects **Apple Silicon** via sysctl and enables Metal
3. Falls back to **CPU** with auto-detected thread count
4. Scans common directories for `.gguf` files
5. Generates `settings.json` with optimal parameters

### Model Auto-Start

When `auto_start: true` is set, the engine automatically starts llama-server before creating any sessions. Start/stop scripts are provided at `~/.prosophor/bin/scripts/`.

---

## MCP Protocol

Prosophor implements a full **Model Context Protocol (MCP)** client for standardized tool/resource/prompt sharing.

### Transports

| Transport | Description |
|-----------|-------------|
| **stdio** | Server as subprocess stdin/stdout |
| **SSE** | Server-Sent Events over HTTP |
| **WebSocket** | Bidirectional WebSocket |

### MCP Server Management

- `/mcp list` — List connected servers
- `/mcp add <name> <type> <config>` — Add a server
- `/mcp remove <name>` — Remove a server

### MCP Client Capabilities

The MCP client can connect to servers (stdio, SSE, or WebSocket), discover tools, call tools, read resources, and retrieve prompts. MCP tools are dynamically registered into the ToolRegistry and exposed to the LLM as native tools.

---

## SDL Graphical UI

When built with `PROSOPHOR_SDL_UI=ON`, Prosophor provides a full graphical interface powered by SDL3 and ImGui.

### UI Modes

| Mode | Description |
|------|-------------|
| **HOME** | Dashboard / launcher screen |
| **VIRTUAL_HUMAN** | Animated character companion with chat |
| **GALGAME** | Visual novel style interaction |
| **TERMINAL** | In-app terminal emulation |

### Visual Components

| Component | Description |
|-----------|-------------|
| **AnimeCharacter** | Animated anime-style character rendering |
| **PixelCharacter** | Pixel-art character with sprite animations |
| **CharacterSprite** | Multi-frame sprite system |
| **OfficeBackground** | Office scene background |
| **ChatPanel** | Chat message display |
| **StatusBar** | Agent state visualization |
| **HeaderBar** | Session info and controls |

### State-to-Visual Mapping

The agent state observer maps runtime states to visual properties: `THINKING` triggers character thinking animation, `EXECUTING_TOOL` shows a tool execution indicator, `STREAM_CONTENT_TYPING` displays typing animation, and `STATE_ERROR` shows an error state visual.

---

## Context Compression & Memory

### CompactService

Three compaction strategies:

| Strategy | Behavior |
|----------|----------|
| **Summary** | Generate AI summary of old messages, keep only summary |
| **Truncate** | Keep only recent N messages, discard rest |
| **Hybrid** | Generate summary + keep recent N messages |

**Trigger conditions** are configurable per role: auto-compaction toggle, message count threshold (default 100), number of recent messages to always keep (default 20), and token count threshold (default ~100k tokens).

### MemoryConsolidationService

Provides structured memory management through **key decision extraction** — parsing conversation into structured records with type (design decision, code change, unresolved issue, or lesson learned), content description, related files, and timestamp. Also generates **session exit summaries** that save to role memory for cross-session continuity.

### Token Tracker

Records per-model and aggregate token usage (input, output, cache read/write), API timing and retry counts, cost estimation with configurable per-model pricing, tool invocation metrics, web search requests, and code change statistics.

---

## Permission Management

Three-level permission system with rule matching ordered as: Deny rules → Ask rules → Allow rules. If a rule matches, its action is applied; unmatched rules fall back to the default mode.

**Rule patterns** use glob matching on tool names (e.g., `bash`, `read_*`, `*`), command patterns for the bash tool (e.g., `git *`, `npm install *`), and path patterns for file tools.

**Denial tracking**: After 3 denials of the same tool, the system auto-allows to reduce noise.

---

## Skill System

Skills are defined via `SKILL.md` files with YAML frontmatter, supporting environment gating (required binaries, environment variables, OS restrictions), multiple install methods (node, go, uv, download, apt, brew), slash commands, and multi-directory loading with dedup.

---

## LSP Integration

The LSP manager provides Language Server Protocol integration: go to definition, find references, hover information, document and workspace symbols, diagnostics, code formatting, and LSP server lifecycle management.

---

## Cron Scheduler

Built-in cron scheduler with standard 5-field cron expressions, recurring and one-shot tasks, durable persistence to `.claude/scheduled_tasks.json`, and task pause/resume/delete/run-now operations.

---

## Slash Commands

| Command | Function | Command | Function |
|---------|----------|---------|----------|
| `/help` | Show help | `/clear` | Clear history |
| `/plan` | Plan mode | `/compact` | Compress context |
| `/model` | Switch model | `/provider` | Switch LLM provider |
| `/role` | Switch role | `/roles` | List roles |
| `/session` | Session management | `/sessions` | List sessions |
| `/server` | Local model server | `/setup` | Auto-configure local model |
| `/mcp` | MCP servers | `/skill` | Skill management |
| `/cost` | Cost statistics | `/status` | System status |
| `/diff` | Show git diff | `/commit` | Git commit |
| `/tasks` | Task management | `/config` | Edit config |
| `/context` | Show context | `/doctor` | System diagnostics |
| `/effort` | Set reasoning effort | `/autocommit` | Auto-commit toggle |
| `/memory` | Memory management | `/summary` | Session summary |
| `/permissions` | Permission settings | `/history` | Session history |
| `/token` | Token usage | `/bye` | Exit |

---

## Quick Start

### Install

**macOS / Linux** (one command):
```
curl -fsSL https://aicodingbox.com/install.sh | bash
```

**Windows** (PowerShell):
```
irm https://aicodingbox.com/install.ps1 | iex
```

Or via package manager:

| Platform | Command |
|----------|---------|
| **Homebrew** (macOS/Linux) | `brew install Swair/tap/prosophor` |
| **Scoop** (Windows) | `scoop install prosophor` |
| **WinGet** (Windows) | `winget install Swair.prosophor` |

### Run

```
prosophor
```

First launch auto-generates **`~/.prosophor/settings.json`**. Edit it to add your API keys and configure providers.

### Quick Configuration

At minimum, set up at least one provider with your API key and model. The config file uses a JSON format with provider entries as arrays — each entry has its own api_key and base_url, and contains named agent configs specifying model, temperature, and other parameters.

---

## Build from Source

### Requirements

| Component | Requirement |
|-----------|-------------|
| Compiler | C++17 or later (GCC 9+, Clang 12+, MSVC 2019+) |
| CMake | 3.20+ |
| Dependencies | spdlog, nlohmann/json, libcurl, OpenSSL (auto-fetched) |

### Build

```
git clone https://github.com/Swair/prosophor.git
cd prosophor
make build
```

Or manually:

```
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) && make install
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PROSOPHOR_BUILD_LLAMA` | ON | Build llama.cpp for local GGUF inference |
| `PROSOPHOR_SDL_UI` | OFF | Build SDL graphical UI (requires SDL3 dev libs) |

Skip llama.cpp build:
```
cmake .. -DPROSOPHOR_BUILD_LLAMA=OFF
```

### Build Variants

| Target | Description |
|--------|-------------|
| `prosophor_core` | Static library — pure business logic, no UI deps |
| `prosophor` (TUI) | Core + terminal UI |
| `prosophor` (SDL) | Core + SDL/ImGui graphical UI |

### Tests

```
make test
```

Or:
```
cd build && ctest --output-on-failure
```

---

## Project Structure

```
├── CMakeLists.txt              # Top-level CMake build
├── Makefile                    # Convenience build targets
├── config/
│   └── .prosophor/             # Configuration directory
│       ├── settings.json       # Main configuration
│       ├── roles/              # Agent role definitions
│       ├── active/             # Proactive trigger plugins
│       └── workspace/          # Workspace config
├── main_src/
│   ├── prosophor_core/         # Static library (zero UI deps)
│   │   ├── agent_engine.cc/h   # Central orchestrator
│   │   ├── command_registry.cc/h  # Slash command system
│   │   ├── core/               # AgentCore, REACT loop, compaction, memory
│   │   ├── config/             # Configuration system
│   │   ├── managers/           # Session, memory, plugin, permission, local model mgmt
│   │   ├── mcp/                # MCP protocol client
│   │   ├── network/            # HTTP client, local model utils
│   │   ├── providers/          # LLM providers (Anthropic, OpenAI, Ollama)
│   │   ├── services/           # LSP, cron, TTS
│   │   └── tools/              # Tool implementations
│   ├── ai_coding/              # Terminal UI frontend
│   ├── common/                 # Utilities (file, string, time, logging)
│   ├── platform/               # Cross-platform abstraction
│   ├── media_engine/           # SDL/ImGui rendering engine
│   ├── scene/                  # SDL scenes (home, galgame, character)
│   ├── virtual_sprite/         # SDL frontend entry point
│   └── components/             # SDL UI components
├── tests/                      # GoogleTest-based tests
├── scripts/                    # llama-server start/stop scripts
├── assets/                     # Game assets (images, audio)
└── docs/                       # Documentation
```

---

## License

Apache-2.0 · [LICENSE](./LICENSE)

---

<div align="center">

**Made with C++** · If this helps, give us a ⭐️ Star!

</div>
