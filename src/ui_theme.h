#ifndef UI_THEME_H
#define UI_THEME_H

#include <windows.h>

typedef struct UiTheme {
    COLORREF background;
    COLORREF navigation;
    COLORREF panel;
    COLORREF border;
    COLORREF text;
    COLORREF muted_text;
    COLORREF accent;
    COLORREF accent_soft;
    COLORREF input;
    COLORREF weekend;
    HBRUSH background_brush;
    HBRUSH navigation_brush;
    HBRUSH panel_brush;
    HBRUSH accent_soft_brush;
    HBRUSH input_brush;
    HBRUSH weekend_brush;
} UiTheme;

void ui_theme_init(UiTheme *theme);
void ui_theme_destroy(UiTheme *theme);
int ui_scale(UINT dpi, int logical_pixels);
void ui_draw_card(HDC dc, const RECT *rect, const UiTheme *theme);

#endif
