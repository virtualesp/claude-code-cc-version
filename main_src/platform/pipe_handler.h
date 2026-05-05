// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

// Pipe and process operations for LSP/MCP server communication

#ifndef PROSOPHOR_PLATFORM_PIPE_HANDLER_H
#define PROSOPHOR_PLATFORM_PIPE_HANDLER_H

#include <string>
#include <vector>

namespace prosophor {
namespace platform {

// Pipe operations for subprocess stdin/stdout communication
struct PipePair {
    int read_fd = -1;
    int write_fd = -1;
};

// Create a pipe pair for bidirectional communication
PipePair CreatePipe();

// Close a pipe file descriptor
bool ClosePipe(int fd);

// Read from pipe
int ReadPipe(int fd, char* buf, size_t size);

// Write to pipe
int WritePipe(int fd, const char* buf, size_t size);

// Duplicate file descriptor (for redirecting stdin/stdout)
bool Dup2Pipe(int old_fd, int new_fd);

// Process creation for spawning LSP/MCP servers
struct ForkedProcess {
    int pid = -1;
    int stdin_fd = -1;
    int stdout_fd = -1;
};

// Fork and exec a process, returning pipe handles for communication
ForkedProcess ForkAndExec(const std::string& command,
                          const std::vector<std::string>& args,
                          const std::string& workdir = "",
                          const std::vector<std::string>& env = {});

// Wait for process to complete
bool WaitProcess(int pid);

// Get current process ID
int GetCurrentPid();

}  // namespace platform
}  // namespace prosophor

#endif  // PROSOPHOR_PLATFORM_PIPE_HANDLER_H