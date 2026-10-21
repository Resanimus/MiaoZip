// MiaoZip client title bar. Windows still owns resizing and caption buttons.
#pragma once
#include "Theme.h"
#include <uxtheme.h>
#include <windowsx.h>
#include <vector>
#include <algorithm>
#pragma comment(lib, "uxtheme.lib")

namespace MiaoZip::Caption
{
    inline constexpr int Height = 32;

    inline void Invalidate(HWND window)
    {
        RECT area{}; GetClientRect(window, &area);
        area.bottom = Theme::Scale(window, Height);
        InvalidateRect(window, &area, FALSE);
    }

    inline LRESULT Calculate(HWND window, WPARAM parameter, LPARAM value)
    {
        auto* layout = reinterpret_cast<NCCALCSIZE_PARAMS*>(value);
        const LONG top = layout->rgrc[0].top;
        DefWindowProcW(window, WM_NCCALCSIZE, parameter, value);
        const UINT dpi = GetDpiForWindow(window);
        layout->rgrc[0].top = top + (IsZoomed(window)
            ? GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi)
                + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi) : 0);
        return 0;
    }

    inline LRESULT HitTest(HWND window, WPARAM parameter, LPARAM value)
    {
        // Use exactly the same client rectangles as the painted glyphs. DWM may
        // keep caption bounds in frame coordinates after extending the client.
        POINT clientPoint{ GET_X_LPARAM(value), GET_Y_LPARAM(value) };
        ScreenToClient(window, &clientPoint);
        RECT client{}; GetClientRect(window, &client);
        const int buttonWidth = Theme::Scale(window, 46);
        if (clientPoint.y >= 0 && clientPoint.y < Theme::Scale(window, Height) &&
            clientPoint.x >= client.right - buttonWidth * 3 && clientPoint.x < client.right)
        {
            if (clientPoint.x >= client.right - buttonWidth) return HTCLOSE;
            if (clientPoint.x >= client.right - buttonWidth * 2) return HTMAXBUTTON;
            return HTMINBUTTON;
        }
        LRESULT result{};
        if (DwmDefWindowProc(window, WM_NCHITTEST, parameter, value, &result)) return result;
        const LRESULT standard = DefWindowProcW(window, WM_NCHITTEST, parameter, value);
        if (standard != HTCLIENT && standard != HTCAPTION) return standard;
        POINT pointer{ GET_X_LPARAM(value), GET_Y_LPARAM(value) };
        RECT frame{}; GetWindowRect(window, &frame);
        const int border = Theme::Scale(window, 7);
        if (!IsZoomed(window) && pointer.y < frame.top + border)
        {
            if (pointer.x < frame.left + border * 2) return HTTOPLEFT;
            if (pointer.x >= frame.right - border * 2) return HTTOPRIGHT;
            return HTTOP;
        }
        ScreenToClient(window, &pointer);
        return pointer.y < Theme::Scale(window, Height) ? HTCAPTION : HTCLIENT;
    }

    inline void Paint(HWND window)
    {
        PAINTSTRUCT paint{}; HDC target = BeginPaint(window, &paint);
        RECT client{}; GetClientRect(window, &client);
        const int width = client.right, height = Theme::Scale(window, Height);
        if (width <= 0 || height <= 0) { EndPaint(window, &paint); return; }
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(target, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        HDC memory = bitmap ? CreateCompatibleDC(target) : nullptr;
        if (memory)
        {
            const auto oldBitmap = SelectObject(memory, bitmap);
            const bool glass = GetPropW(window, L"MiaoZip.GlassActive") != nullptr;
            const bool dark = Theme::IsDark();
            const auto palette = Theme::Colors();
            const COLORREF color = glass && dark ? RGB(26,31,39) : palette.Chrome;
            const DWORD alpha = glass ? (dark ? 0xC0 : 0x70) : 255;
            const DWORD pixel = (alpha << 24) | (GetRValue(color) * alpha / 255 << 16)
                | (GetGValue(color) * alpha / 255 << 8) | (GetBValue(color) * alpha / 255);
            std::fill_n(static_cast<DWORD*>(pixels), static_cast<size_t>(width) * height, pixel);
            HFONT font = CreateFontW(-Theme::Scale(window, 12), 0, 0, 0, FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
            const auto oldFont = SelectObject(memory, font);
            std::vector<wchar_t> title(static_cast<size_t>(GetWindowTextLengthW(window)) + 1);
            GetWindowTextW(window, title.data(), static_cast<int>(title.size()));
            RECT text{ Theme::Scale(window, 36), 0,
                width - Theme::Scale(window, 155), height };
            if (HTHEME theme = OpenThemeData(window, L"WINDOW"))
            {
                DTTOPTS options{ sizeof(options) };
                options.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;
                options.crText = palette.Text;
                DrawThemeTextEx(theme, memory, 0, 0, title.data(), -1,
                    DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX, &text, &options);
                // Keep the system hit targets, and restore crisp glyphs above our shared glass tint.
                HFONT glyphFont = CreateFontW(-Theme::Scale(window, 10), 0, 0, 0, FW_NORMAL,
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe MDL2 Assets");
                const auto oldGlyphFont = SelectObject(memory, glyphFont);
                RECT bounds{};
                int buttonHeight = height;
                if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CAPTION_BUTTON_BOUNDS, &bounds, sizeof(bounds)))
                    && bounds.bottom > bounds.top)
                    buttonHeight = (std::min)(height, static_cast<int>(bounds.bottom - bounds.top));
                // DWM's bounds use frame coordinates; anchor glyphs to the visible client right edge.
                const int buttonWidth = Theme::Scale(window, 46);
                const wchar_t glyphs[]{ L'\xE921', IsZoomed(window) ? L'\xE923' : L'\xE922', L'\xE8BB' };
                for (int i = 0; i < 3; ++i)
                {
                    RECT button{ width - buttonWidth * (3 - i), 0,
                        width - buttonWidth * (2 - i), buttonHeight };
                    DrawThemeTextEx(theme, memory, 0, 0, &glyphs[i], 1,
                        DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX, &button, &options);
                }
                SelectObject(memory, oldGlyphFont); if (glyphFont) DeleteObject(glyphFont);
                CloseThemeData(theme);
            }
            const HICON icon = reinterpret_cast<HICON>(GetClassLongPtrW(window, GCLP_HICON));
            const int iconSize = Theme::Scale(window, 16);
            if (icon) DrawIconEx(memory, Theme::Scale(window, 12), (height - iconSize) / 2,
                icon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
            BitBlt(target, 0, 0, width, height, memory, 0, 0, SRCCOPY);
            SelectObject(memory, oldFont); if (font) DeleteObject(font);
            SelectObject(memory, oldBitmap); DeleteDC(memory);
        }
        if (bitmap) DeleteObject(bitmap);
        EndPaint(window, &paint);
    }
}
