#ifndef UI_AGENT_PANEL_H
#define UI_AGENT_PANEL_H

#include <windows.h>

typedef struct AgentPanel {
    HWND title;
    HWND hint;
    HWND input;
    HWND draft_button;
    HWND output;
} AgentPanel;

int agent_panel_create(AgentPanel *panel, HWND parent, HFONT font, HFONT bold_font,
                       int input_id, int button_id, int output_id);
void agent_panel_show(AgentPanel *panel, int visible);
void agent_panel_layout(AgentPanel *panel, int x, int y, int width, int height, UINT dpi);
void agent_panel_draft(AgentPanel *panel);

#endif
