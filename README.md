# Taskboard

Taskboard is a native Windows task organizer prototype written in C with the Win32 API. It brings a monthly calendar, a dated agenda, a filterable task list, and a task entry form into one dark-themed desktop interface.

<img width="1548" height="951" alt="image" src="https://github.com/user-attachments/assets/62ba89ce-8d42-4527-9106-41c8a1e6f7e9" />

<img width="1540" height="953" alt="image" src="https://github.com/user-attachments/assets/013df7b1-4ece-405a-b903-d9623b6575f9" />

<img width="1539" height="961" alt="image" src="https://github.com/user-attachments/assets/a3827217-e605-48e1-aae9-04f86ce1994c" />


The project explores a small, maintainable foundation for a larger organizer: task rules live outside the UI, visual elements share a theme, and an optional assistant panel has its own component boundary. The assistant is currently a **mock demonstration**, intended for future AI integration.

## What works

- **Calendar and agenda:** Browse months, return to today, and see task markers on scheduled dates alongside a dated list for the selected month.
- **Task list:** View all tasks or filter to one-time and recurring tasks. Task shape and color markers stay consistent across the list, calendar, and agenda.
- **Task entry:** Add a title, category, start date, optional due date, and a daily, weekly, monthly, or yearly repeat choice. The form checks date input, and the task model validates the task before adding it.
- **Windows UI:** Custom-drawn navigation, controls, list icons, and calendar cells use shared color roles and measurements scaled for display DPI. The manifest enables modern common controls and per-monitor DPI awareness.
- **Assistant concept:** The **Ask Agent** panel previews a mock task proposal without saving or executing anything. A separate `taskagent_stub.exe` illustrates the intended process boundary.

Tasks are held in memory for the current session. Three sample tasks are loaded at startup so the calendar and list have something to display.

## Build and run

On Windows, install Visual Studio or Visual Studio Build Tools with the **Desktop development with C++** workload and a Windows SDK. From Command Prompt or PowerShell in the project directory, run:

```bat
build_windows.bat
```

The script uses an existing `cl.exe` environment when available. Otherwise, it finds the newest suitable Visual Studio installation with `vswhere.exe` and loads its x64 build environment for the script. It compiles the application and resources into:

```text
build\taskboard.exe
build\taskagent_stub.exe
```

Run `build\taskboard.exe` to open the organizer. The build uses the static multithreaded C runtime (`/MT`) and Windows SDK libraries; Windows system DLLs are still required at runtime. No third-party application framework is used.

## How it is organized

| Path | Responsibility |
| --- | --- |
| `src/taskboard.c` | Main window, page navigation, calendar, task list, form, and Win32 event handling |
| `src/task_model.*` | Task data, validation, date helpers, and recurrence calculations, independent of the UI |
| `src/ui_theme.*` | Shared colors, brushes, scaling helper, and card drawing |
| `src/ui_agent_panel.*` | Controls and layout for the optional assistant panel |
| `src/agent_protocol.*` | Versioned proposal structure and mock proposal text |
| `src/taskagent_stub.c` | Standalone placeholder for a future agent process |
| `resources/` | Windows application manifest and resource definition |

See [DESIGN.md](DESIGN.md) for the interface rules and [ARCHITECTURE.md](ARCHITECTURE.md) for the intended component and process boundaries.

## Current scope

This is a prototype, with a few deliberate limits:

- Tasks are not persisted; they disappear when the application closes. The in-memory store holds up to 128 tasks.
- Due dates are validated and shown in the task list, but the calendar and agenda place one-time tasks on their **start** dates. Due dates are not separate calendar events.
- The UI has no task editing, deletion, or completion workflow yet.
- The assistant panel returns fixed mock text. It does not interpret requests, connect to a model, communicate with another process, or change tasks.

Likely next steps are persistence, focused tests for recurrence and date edge cases, and completion/editing workflows. The proposed agent service and named-pipe protocol in [ARCHITECTURE.md](ARCHITECTURE.md) are future design ideas, not implemented features.
