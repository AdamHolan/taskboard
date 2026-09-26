#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
#include <wchar.h>

#include "task_model.h"
#include "agent_protocol.h"
#include "ui_agent_panel.h"
#include "ui_theme.h"

#pragma comment(lib, "comctl32.lib")

#define APP_CLASS L"TaskboardMainWindow"
#define CAL_CLASS L"TaskboardCalendar"

enum {
    ID_NAV_CALENDAR = 100,
    ID_NAV_TASKS,
    ID_NAV_NEW,
    ID_CAL_PREV,
    ID_CAL_TODAY,
    ID_CAL_NEXT,
    ID_FILTER_ALL,
    ID_FILTER_PROGRESS,
    ID_FILTER_RECURRING,
    ID_TASK_LIST,
    ID_TITLE,
    ID_CATEGORY,
    ID_START_DATE,
    ID_DUE_DATE,
    ID_REPEAT,
    ID_SAVE_TASK,
    ID_AGENT_INPUT,
    ID_AGENT_DRAFT,
    ID_AGENT_OUTPUT
};

typedef enum Page { PAGE_CALENDAR, PAGE_TASKS, PAGE_NEW } Page;

typedef struct App {
    HWND window;
    HWND nav_calendar, nav_tasks, nav_new;
    HWND page_title, page_hint;
    HWND cal_prev, cal_today, cal_next, calendar;
    HWND agenda_title, agenda_hint, agenda_list;
    HWND filter_all, filter_progress, filter_recurring, task_list;
    HWND label_title, edit_title, label_category, combo_category;
    HWND label_start, edit_start, label_due, edit_due;
    HWND label_repeat, combo_repeat, save_task, form_status;
    AgentPanel agent_panel;
    HFONT font, font_bold, font_title;
    UiTheme theme;
    RECT primary_card, support_card;
    TaskStore store;
    Page page;
    int task_filter;
    int calendar_year, calendar_month;
    UINT dpi;
} App;

static void layout(App *app);
static void draw_shape(HDC dc, int shape, int x, int y, int size, COLORREF color);

static int scale_for(UINT dpi, int value) { return ui_scale(dpi, value); }

static void enable_dark_title_bar(HWND window)
{
    typedef HRESULT (WINAPI *DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    BOOL enabled = TRUE;
    if (dwm) {
        DwmSetWindowAttributeFn set_attribute =
            (DwmSetWindowAttributeFn)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (set_attribute) {
            if (FAILED(set_attribute(window, 20, &enabled, sizeof(enabled))))
                set_attribute(window, 19, &enabled, sizeof(enabled));
        }
        FreeLibrary(dwm);
    }
}

static void use_dark_control_theme(HWND control_window)
{
    if (control_window) SetWindowTheme(control_window, L"DarkMode_Explorer", NULL);
}

static UINT window_dpi(HWND window)
{
    typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    GetDpiForWindowFn fn = (GetDpiForWindowFn)(void *)GetProcAddress(user32, "GetDpiForWindow");
    return fn ? fn(window) : 96;
}

static HFONT make_font(UINT dpi, int points, int weight)
{
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -MulDiv(points, (int)dpi, 72);
    lf.lfWeight = weight;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(lf.lfFaceName, L"Segoe UI");
    return CreateFontIndirectW(&lf);
}

static HWND control(App *app, const wchar_t *klass, const wchar_t *text, DWORD style,
                    DWORD ex_style, int id)
{
    HWND hwnd = CreateWindowExW(ex_style, klass, text, WS_CHILD | style, 0, 0, 0, 0,
                                app->window, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)app->font, TRUE);
    return hwnd;
}

