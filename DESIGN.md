# Taskboard visual system

The interface is organized from large, stable regions down to individual
controls. This keeps a new feature from inventing its own margins and visual
language.

## Hierarchy

```text
Application shell
├── Navigation rail (global destinations)
└── Workspace
    ├── Page header (title and one-line purpose)
    └── Content row
        ├── Primary card (the page's main job)
        └── Optional support card (context or assistance)
```

The page chooses the composition:

- Calendar: calendar card + monthly-agenda card.
- Tasks: one full-width task-list card.
- New task: form card + optional agent card.

The support card is not a permanent sidebar. It is a replaceable page section.

## Spacing

Logical measurements are scaled from 96 DPI. Prefer this small scale:

| Purpose | Logical pixels |
|---|---:|
| Tight relationship | 4 or 8 |
| Related controls | 12 or 16 |
| Card padding | 20 |
| Region gap / outer margin | 20 or 24 |
| Major separation | 32 or 48 |

Avoid introducing arbitrary values when an existing spacing value expresses
the relationship correctly.

## Color roles

Colors live in `UiTheme`; page code refers to their roles, not copied RGB
values:

- `background`: workspace canvas.
- `navigation`: global navigation surface.
- `panel`: primary and support cards.
- `border`: quiet structural separation.
- `text`: primary information.
- `muted_text`: descriptions and status copy.
- `accent`: selection and important action identity.
- `accent_soft`: selected/today background.

Category colors remain task data. They should not be reused for application
navigation because that would weaken their meaning.

The current palette is deliberately dark. White is the structural accent for
navigation selection, focus, and today's date; it is not used as a task
category color. Input surfaces and weekend cells use separate near-black roles
so hierarchy remains visible without relying on bright borders.

List headers are painted by subclasses of the actual header child windows;
their notifications do not reliably reach the main window through the list
view. Task filters use three owner-drawn buttons instead of a native tab
control, avoiding the system-painted tab canvas while preserving tab-like
filter behavior.

Task shape/color icons are drawn directly into their list cells. Calendar
markers, the task table, and the monthly agenda all use the same drawing
function and derive their identity from the same `Task.shape` and
`Task.color_rgb` fields. Direct drawing avoids bitmap alpha, color-key, and
mask conversion differences in Win32 image lists.

## Component ownership

`UiTheme` owns shared colors and brushes. `AgentPanel` owns all agent controls,
visibility, and internal layout. The main window owns page composition and the
rectangles assigned to each component. The calendar owns its grid drawing.

When adding another optional section, follow the `AgentPanel` shape: one state
structure with create, show, layout, and action functions. The page should not
position the component's individual child controls.
