# Architecture notes

This prototype keeps the future agent outside the trusted task logic.

```text
taskboard.exe
    UI, confirmation, calendar rendering
          |
          | versioned AgentProposal (future named pipe)
          v
taskagent_stub.exe
    future local-model adapter; currently inert

task_model.c
    validation and recurrence; no HWND or UI ownership

ui_agent_panel.c
    optional UI section; owns its controls, visibility, layout, and mock drafting
```

The main window decides which sections compose each page. The Tasks page uses
the full content region, Calendar pairs its grid with a monthly agenda, and New
Task pairs its form with the optional `AgentPanel`. Removing or disabling that
panel later does not require changing the task form's controls.

## Intended next boundaries

1. Add a `taskservice.exe` that is the sole owner of persistent data.
2. Move proposals over a size-limited, length-prefixed named-pipe protocol.
3. Let the agent request read-only lookups and return proposals only.
4. Validate and preview every proposal in deterministic C.
5. Require the UI to confirm mutations before sending them to the service.

`AgentProposal` is intentionally not a complete wire representation yet. Its
presence makes the process boundary visible without prematurely committing the
project to JSON, a binary schema, or a particular inference engine.