static void set_visible(HWND hwnd, int visible)
{
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

static int parse_date(HWND edit, Date *date)
{
    wchar_t text[32];
    int y, m, d;
    GetWindowTextW(edit, text, 32);
    if (swscanf(text, L"%d-%d-%d", &y, &m, &d) != 3) return 0;
    *date = (Date){ y, m, d };
    return date_is_valid(*date);
}

static void populate_task_list(App *app)
{
    size_t i;
    int row = 0;
    int filter = app->task_filter;
    wchar_t schedule[96];
    ListView_DeleteAllItems(app->task_list);
    for (i = 0; i < app->store.count; ++i) {
        Task *task = &app->store.items[i];
        LVITEMW item;
        if (filter == 1 && task->kind != TASK_FINITE) continue;
        if (filter == 2 && task->kind != TASK_RECURRING) continue;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.pszText = task->title;
        item.lParam = task->id;
        ListView_InsertItem(app->task_list, &item);
        ListView_SetItemText(app->task_list, row, 1, task->category);
        if (task->repeat != REPEAT_NONE) {
            _snwprintf(schedule, 95, L"%ls, every %u", repeat_name(task->repeat), task->repeat_interval);
        } else if (date_is_valid(task->due)) {
            _snwprintf(schedule, 95, L"Due %04d-%02d-%02d", task->due.year, task->due.month, task->due.day);
        } else {
            _snwprintf(schedule, 95, L"%04d-%02d-%02d", task->start.year, task->start.month, task->start.day);
        }
        schedule[95] = L'\0';
        ListView_SetItemText(app->task_list, row, 2, schedule);
        ListView_SetItemText(app->task_list, row, 3,
            task->kind == TASK_RECURRING ? L"Recurring" : L"In progress");
        ++row;
    }
}

static void populate_month_agenda(App *app)
{
    int day;
    int row = 0;
    wchar_t date_text[32];
    wchar_t heading[64];
    _snwprintf(heading, 63, L"Tasks in %04d-%02d", app->calendar_year, app->calendar_month);
    heading[63] = L'\0';
    SetWindowTextW(app->agenda_title, heading);
    ListView_DeleteAllItems(app->agenda_list);
    for (day = 1; day <= days_in_month(app->calendar_year, app->calendar_month); ++day) {
        size_t i;
        Date date = { app->calendar_year, app->calendar_month, day };
        for (i = 0; i < app->store.count; ++i) {
            Task *task = &app->store.items[i];
            LVITEMW item;
            if (!task_occurs_on(task, date)) continue;
            _snwprintf(date_text, 31, L"%04d-%02d-%02d", date.year, date.month, date.day);
            date_text[31] = L'\0';
            ZeroMemory(&item, sizeof(item));
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = row;
            item.pszText = date_text;
            item.lParam = task->id;
            ListView_InsertItem(app->agenda_list, &item);
            ListView_SetItemText(app->agenda_list, row, 1, task->title);
            ++row;
        }
    }
}

static void set_agenda_panel_visible(App *app, int visible)
{
    set_visible(app->agenda_title, visible);
    set_visible(app->agenda_hint, visible);
    set_visible(app->agenda_list, visible);
}

static void show_page(App *app, Page page)
{
    int cal = page == PAGE_CALENDAR;
    int tasks = page == PAGE_TASKS;
    int form = page == PAGE_NEW;
    app->page = page;

    set_visible(app->cal_prev, cal); set_visible(app->cal_today, cal);
    set_visible(app->cal_next, cal); set_visible(app->calendar, cal);
    set_visible(app->filter_all, tasks); set_visible(app->filter_progress, tasks);
    set_visible(app->filter_recurring, tasks); set_visible(app->task_list, tasks);
    set_visible(app->label_title, form); set_visible(app->edit_title, form);
    set_visible(app->label_category, form); set_visible(app->combo_category, form);
    set_visible(app->label_start, form); set_visible(app->edit_start, form);
    set_visible(app->label_due, form); set_visible(app->edit_due, form);
    set_visible(app->label_repeat, form); set_visible(app->combo_repeat, form);
    set_visible(app->save_task, form); set_visible(app->form_status, form);
    agent_panel_show(&app->agent_panel, form);
    set_agenda_panel_visible(app, cal);

    if (cal) {
        SetWindowTextW(app->page_title, L"Calendar");
        SetWindowTextW(app->page_hint, L"A monthly view of your tasks");
        populate_month_agenda(app);
    } else if (tasks) {
        SetWindowTextW(app->page_title, L"Tasks");
        SetWindowTextW(app->page_hint, L"See in progress and recurring tasks");
        populate_task_list(app);
    } else {
        SetWindowTextW(app->page_title, L"New task");
        SetWindowTextW(app->page_hint, L"Create a new task manually or describe it");
        SetFocus(app->edit_title);
    }
    layout(app);
    InvalidateRect(app->nav_calendar, NULL, TRUE);
    InvalidateRect(app->nav_tasks, NULL, TRUE);
    InvalidateRect(app->nav_new, NULL, TRUE);
    InvalidateRect(app->window, NULL, TRUE);
}

static void layout(App *app)
{
    RECT rc;
    int outer = scale_for(app->dpi, 24);
    int inner = scale_for(app->dpi, 20);
    int gap = scale_for(app->dpi, 20);
    int nav_w = scale_for(app->dpi, 196);
    int side_w = scale_for(app->dpi, 326);
    int nav_x = scale_for(app->dpi, 18);
    int main_x = nav_w + scale_for(app->dpi, 28);
    int button_h = scale_for(app->dpi, 42);
    int content_top = scale_for(app->dpi, 112);
    int full_w, main_w, side_x, card_h;
    GetClientRect(app->window, &rc);
    full_w = rc.right - main_x - outer;
    main_w = full_w;
    side_x = rc.right - side_w - outer;
    if (app->page == PAGE_CALENDAR || app->page == PAGE_NEW)
        main_w = side_x - main_x - gap;
    card_h = rc.bottom - content_top - outer;

    app->primary_card = (RECT){ main_x, content_top, main_x + main_w, content_top + card_h };
    if (app->page == PAGE_CALENDAR || app->page == PAGE_NEW)
        app->support_card = (RECT){ side_x, content_top, side_x + side_w, content_top + card_h };
    else
        SetRectEmpty(&app->support_card);

    MoveWindow(app->nav_calendar, nav_x, scale_for(app->dpi, 94), nav_w - scale_for(app->dpi, 36), button_h, TRUE);
    MoveWindow(app->nav_tasks, nav_x, scale_for(app->dpi, 142), nav_w - scale_for(app->dpi, 36), button_h, TRUE);
    MoveWindow(app->nav_new, nav_x, scale_for(app->dpi, 202), nav_w - scale_for(app->dpi, 36), button_h, TRUE);
    MoveWindow(app->page_title, main_x, scale_for(app->dpi, 24), full_w, scale_for(app->dpi, 38), TRUE);
    MoveWindow(app->page_hint, main_x, scale_for(app->dpi, 65), full_w, scale_for(app->dpi, 24), TRUE);

    MoveWindow(app->cal_prev, main_x + inner, content_top + inner, scale_for(app->dpi, 38), scale_for(app->dpi, 34), TRUE);
    MoveWindow(app->cal_today, main_x + inner + scale_for(app->dpi, 46), content_top + inner,
               scale_for(app->dpi, 72), scale_for(app->dpi, 34), TRUE);
    MoveWindow(app->cal_next, main_x + inner + scale_for(app->dpi, 126), content_top + inner,
               scale_for(app->dpi, 38), scale_for(app->dpi, 34), TRUE);
    MoveWindow(app->calendar, main_x + inner, content_top + scale_for(app->dpi, 66), main_w - inner * 2,
               card_h - scale_for(app->dpi, 86), TRUE);

    MoveWindow(app->agenda_title, side_x + inner, content_top + inner, side_w - inner * 2, scale_for(app->dpi, 28), TRUE);
    MoveWindow(app->agenda_hint, side_x + inner, content_top + scale_for(app->dpi, 48),
               side_w - inner * 2, scale_for(app->dpi, 42), TRUE);
    MoveWindow(app->agenda_list, side_x + inner, content_top + scale_for(app->dpi, 94),
               side_w - inner * 2, card_h - scale_for(app->dpi, 114), TRUE);

    MoveWindow(app->filter_all, main_x + inner, content_top + inner,
               scale_for(app->dpi, 92), scale_for(app->dpi, 34), TRUE);
    MoveWindow(app->filter_progress, main_x + inner + scale_for(app->dpi, 100), content_top + inner,
               scale_for(app->dpi, 104), scale_for(app->dpi, 34), TRUE);
    MoveWindow(app->filter_recurring, main_x + inner + scale_for(app->dpi, 212), content_top + inner,
               scale_for(app->dpi, 100), scale_for(app->dpi, 34), TRUE);
    MoveWindow(app->task_list, main_x + inner, content_top + scale_for(app->dpi, 66),
               full_w - inner * 2, card_h - scale_for(app->dpi, 86), TRUE);
    ListView_SetColumnWidth(app->task_list, 0,
        full_w - inner * 2 - scale_for(app->dpi, 150 + 160 + 100 + 58 + 24));
    ListView_SetColumnWidth(app->agenda_list, 1,
        side_w - inner * 2 - scale_for(app->dpi, 92 + 48 + 22));

    {
        int label_w = scale_for(app->dpi, 112);
        int form_x = main_x + inner;
        int field_x = form_x + label_w;
        int field_w = main_w - inner * 2 - label_w;
        int row = content_top + inner;
        HWND labels[] = { app->label_title, app->label_category, app->label_start,
                          app->label_due, app->label_repeat };
        HWND fields[] = { app->edit_title, app->combo_category, app->edit_start,
                          app->edit_due, app->combo_repeat };
        int i;
        for (i = 0; i < 5; ++i) {
            MoveWindow(labels[i], form_x, row + scale_for(app->dpi, 7), label_w, scale_for(app->dpi, 24), TRUE);
            MoveWindow(fields[i], field_x, row, field_w, scale_for(app->dpi, i == 1 || i == 4 ? 160 : 32), TRUE);
            row += scale_for(app->dpi, 52);
        }
        MoveWindow(app->save_task, field_x, row + scale_for(app->dpi, 6), scale_for(app->dpi, 126), scale_for(app->dpi, 38), TRUE);
        MoveWindow(app->form_status, field_x, row + scale_for(app->dpi, 54), field_w, scale_for(app->dpi, 44), TRUE);
    }

    agent_panel_layout(&app->agent_panel, side_x, content_top, side_w, card_h, app->dpi);
}

static void draw_shape(HDC dc, int shape, int x, int y, int size, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH old_brush = SelectObject(dc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(28, 31, 34));
    HPEN old_pen = SelectObject(dc, pen);
    if (shape % 3 == 0) Ellipse(dc, x, y, x + size, y + size);
    else if (shape % 3 == 1) Rectangle(dc, x, y, x + size, y + size);
    else {
        POINT pts[3] = { { x + size / 2, y }, { x + size, y + size }, { x, y + size } };
        Polygon(dc, pts, 3);
    }
    SelectObject(dc, old_pen); SelectObject(dc, old_brush);
    DeleteObject(pen); DeleteObject(brush);
}

static const Task *find_task_by_id(const App *app, uint32_t id)
{
    size_t i;
    for (i = 0; i < app->store.count; ++i) {
        if (app->store.items[i].id == id) return &app->store.items[i];
    }
    return NULL;
}

static LRESULT draw_list_icon_cell(App *app, NMLVCUSTOMDRAW *custom, int icon_column)
{
    DWORD stage = custom->nmcd.dwDrawStage;
    if (stage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (stage == CDDS_ITEMPREPAINT) {
        int row = (int)custom->nmcd.dwItemSpec;
        int selected = (ListView_GetItemState(custom->nmcd.hdr.hwndFrom, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
        custom->clrText = app->theme.text;
        custom->clrTextBk = selected ? app->theme.accent_soft : app->theme.panel;
        return CDRF_NOTIFYSUBITEMDRAW;
    }
    if (stage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM) && custom->iSubItem == icon_column) {
        HWND list = custom->nmcd.hdr.hwndFrom;
        int row = (int)custom->nmcd.dwItemSpec;
        RECT cell;
        int size = scale_for(app->dpi, 14);
        const Task *task = find_task_by_id(app, (uint32_t)custom->nmcd.lItemlParam);
        HBRUSH background;
        COLORREF color;
        if (!task || !ListView_GetSubItemRect(list, row, icon_column, LVIR_BOUNDS, &cell))
            return CDRF_DODEFAULT;
        background = (ListView_GetItemState(list, row, LVIS_SELECTED) & LVIS_SELECTED)
            ? app->theme.accent_soft_brush : app->theme.panel_brush;
        FillRect(custom->nmcd.hdc, &cell, background);
        color = RGB((task->color_rgb >> 16) & 255,
                    (task->color_rgb >> 8) & 255,
                    task->color_rgb & 255);
        draw_shape(custom->nmcd.hdc, (int)task->shape,
                   cell.left + (cell.right - cell.left - size) / 2,
                   cell.top + (cell.bottom - cell.top - size) / 2,
                   size, color);
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_DODEFAULT;
}

static LRESULT CALLBACK header_subclass_proc(HWND header, UINT message, WPARAM wparam,
                                              LPARAM lparam, UINT_PTR subclass_id,
                                              DWORD_PTR reference_data)
{
    App *app = (App *)reference_data;
    (void)wparam; (void)lparam; (void)subclass_id;
    if (message == WM_PAINT && app) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(header, &paint);
        RECT client;
        int count = Header_GetItemCount(header);
        int i;
        GetClientRect(header, &client);
        FillRect(dc, &client, app->theme.input_brush);
        for (i = 0; i < count; ++i) {
            HDITEMW header_item;
            wchar_t text[96];
            RECT rc;
            HPEN border;
            HPEN old_pen;
            if (!Header_GetItemRect(header, i, &rc)) continue;
            ZeroMemory(&header_item, sizeof(header_item));
            header_item.mask = HDI_TEXT;
            header_item.pszText = text;
            header_item.cchTextMax = 96;
            text[0] = L'\0';
            Header_GetItem(header, i, &header_item);
            border = CreatePen(PS_SOLID, 1, app->theme.border);
            old_pen = SelectObject(dc, border);
            MoveToEx(dc, rc.right - 1, rc.top, NULL);
            LineTo(dc, rc.right - 1, rc.bottom);
            LineTo(dc, rc.left, rc.bottom - 1);
            SelectObject(dc, old_pen);
            DeleteObject(border);
            SelectObject(dc, app->font_bold);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, app->theme.muted_text);
            rc.left += scale_for(app->dpi, 8);
            DrawTextW(dc, text, -1, &rc,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        EndPaint(header, &paint);
        return 0;
    }
    if (message == WM_ERASEBKGND && app) return 1;
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(header, header_subclass_proc, 1);
    return DefSubclassProc(header, message, wparam, lparam);
}

static void draw_filter_button(App *app, const DRAWITEMSTRUCT *item)
{
    wchar_t text[64];
    RECT rc = item->rcItem;
    int filter = item->CtlID == ID_FILTER_PROGRESS ? 1 :
                 item->CtlID == ID_FILTER_RECURRING ? 2 : 0;
    int selected = app->task_filter == filter;
    GetWindowTextW(item->hwndItem, text, 64);
    FillRect(item->hDC, &rc, selected ? app->theme.accent_soft_brush : app->theme.panel_brush);
    SelectObject(item->hDC, selected ? app->font_bold : app->font);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, selected ? app->theme.text : app->theme.muted_text);
    DrawTextW(item->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (selected) {
        RECT line = rc;
        HBRUSH accent = CreateSolidBrush(app->theme.accent);
        line.top = line.bottom - scale_for(app->dpi, 2);
        FillRect(item->hDC, &line, accent);
        DeleteObject(accent);
    }
}

static void draw_combo_item(App *app, const DRAWITEMSTRUCT *item)
{
    RECT rc = item->rcItem;
    wchar_t text[96];
    int index = item->itemID == (UINT)-1
        ? ComboBox_GetCurSel(item->hwndItem) : (int)item->itemID;
    int selected = (item->itemState & ODS_SELECTED) != 0;
    HBRUSH background = selected ? app->theme.accent_soft_brush : app->theme.input_brush;
    FillRect(item->hDC, &rc, background);
    text[0] = L'\0';
    if (index >= 0) ComboBox_GetLBText(item->hwndItem, index, text);
    SelectObject(item->hDC, app->font);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, app->theme.text);
    rc.left += scale_for(app->dpi, 8);
    rc.right -= scale_for(app->dpi, 4);
    DrawTextW(item->hDC, text, -1, &rc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
}

static void draw_action_button(App *app, const DRAWITEMSTRUCT *item)
{
    int primary = item->CtlID == ID_SAVE_TASK || item->CtlID == ID_AGENT_DRAFT;
    int pressed = (item->itemState & ODS_SELECTED) != 0;
    RECT rc = item->rcItem;
    wchar_t text[64];
    HBRUSH fill;
    HPEN border;
    HPEN old_pen;
    if (pressed) fill = CreateSolidBrush(app->theme.accent_soft);
    else fill = CreateSolidBrush(app->theme.input);
    border = CreatePen(PS_SOLID, 1, primary ? app->theme.accent : app->theme.border);
    old_pen = SelectObject(item->hDC, border);
    {
        HBRUSH old_brush = SelectObject(item->hDC, fill);
        Rectangle(item->hDC, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(item->hDC, old_brush);
    }
    SelectObject(item->hDC, old_pen);
    DeleteObject(border);
    DeleteObject(fill);
    GetWindowTextW(item->hwndItem, text, 64);
    SelectObject(item->hDC, primary ? app->font_bold : app->font);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, app->theme.text);
    DrawTextW(item->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item->itemState & ODS_FOCUS) {
        InflateRect(&rc, -scale_for(app->dpi, 3), -scale_for(app->dpi, 3));
        DrawFocusRect(item->hDC, &rc);
    }
}

static void draw_navigation_item(App *app, const DRAWITEMSTRUCT *item)
{
    int selected =
        (item->CtlID == ID_NAV_CALENDAR && app->page == PAGE_CALENDAR) ||
        (item->CtlID == ID_NAV_TASKS && app->page == PAGE_TASKS) ||
        (item->CtlID == ID_NAV_NEW && app->page == PAGE_NEW);
    RECT rc = item->rcItem;
    wchar_t text[64];
    HBRUSH fill = selected ? app->theme.accent_soft_brush : app->theme.navigation_brush;
    FillRect(item->hDC, &rc, fill);
    if (selected) {
        RECT bar = rc;
        HBRUSH accent = CreateSolidBrush(app->theme.accent);
        bar.right = bar.left + scale_for(app->dpi, 4);
        FillRect(item->hDC, &bar, accent);
        DeleteObject(accent);
    }
    if (item->itemState & ODS_SELECTED) {
        HBRUSH pressed = CreateSolidBrush(app->theme.input);
        InflateRect(&rc, -scale_for(app->dpi, 5), -scale_for(app->dpi, 5));
        FillRect(item->hDC, &rc, pressed);
        DeleteObject(pressed);
        rc = item->rcItem;
    }
    GetWindowTextW(item->hwndItem, text, 64);
    SelectObject(item->hDC, selected ? app->font_bold : app->font);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, selected ? app->theme.accent : app->theme.text);
    rc.left += scale_for(app->dpi, 16);
    DrawTextW(item->hDC, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
}

static int static_is_on_panel(App *app, HWND control_window)
{
    return control_window == app->agenda_title || control_window == app->agenda_hint ||
           control_window == app->label_title || control_window == app->label_category ||
           control_window == app->label_start || control_window == app->label_due ||
           control_window == app->label_repeat || control_window == app->form_status ||
           control_window == app->agent_panel.title || control_window == app->agent_panel.hint;
}

static int static_is_secondary(App *app, HWND control_window)
{
    return control_window == app->page_hint || control_window == app->agenda_hint ||
           control_window == app->form_status || control_window == app->agent_panel.hint;
}

static LRESULT CALLBACK calendar_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    App *app = (App *)(LONG_PTR)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        return TRUE;
    }
    if (message == WM_PAINT && app) {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        int header_h = scale_for(app->dpi, 68);
        int week_h = scale_for(app->dpi, 30);
        int grid_top = header_h + week_h;
        int col_w, row_h, first_weekday, day, days, i;
        SYSTEMTIME first = {0};
        SYSTEMTIME today;
        wchar_t title[64], number[8];
        static const wchar_t *months[] = { L"January",L"February",L"March",L"April",L"May",L"June",L"July",L"August",L"September",L"October",L"November",L"December" };
        static const wchar_t *weekdays[] = { L"SUN",L"MON",L"TUE",L"WED",L"THU",L"FRI",L"SAT" };
        GetClientRect(hwnd, &rc);
        GetLocalTime(&today);
        FillRect(dc, &rc, app->theme.panel_brush);
        SelectObject(dc, app->font_title);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, app->theme.text);
        _snwprintf(title, 63, L"%ls %d", months[app->calendar_month - 1], app->calendar_year);
        TextOutW(dc, scale_for(app->dpi, 18), scale_for(app->dpi, 17), title, (int)wcslen(title));
        col_w = rc.right / 7;
        for (i = 0; i < 7; ++i) {
            RECT wr = { i * col_w, header_h, (i + 1) * col_w, grid_top };
            SelectObject(dc, app->font_bold); SetTextColor(dc, app->theme.muted_text);
            DrawTextW(dc, weekdays[i], -1, &wr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        first.wYear = (WORD)app->calendar_year; first.wMonth = (WORD)app->calendar_month; first.wDay = 1;
        {
            FILETIME ft;
            SystemTimeToFileTime(&first, &ft);
            FileTimeToSystemTime(&ft, &first);
        }
        first_weekday = first.wDayOfWeek;
        days = days_in_month(app->calendar_year, app->calendar_month);
        row_h = (rc.bottom - grid_top) / 6;
        SelectObject(dc, app->font);
        for (i = 0; i < 42; ++i) {
            int col = i % 7, row = i / 7;
            RECT cell = { col * col_w, grid_top + row * row_h, (col + 1) * col_w, grid_top + (row + 1) * row_h };
            day = i - first_weekday + 1;
            if (col == 0 || col == 6) {
                FillRect(dc, &cell, app->theme.weekend_brush);
            }
            if (day >= 1 && day <= days && today.wYear == app->calendar_year &&
                today.wMonth == app->calendar_month && today.wDay == day) {
                FillRect(dc, &cell, app->theme.accent_soft_brush);
            }
            HPEN grid = CreatePen(PS_SOLID, 1, app->theme.border);
            HPEN old = SelectObject(dc, grid);
            MoveToEx(dc, cell.left, cell.top, NULL); LineTo(dc, cell.right, cell.top); LineTo(dc, cell.right, cell.bottom);
            SelectObject(dc, old); DeleteObject(grid);
            if (day >= 1 && day <= days) {
                size_t t;
                int marker_x = cell.left + scale_for(app->dpi, 9);
                _snwprintf(number, 7, L"%d", day);
                if (today.wYear == app->calendar_year && today.wMonth == app->calendar_month && today.wDay == day) {
                    SelectObject(dc, app->font_bold);
                    SetTextColor(dc, app->theme.accent);
                } else {
                    SelectObject(dc, app->font);
                    SetTextColor(dc, app->theme.text);
                }
                TextOutW(dc, cell.left + scale_for(app->dpi, 8), cell.top + scale_for(app->dpi, 6), number, (int)wcslen(number));
                for (t = 0; t < app->store.count; ++t) {
                    Task *task = &app->store.items[t];
                    Date date = { app->calendar_year, app->calendar_month, day };
                    if (task_occurs_on(task, date)) {
                        COLORREF c = RGB((task->color_rgb >> 16) & 255, (task->color_rgb >> 8) & 255, task->color_rgb & 255);
                        draw_shape(dc, (int)task->shape, marker_x, cell.top + scale_for(app->dpi, 31), scale_for(app->dpi, 11), c);
                        marker_x += scale_for(app->dpi, 17);
                    }
                }
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static void apply_dark_control_themes(App *app)
{
    HWND controls[] = {
        app->cal_prev, app->cal_today, app->cal_next,
        app->agenda_list, ListView_GetHeader(app->agenda_list),
        app->filter_all, app->filter_progress, app->filter_recurring,
        app->task_list, ListView_GetHeader(app->task_list),
        app->edit_title, app->combo_category, app->edit_start, app->edit_due,
        app->combo_repeat, app->save_task,
        app->agent_panel.input, app->agent_panel.draft_button, app->agent_panel.output
    };
    size_t i;
    for (i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
        use_dark_control_theme(controls[i]);
}

static void create_ui(App *app)
{
    LVCOLUMNW column;
    app->font = make_font(app->dpi, 10, FW_NORMAL);
    app->font_bold = make_font(app->dpi, 10, FW_SEMIBOLD);
    app->font_title = make_font(app->dpi, 20, FW_SEMIBOLD);
    ui_theme_init(&app->theme);

    app->nav_calendar = control(app, L"BUTTON", L"Calendar", BS_OWNERDRAW | WS_TABSTOP | WS_VISIBLE, 0, ID_NAV_CALENDAR);
    app->nav_tasks = control(app, L"BUTTON", L"Tasks", BS_OWNERDRAW | WS_TABSTOP | WS_VISIBLE, 0, ID_NAV_TASKS);
    app->nav_new = control(app, L"BUTTON", L"+  New task", BS_OWNERDRAW | WS_TABSTOP | WS_VISIBLE, 0, ID_NAV_NEW);
    app->page_title = control(app, L"STATIC", L"Calendar", SS_LEFT | WS_VISIBLE, 0, 0);
    app->page_hint = control(app, L"STATIC", L"", SS_LEFT | WS_VISIBLE, 0, 0);
    SendMessageW(app->page_title, WM_SETFONT, (WPARAM)app->font_title, TRUE);

    app->cal_prev = control(app, L"BUTTON", L"<", BS_OWNERDRAW, 0, ID_CAL_PREV);
    app->cal_today = control(app, L"BUTTON", L"Today", BS_OWNERDRAW, 0, ID_CAL_TODAY);
    app->cal_next = control(app, L"BUTTON", L">", BS_OWNERDRAW, 0, ID_CAL_NEXT);
    app->calendar = CreateWindowExW(0, CAL_CLASS, L"", WS_CHILD | WS_BORDER, 0,0,0,0,
                                    app->window, NULL, GetModuleHandleW(NULL), app);
    app->agenda_title = control(app, L"STATIC", L"This month", SS_LEFT, 0, 0);
    SendMessageW(app->agenda_title, WM_SETFONT, (WPARAM)app->font_bold, TRUE);
    // app->agenda_hint = control(app, L"STATIC", L"Scheduled tasks and their dates", SS_LEFT, 0, 0);
    app->agenda_list = control(app, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                               WS_EX_CLIENTEDGE, 0);
    ListView_SetExtendedListViewStyle(app->agenda_list,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(app->agenda_list, app->theme.panel);
    ListView_SetTextBkColor(app->agenda_list, app->theme.panel);
    ListView_SetTextColor(app->agenda_list, app->theme.text);
    ZeroMemory(&column, sizeof(column)); column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = L"Date"; column.cx = scale_for(app->dpi, 92); ListView_InsertColumn(app->agenda_list, 0, &column);
    column.pszText = L"Task"; column.cx = scale_for(app->dpi, 200); ListView_InsertColumn(app->agenda_list, 1, &column);
    column.pszText = L"Icon"; column.cx = scale_for(app->dpi, 48); ListView_InsertColumn(app->agenda_list, 2, &column);

    app->filter_all = control(app, L"BUTTON", L"All active", BS_OWNERDRAW | WS_TABSTOP, 0, ID_FILTER_ALL);
    app->filter_progress = control(app, L"BUTTON", L"In progress", BS_OWNERDRAW | WS_TABSTOP, 0, ID_FILTER_PROGRESS);
    app->filter_recurring = control(app, L"BUTTON", L"Recurring", BS_OWNERDRAW | WS_TABSTOP, 0, ID_FILTER_RECURRING);
    app->task_filter = 0;
    app->task_list = control(app, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, ID_TASK_LIST);
    ListView_SetExtendedListViewStyle(app->task_list,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(app->task_list, app->theme.panel);
    ListView_SetTextBkColor(app->task_list, app->theme.panel);
    ListView_SetTextColor(app->task_list, app->theme.text);
    ZeroMemory(&column, sizeof(column)); column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = L"Task"; column.cx = scale_for(app->dpi, 205); ListView_InsertColumn(app->task_list, 0, &column);
    column.pszText = L"Category"; column.cx = scale_for(app->dpi, 150); ListView_InsertColumn(app->task_list, 1, &column);
    column.pszText = L"Schedule"; column.cx = scale_for(app->dpi, 160); ListView_InsertColumn(app->task_list, 2, &column);
    column.pszText = L"View"; column.cx = scale_for(app->dpi, 100); ListView_InsertColumn(app->task_list, 3, &column);
    column.pszText = L"Icon"; column.cx = scale_for(app->dpi, 58); ListView_InsertColumn(app->task_list, 4, &column);

    app->label_title = control(app, L"STATIC", L"Task", SS_LEFT, 0, 0);
    app->edit_title = control(app, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_TITLE);
    app->label_category = control(app, L"STATIC", L"Category", SS_LEFT, 0, 0);
    app->combo_category = control(app, WC_COMBOBOXW, L"", CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP, 0, ID_CATEGORY);
    ComboBox_AddString(app->combo_category, L"Household maintenance");
    ComboBox_AddString(app->combo_category, L"Administration");
    ComboBox_AddString(app->combo_category, L"Health");
    ComboBox_AddString(app->combo_category, L"Uncategorized");
    ComboBox_SetCurSel(app->combo_category, 0);
    app->label_start = control(app, L"STATIC", L"Start date", SS_LEFT, 0, 0);
    app->edit_start = control(app, L"EDIT", L"2026-09-15", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_START_DATE);
    app->label_due = control(app, L"STATIC", L"Due date", SS_LEFT, 0, 0);
    app->edit_due = control(app, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_DUE_DATE);
    app->label_repeat = control(app, L"STATIC", L"Repeats", SS_LEFT, 0, 0);
    app->combo_repeat = control(app, WC_COMBOBOXW, L"",
        CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
        0, ID_REPEAT);
    ComboBox_AddString(app->combo_repeat, L"Does not repeat"); ComboBox_AddString(app->combo_repeat, L"Daily");
    ComboBox_AddString(app->combo_repeat, L"Weekly"); ComboBox_AddString(app->combo_repeat, L"Monthly");
    ComboBox_AddString(app->combo_repeat, L"Yearly"); ComboBox_SetCurSel(app->combo_repeat, 0);
    SendMessageW(app->combo_repeat, CB_SETITEMHEIGHT, (WPARAM)-1, scale_for(app->dpi, 28));
    SendMessageW(app->combo_repeat, CB_SETITEMHEIGHT, 0, scale_for(app->dpi, 28));
    app->save_task = control(app, L"BUTTON", L"Add task", BS_OWNERDRAW | WS_TABSTOP, 0, ID_SAVE_TASK);
    app->form_status = control(app, L"STATIC", L"Tasks are kept in memory in this prototype.", SS_LEFT, 0, 0);

    agent_panel_create(&app->agent_panel, app->window, app->font, app->font_bold,
                       ID_AGENT_INPUT, ID_AGENT_DRAFT, ID_AGENT_OUTPUT);
    SetWindowSubclass(ListView_GetHeader(app->agenda_list), header_subclass_proc, 1, (DWORD_PTR)app);
    SetWindowSubclass(ListView_GetHeader(app->task_list), header_subclass_proc, 1, (DWORD_PTR)app);
    apply_dark_control_themes(app);
}

static void save_task(App *app)
{
    Task task;
    wchar_t error[160];
    int repeat;
    ZeroMemory(&task, sizeof(task));
    GetWindowTextW(app->edit_title, task.title, TASK_TITLE_CAP);
    GetWindowTextW(app->combo_category, task.category, TASK_CATEGORY_CAP);
    if (!parse_date(app->edit_start, &task.start)) {
        SetWindowTextW(app->form_status, L"Use YYYY-MM-DD for the start date."); return;
    }
    if (GetWindowTextLengthW(app->edit_due) && !parse_date(app->edit_due, &task.due)) {
        SetWindowTextW(app->form_status, L"Use YYYY-MM-DD for the due date, or leave it empty."); return;
    }
    repeat = ComboBox_GetCurSel(app->combo_repeat);
    task.repeat = repeat < 0 ? REPEAT_NONE : (RepeatUnit)repeat;
    task.repeat_interval = 1;
    task.kind = task.repeat == REPEAT_NONE ? TASK_FINITE : TASK_RECURRING;
    task.shape = (unsigned)(app->store.count % 3);
    task.color_rgb = 0xB17A43;
    if (task_store_add(&app->store, &task, error, 160)) {
        SetWindowTextW(app->form_status, L"Task added to this session. Open Calendar or Tasks to see it.");
        SetWindowTextW(app->edit_title, L"");
        InvalidateRect(app->calendar, NULL, TRUE);
    } else SetWindowTextW(app->form_status, error);
}

static void update_today(App *app)
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    app->calendar_year = now.wYear;
    app->calendar_month = now.wMonth;
    populate_month_agenda(app);
    InvalidateRect(app->calendar, NULL, TRUE);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    App *app = (App *)(LONG_PTR)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        SYSTEMTIME now;
        app = (App *)create->lpCreateParams;
        app->window = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
        app->dpi = window_dpi(hwnd);
        GetLocalTime(&now); app->calendar_year = now.wYear; app->calendar_month = now.wMonth;
        task_store_init(&app->store);
        create_ui(app);
        show_page(app, PAGE_CALENDAR);
        return 0;
    }
    case WM_SIZE:
        if (app) { layout(app); InvalidateRect(hwnd, NULL, TRUE); }
        return 0;
    case WM_DPICHANGED:
        if (app) {
            RECT *suggested = (RECT *)lparam;
            app->dpi = HIWORD(wparam);
            SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            layout(app);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    case WM_COMMAND:
        if (!app) break;
        switch (LOWORD(wparam)) {
        case ID_NAV_CALENDAR: show_page(app, PAGE_CALENDAR); break;
        case ID_NAV_TASKS: show_page(app, PAGE_TASKS); break;
        case ID_NAV_NEW: show_page(app, PAGE_NEW); break;
        case ID_FILTER_ALL:
            app->task_filter = 0; populate_task_list(app);
            InvalidateRect(app->filter_all, NULL, TRUE);
            InvalidateRect(app->filter_progress, NULL, TRUE);
            InvalidateRect(app->filter_recurring, NULL, TRUE); break;
        case ID_FILTER_PROGRESS:
            app->task_filter = 1; populate_task_list(app);
            InvalidateRect(app->filter_all, NULL, TRUE);
            InvalidateRect(app->filter_progress, NULL, TRUE);
            InvalidateRect(app->filter_recurring, NULL, TRUE); break;
        case ID_FILTER_RECURRING:
            app->task_filter = 2; populate_task_list(app);
            InvalidateRect(app->filter_all, NULL, TRUE);
            InvalidateRect(app->filter_progress, NULL, TRUE);
            InvalidateRect(app->filter_recurring, NULL, TRUE); break;
        case ID_CAL_PREV:
            if (--app->calendar_month == 0) { app->calendar_month = 12; --app->calendar_year; }
            populate_month_agenda(app);
            InvalidateRect(app->calendar, NULL, TRUE); break;
        case ID_CAL_TODAY: update_today(app); break;
        case ID_CAL_NEXT:
            if (++app->calendar_month == 13) { app->calendar_month = 1; ++app->calendar_year; }
            populate_month_agenda(app);
            InvalidateRect(app->calendar, NULL, TRUE); break;
        case ID_SAVE_TASK: save_task(app); break;
        case ID_AGENT_DRAFT: agent_panel_draft(&app->agent_panel); break;
        }
        return 0;
    case WM_NOTIFY:
        if (app && ((NMHDR *)lparam)->code == NM_CUSTOMDRAW) {
            NMHDR *header = (NMHDR *)lparam;
            if (header->hwndFrom == app->task_list)
                return draw_list_icon_cell(app, (NMLVCUSTOMDRAW *)lparam, 4);
            if (header->hwndFrom == app->agenda_list)
                return draw_list_icon_cell(app, (NMLVCUSTOMDRAW *)lparam, 2);
        }
        break;
    case WM_DRAWITEM:
        if (app) {
            DRAWITEMSTRUCT *item = (DRAWITEMSTRUCT *)lparam;
            if (item->CtlID == ID_REPEAT && item->CtlType == ODT_COMBOBOX) {
                draw_combo_item(app, item);
                return TRUE;
            }
            if (item->CtlID == ID_FILTER_ALL || item->CtlID == ID_FILTER_PROGRESS ||
                item->CtlID == ID_FILTER_RECURRING) {
                draw_filter_button(app, item);
                return TRUE;
            }
            if (item->CtlID == ID_CAL_PREV || item->CtlID == ID_CAL_TODAY ||
                item->CtlID == ID_CAL_NEXT || item->CtlID == ID_SAVE_TASK ||
                item->CtlID == ID_AGENT_DRAFT) {
                draw_action_button(app, item);
                return TRUE;
            }
            if (item->CtlID == ID_NAV_CALENDAR || item->CtlID == ID_NAV_TASKS ||
                item->CtlID == ID_NAV_NEW) {
                draw_navigation_item(app, item);
                return TRUE;
            }
        }
        break;
    case WM_GETMINMAXINFO:
        if (app) {
            MINMAXINFO *limits = (MINMAXINFO *)lparam;
            limits->ptMinTrackSize.x = scale_for(app->dpi, 1050);
            limits->ptMinTrackSize.y = scale_for(app->dpi, 650);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
        if (app) {
            HDC dc = (HDC)wparam;
            HWND control_window = (HWND)lparam;
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, static_is_secondary(app, control_window)
                ? app->theme.muted_text : app->theme.text);
            return (LRESULT)(static_is_on_panel(app, control_window)
                ? app->theme.panel_brush : app->theme.background_brush);
        }
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (app) {
            HDC dc = (HDC)wparam;
            SetTextColor(dc, app->theme.text);
            SetBkColor(dc, app->theme.input);
            return (LRESULT)app->theme.input_brush;
        }
        break;
    case WM_CTLCOLORBTN:
        if (app) {
            HDC dc = (HDC)wparam;
            SetTextColor(dc, app->theme.text);
            SetBkColor(dc, app->theme.panel);
            return (LRESULT)app->theme.panel_brush;
        }
        break;
    case WM_PAINT:
        if (app) {
            PAINTSTRUCT paint;
            RECT client, navigation, divider;
            HDC dc = BeginPaint(hwnd, &paint);
            GetClientRect(hwnd, &client);
            FillRect(dc, &client, app->theme.background_brush);
            navigation = client;
            navigation.right = scale_for(app->dpi, 196);
            FillRect(dc, &navigation, app->theme.navigation_brush);

            SelectObject(dc, app->font_bold);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, app->theme.text);
            TextOutW(dc, scale_for(app->dpi, 18), scale_for(app->dpi, 27), L"TASKBOARD", 9);
            SelectObject(dc, app->font);
            SetTextColor(dc, app->theme.muted_text);
            TextOutW(dc, scale_for(app->dpi, 18), scale_for(app->dpi, 52), L"PERSONAL ORGANIZER", 18);

            divider = navigation;
            divider.left = divider.right - 1;
            FillRect(dc, &divider, app->theme.background_brush);
            ui_draw_card(dc, &app->primary_card, &app->theme);
            if (!IsRectEmpty(&app->support_card))
                ui_draw_card(dc, &app->support_card, &app->theme);
            EndPaint(hwnd, &paint);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        if (app) return 1;
        break;
    case WM_DESTROY:
        if (app) {
            DeleteObject(app->font); DeleteObject(app->font_bold); DeleteObject(app->font_title);
            ui_theme_destroy(&app->theme);
        }
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show)
{
    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES };
    WNDCLASSEXW wc;
    App app;
    HWND window;
    MSG message;
    (void)previous; (void)command_line;
    InitCommonControlsEx(&controls);
    ZeroMemory(&app, sizeof(app));

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc); wc.hInstance = instance; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION); wc.hbrBackground = NULL;
    wc.lpfnWndProc = calendar_proc; wc.lpszClassName = CAL_CLASS;
    RegisterClassExW(&wc);
    wc.lpfnWndProc = window_proc; wc.lpszClassName = APP_CLASS;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    window = CreateWindowExW(0, APP_CLASS, L"Taskboard", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, NULL, NULL, instance, &app);
    if (!window) return 1;
    enable_dark_title_bar(window);
    ShowWindow(window, show); UpdateWindow(window);
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
    }
    return (int)message.wParam;
}
