#include "ui_theme.h"

void ui_theme_init(UiTheme *theme)
{
    theme->background = RGB(17, 18, 18);
    theme->navigation = RGB(12, 13, 13);
    theme->panel = RGB(24, 26, 25);
    theme->border = RGB(55, 59, 57);
    theme->text = RGB(242, 244, 243);
    theme->muted_text = RGB(164, 171, 167);
    theme->accent = RGB(255, 255, 255);
    theme->accent_soft = RGB(48, 52, 50);
    theme->input = RGB(31, 34, 32);
    theme->weekend = RGB(28, 30, 29);
    theme->background_brush = CreateSolidBrush(theme->background);
    theme->navigation_brush = CreateSolidBrush(theme->navigation);
    theme->panel_brush = CreateSolidBrush(theme->panel);
    theme->accent_soft_brush = CreateSolidBrush(theme->accent_soft);
    theme->input_brush = CreateSolidBrush(theme->input);
    theme->weekend_brush = CreateSolidBrush(theme->weekend);
}

void ui_theme_destroy(UiTheme *theme)
{
    DeleteObject(theme->background_brush);
    DeleteObject(theme->navigation_brush);
    DeleteObject(theme->panel_brush);
    DeleteObject(theme->accent_soft_brush);
    DeleteObject(theme->input_brush);
    DeleteObject(theme->weekend_brush);
}

int ui_scale(UINT dpi, int logical_pixels)
{
    return MulDiv(logical_pixels, (int)dpi, 96);
}

void ui_draw_card(HDC dc, const RECT *rect, const UiTheme *theme)
{
    HPEN border = CreatePen(PS_SOLID, 1, theme->border);
    HPEN old_pen = SelectObject(dc, border);
    HBRUSH old_brush = SelectObject(dc, theme->panel_brush);
    Rectangle(dc, rect->left, rect->top, rect->right, rect->bottom);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);
}
