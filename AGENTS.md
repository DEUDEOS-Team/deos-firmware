# AI Agent Instructions for DEOS Firmware

These rules apply to ALL AI assistants, coders, and agents (Gemini, Claude, DeepSeek, Cursor, Copilot, etc.) working on this repository.

## 1. Architectural Boundaries & Permissions

- **Read-Only Access to Core Library**: AI agents are strictly prohibited from modifying the `deos_core` library located in the `lib/` directory. You are only allowed to read it to understand the underlying infrastructure.
- **Approval Required for Core Changes**: Any modifications to the `deos_core` library must be explicitly asked to and approved by Emir Arkalı. Unauthorized changes will break the wire protocol and cause communication failures with other CAN nodes.
- **Reporting Necessary Core Changes**: If a modification in the `lib/` directory is absolutely necessary for an application to function properly, the AI agent must NOT apply the changes directly. Instead, the AI agent must report the situation to the user and open a formal GitHub Issue using the GitHub CLI (`gh issue create --title "[Core] ..." --body "..." --assignee "emirarkali" --label "bug"`), requesting Emir Arkalı's explicit review and approval.
- **Scope of Permissions**: In general, developers and AI agents are only authorized to create and modify applications within the `apps/` directory (e.g., `apps/traction`, `apps/steering`). They do not have the authority to edit the core library without explicit permission.
- **Language Choice**: Even though the core library is written in C, the application layer within the `apps/` directory can be written in C++.

## 2. Zephyr RTOS & Build Rules

- **Build Commands**: Do not run a generic `west build` at the root of the project. Each application is built independently. Always specify the board and application directory:
  `west build -b <board_name> apps/<app_name>` (e.g., `west build -b native_sim apps/traction`)
- **Logging Standards**: Do NOT use standard C `printf()`. Always use Zephyr's logging subsystem (`<zephyr/logging/log.h>`) with `LOG_INF()`, `LOG_ERR()`, `LOG_WRN()`, etc. Ensure `LOG_MODULE_REGISTER()` is declared at the top of the C files.
- **Include Paths**: When an application needs to include DEOS core headers, it must use the system include format `<deos/deos.h>` or `<deos/deos_icd.h>`. Never use relative paths (like `../../lib/deos_core/...`) to access the library headers.

## 3. Git Workflow

- **Syncing Changes**: AI agents must always execute a `git pull` before starting any task, and a `git push` after the work is finished to keep the repository synchronized.

## 4. Architectural Knowledge Base
- **MANDATORY READING**: Before modifying network routing, LoRa/UART code, or adding new test scripts, you MUST read the `docs/DEOS_ARCHITECTURE_KNOWLEDGE.md` file. It contains the history of zero-latency optimizations, Split Horizon routing rules, and node ID mapping (e.g. GROUND_CONTROL vs DIAG_TOOL) that we spent hours debugging. Do not act blindly.
