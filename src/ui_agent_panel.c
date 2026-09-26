#include "ui_agent_panel.h"

#include "agent_protocol.h"

static int scaled(UINT dpi, int value)
{
    return MulDiv(value, (int)dpi, 96);
}

static HWND make_control(HWND parent, HFONT font, const wchar_t *klass,
                         const wchar_t *text, DWORD style, DWORD ex_style, int id)
{
    HWND control = CreateWindowExW(ex_style, klass, text, WS_CHILD | style,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (control) SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
    return control;
}

int agent_panel_create(AgentPanel *panel, HWND parent, HFONT font, HFONT bold_font,
                       int input_id, int button_id, int output_id)
{
    panel->title = make_control(parent, bold_font, L"STATIC", L"Ask Agent",
                                SS_LEFT, 0, 0);
    panel->hint = make_control(parent, font, L"STATIC",
        L"Describe a task naturally", SS_LEFT, 0, 0);
    panel->input = make_control(parent, font, L"EDIT", L"Clean the bathroom every month",
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, input_id);
    panel->draft_button = make_control(parent, font, L"BUTTON", L"Draft task (mock)",
        BS_OWNERDRAW | WS_TABSTOP, 0, button_id);
    panel->output = make_control(parent, font, L"EDIT",
        L"Agent process: not connected\r\n\r\nNo operation will be executed.",
        ES_MULTILINE | ES_READONLY | WS_VSCROLL, WS_EX_CLIENTEDGE, output_id);
    return panel->title && panel->hint && panel->input && panel->draft_button && panel->output;
}

void agent_panel_show(AgentPanel *panel, int visible)
{
    int command = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(panel->title, command);
    ShowWindow(panel->hint, command);
    ShowWindow(panel->input, command);
    ShowWindow(panel->draft_button, command);
    ShowWindow(panel->output, command);
}

void agent_panel_layout(AgentPanel *panel, int x, int y, int width, int height, UINT dpi)
{
    int pad = scaled(dpi, 20);
    int inner_x = x + pad;
    int inner_w = width - pad * 2;
    MoveWindow(panel->title, inner_x, y + pad, inner_w, scaled(dpi, 26), TRUE);
    MoveWindow(panel->hint, inner_x, y + scaled(dpi, 48), inner_w, scaled(dpi, 52), TRUE);
    MoveWindow(panel->input, inner_x, y + scaled(dpi, 108), inner_w, scaled(dpi, 88), TRUE);
    MoveWindow(panel->draft_button, inner_x, y + scaled(dpi, 208), inner_w, scaled(dpi, 38), TRUE);
    MoveWindow(panel->output, inner_x, y + scaled(dpi, 266), inner_w,
               height - scaled(dpi, 286), TRUE);
}

void agent_panel_draft(AgentPanel *panel)
{
    wchar_t request[AGENT_TEXT_CAP];
    AgentProposal proposal;
    GetWindowTextW(panel->input, request, AGENT_TEXT_CAP);
    agent_make_demo_proposal(request, &proposal);
    SetWindowTextW(panel->output, proposal.preview);
}
