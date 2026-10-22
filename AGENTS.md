# AI Agent Instructions for DEOS Firmware

These rules apply to ALL AI assistants, coders, and agents (Gemini, Claude, DeepSeek, Cursor, Copilot, etc.) working on this repository.

## 0. Mandatory AI Context Workflow

- **Read Context First**: Every agent MUST read the root-level `AI_CONTEXT.md` completely before scanning the repository or making changes. Use its `TARGETED_READ_MAP` to open only the source files relevant to the task. Do not begin with a full-codebase scan unless the context is missing, inconsistent, or the user explicitly requests a repository-wide audit.
- **Source Remains Authoritative**: `AI_CONTEXT.md` is a fast operational index, not a replacement for source verification. For any value or behavior being changed, verify the targeted canonical source paths listed in the context. If source and context disagree, follow the source and correct the context in the same change.
- **Keep Context Current**: Any change that affects architecture, node IDs or ownership, services/commands, payloads, routing, transports, concurrency, application responsibilities, public APIs, build commands, boards, or repository layout MUST update the corresponding section of `AI_CONTEXT.md` in the same commit.
- **Context Maintenance Is Part of Completion**: Before finishing, apply the `CONTEXT_MAINTENANCE_PROTOCOL` in `AI_CONTEXT.md`. A code change is incomplete if it leaves represented context stale. Do not append session notes or conversational history; update the current facts in place.
- **New Node Work**: When creating a node application, start from `NEW_NODE_RECIPE` and `APPLICATION_SNAPSHOTS` in `AI_CONTEXT.md`, then inspect only the referenced endpoint template and exact public headers/ICD definitions needed by that node.

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
