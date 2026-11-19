// Shared presentation for our Win32 forms. Native control classes, state and
// notifications remain intact, including automatic checkbox/radio behaviour.
#pragma once
#include "Theme.h"
#include <CommCtrl.h>
#include <Uxtheme.h>
#include <windowsx.h>
#include <algorithm>
#include <string>
#include <vector>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace MiaoZip::Dialogs
{
    inline constexpr wchar_t SurfaceProperty[] = L"MiaoZip.DialogSurface";
    inline constexpr wchar_t CustomLayoutProperty[] = L"MiaoZip.DialogLayout";
    inline constexpr UINT_PTR SurfaceId = 0x4d5a4453;
    inline constexpr UINT_PTR ControlId = 0x4d5a434e;
    inline constexpr UINT ReflowMessage = WM_APP + 0x2b1;
    inline constexpr UINT DpiChangedMessage = 0x02e0; // WM_DPICHANGED with the upstream legacy SDK target.
    // TaskDialog owns a dynamic DirectUI layout; resizing it as a static Win32
    // form can hide its action buttons. Its palette still follows MiaoZip.
    inline thread_local unsigned NativeLayoutDepth = 0;
    struct NativeLayoutScope
    {
        NativeLayoutScope() { ++NativeLayoutDepth; }
        ~NativeLayoutScope() { --NativeLayoutDepth; }
        NativeLayoutScope(const NativeLayoutScope&) = delete;
        NativeLayoutScope& operator=(const NativeLayoutScope&) = delete;
    };

    inline bool ClassIs(HWND window, const wchar_t* name)
    {
        wchar_t buffer[128]{};
        GetClassNameW(window, buffer, 128);
        return _wcsicmp(buffer, name) == 0;
    }
    inline std::wstring Text(HWND window)
    {
        std::wstring result(static_cast<size_t>(GetWindowTextLengthW(window)) + 1, L'\0');
        GetWindowTextW(window, result.data(), static_cast<int>(result.size()));
        result.resize(wcslen(result.c_str()));
        return result;
    }
    inline void Outline(HDC dc, RECT rect, COLORREF color, int radius, int width = 1)
    {
        auto pen = CreatePen(PS_SOLID, width, color);
        auto oldPen = SelectObject(dc, pen);
        auto oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(pen);
    }
    inline void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1)
    {
        auto pen = CreatePen(PS_SOLID, width, color);
        auto old = SelectObject(dc, pen);
        MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);
        SelectObject(dc, old); DeleteObject(pen);
    }

    struct ControlState
    {
        bool Hover = false;
        bool MouseFocus = false;
        bool Edit = false;
        bool Combo = false;
        bool Button = false;
        bool ComboList = false;
        int RegionWidth = 0, RegionHeight = 0;
    };
    struct Placement
    {
        HWND Window{};
        RECT Bounds{}; // 96 DPI content coordinates; never scaled cumulatively.
        bool Footer = false;
        bool Combo = false;
    };
    struct Surface
    {
        HFONT Font{};
        HBRUSH Background{}, FieldBrush{};
        std::vector<Placement> Children;
        int Width = 0, Height = 0, Scroll = 0, HorizontalScroll = 0;
        bool Custom = false;
        bool NativeLayout = false;
        bool LayingOut = false;
    };

    inline COLORREF ControlBackground(HWND window)
    {
        const auto palette = Theme::Colors();
        if (ClassIs(window, L"Button") && (GetWindowLongPtrW(window, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX)
            return palette.Chrome;
        RECT control{}; GetWindowRect(window, &control);
        POINT center{(control.left + control.right) / 2, (control.top + control.bottom) / 2};
        for (auto sibling = GetWindow(GetParent(window), GW_CHILD); sibling; sibling = GetWindow(sibling, GW_HWNDNEXT))
        {
            if (sibling == window || !ClassIs(sibling, L"Button") ||
                (GetWindowLongPtrW(sibling, GWL_STYLE) & BS_TYPEMASK) != BS_GROUPBOX) continue;
            RECT group{}; GetWindowRect(sibling, &group);
            if (PtInRect(&group, center)) return palette.Content;
        }
        return palette.Chrome;
    }

    enum class ChoiceMark { None, Check, Mixed, Dot };

    struct RoundedMask
    {
        float Left, Top, Right, Bottom, Radius;

        bool Contains(float x, float y) const
        {
            if (x < Left || x >= Right || y < Top || y >= Bottom) return false;
            const float dx = x - (std::clamp)(x, Left + Radius, Right - Radius);
            const float dy = y - (std::clamp)(y, Top + Radius, Bottom - Radius);
            return dx * dx + dy * dy <= Radius * Radius;
        }
    };

    // The mark coordinates are fractions of the checkbox or radio size.
    inline bool InChoiceStroke(float x, float y, float ax, float ay, float bx, float by)
    {
        const float vx = bx - ax, vy = by - ay;
        const float t = (std::clamp)(((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy), 0.0f, 1.0f);
        const float dx = x - (ax + t * vx), dy = y - (ay + t * vy);
        constexpr float strokeRadius = 0.064f;
        return dx * dx + dy * dy <= strokeRadius * strokeRadius;
    }

    inline bool InChoiceMark(ChoiceMark mark, float x, float y)
    {
        switch (mark)
        {
        case ChoiceMark::Check:
            return InChoiceStroke(x, y, 0.25f, 0.51f, 0.43f, 0.69f) ||
                InChoiceStroke(x, y, 0.43f, 0.69f, 0.77f, 0.34f);
        case ChoiceMark::Mixed:
            return InChoiceStroke(x, y, 0.28f, 0.5f, 0.72f, 0.5f);
        case ChoiceMark::Dot:
            x -= 0.5f; y -= 0.5f;
            return x * x + y * y <= 0.2f * 0.2f;
        default:
            return false;
        }
    }

    // Paint the frame and its mark together so outlines and glyph edges never
    // leave a layer of aliased pixels over the fill. The text still uses GDI.
    inline void PaintRoundedControl(HDC dc, const RECT& bounds, COLORREF background,
        COLORREF fill, COLORREF border, int radius, int stroke, int margin,
        ChoiceMark mark = ChoiceMark::None, COLORREF ink = 0)
    {
        const int width = bounds.right - bounds.left;
        const int height = bounds.bottom - bounds.top;
        if (width <= 0 || height <= 0) return;
        const auto mask = [&](int inset, int corner)
        {
            const float edge = static_cast<float>(inset);
            const float maxRadius = (std::min)(width, height) * 0.5f - edge;
            return RoundedMask{ edge, edge, width - edge, height - edge,
                (std::max)(0.0f, (std::min)(static_cast<float>(corner), maxRadius)) };
        };
        const auto outer = mask(margin, radius);
        const auto inner = mask(margin + stroke, radius - stroke);
        std::vector<DWORD> pixels(static_cast<size_t>(width) * height);
        for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            unsigned red = 0, green = 0, blue = 0;
            for (int sy = 0; sy < 4; ++sy)
            for (int sx = 0; sx < 4; ++sx)
            {
                const float px = x + (sx + 0.5f) / 4, py = y + (sy + 0.5f) / 4;
                COLORREF color = background;
                if (outer.Contains(px, py))
                {
                    color = border;
                    if (inner.Contains(px, py))
                        color = InChoiceMark(mark, px / width, py / height) ? ink : fill;
                }
                red += GetRValue(color); green += GetGValue(color); blue += GetBValue(color);
            }
            pixels[static_cast<size_t>(y) * width + x] =
                (((red + 8) / 16) << 16) | (((green + 8) / 16) << 8) | ((blue + 8) / 16);
        }
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(dc, bounds.left, bounds.top, width, height, 0, 0, 0, height,
            pixels.data(), &info, DIB_RGB_COLORS);
    }

    inline void PaintButton(HWND window, HDC dc, const ControlState& state)
    {
        const auto p = Theme::Colors();
        const auto style = GetWindowLongPtrW(window, GWL_STYLE);
        const auto type = style & BS_TYPEMASK;
        const bool enabled = IsWindowEnabled(window) != FALSE;
        const bool focus = GetFocus() == window;
        const auto bm = SendMessageW(window, BM_GETSTATE, 0, 0);
        const bool pressed = (bm & BST_PUSHED) != 0;
        const auto checkState = SendMessageW(window, BM_GETCHECK, 0, 0) & 3;
        const bool checked = checkState != BST_UNCHECKED;
        RECT r{}; GetClientRect(window, &r);
        SetBkMode(dc, TRANSPARENT);
        auto font = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
        auto oldFont = SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
        const auto background = ControlBackground(window);
        Theme::Fill(dc, r, background);
        SetTextColor(dc, enabled ? p.Text : p.Muted);
        auto text = Text(window);
        UINT format = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
        if (SendMessageW(window, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) format |= DT_HIDEPREFIX;
        if (type == BS_GROUPBOX)
        {
            Theme::Rounded(dc, r, p.Content, Theme::Scale(window, Design::PanelRadius * 2));
            Outline(dc, r, p.Line, Theme::Scale(window, Design::PanelRadius * 2));
            RECT label = r;
            label.left += Theme::Scale(window, 20);
            label.top += Theme::Scale(window, 8);
            label.bottom = label.top + Theme::Scale(window, 20);
            SetTextColor(dc, p.Muted);
            DrawTextW(dc, text.c_str(), -1, &label, format);
        }
        else if (type == BS_AUTOCHECKBOX || type == BS_CHECKBOX ||
            type == BS_AUTO3STATE || type == BS_3STATE ||
            type == BS_AUTORADIOBUTTON || type == BS_RADIOBUTTON)
        {
            const int box = Theme::Scale(window, 16);
            const int inset = Theme::Scale(window, 1);
            const int top = (r.bottom - box) / 2;
            const RECT mark{ inset, top, inset + box, top + box };
            const bool radio = type == BS_AUTORADIOBUTTON || type == BS_RADIOBUTTON;
            const auto fill = checked ? Design::AccentFill(Theme::IsDark()) :
                (pressed ? Design::Pressed(Theme::IsDark()) : state.Hover ? p.Hover : Design::Field(Theme::IsDark()));
            const bool keyboardFocus = focus && !state.MouseFocus &&
                !(SendMessageW(window, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS);
            auto glyph = ChoiceMark::None;
            if (checked)
                glyph = radio ? ChoiceMark::Dot :
                    checkState == BST_INDETERMINATE ? ChoiceMark::Mixed : ChoiceMark::Check;
            const auto ink = enabled ? Design::AccentText(Theme::IsDark()) : p.Muted;
            const auto border = focus ? p.Accent : enabled && checked ? fill : p.Muted;
            PaintRoundedControl(dc, mark, background, enabled ? fill : p.Hover, border,
                radio ? box / 2 : Theme::Scale(window, 3),
                keyboardFocus ? Theme::Scale(window, 2) : 1, 0, glyph, ink);
            r.left = mark.right + Theme::Scale(window, 9);
            DrawTextW(dc, text.c_str(), -1, &r, format);

        }
        else if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON)
        {
            const bool primary = GetDlgCtrlID(window) == IDOK || type == BS_DEFPUSHBUTTON;
            const auto fill = !enabled ? p.Hover : pressed ? Design::Pressed(Theme::IsDark()) :
                state.Hover ? p.Selected : primary ? Design::AccentFill(Theme::IsDark()) : Design::Field(Theme::IsDark());
            PaintRoundedControl(dc, r, background, fill,
                focus ? p.Accent : primary ? fill : p.Line,
                Theme::Scale(window, Design::ControlRadius), focus ? Theme::Scale(window, 2) : 1, 1);
            InflateRect(&r, -Theme::Scale(window, 1), -Theme::Scale(window, 1));
            SetTextColor(dc, !enabled ? p.Muted : primary && !pressed && !state.Hover
                ? Design::AccentText(Theme::IsDark()) : p.Text);
            DrawTextW(dc, text.c_str(), -1, &r, format | DT_CENTER);
        }
        SelectObject(dc, oldFont);
    }

    inline void PaintCombo(HWND window, HDC dc, const ControlState& state)
    {
        const auto p = Theme::Colors();
        RECT r{}; GetClientRect(window, &r);
        COMBOBOXINFO info{ sizeof(info) }; GetComboBoxInfo(window, &info);
        const bool editable = (GetWindowLongPtrW(window, GWL_STYLE) & 3) == CBS_DROPDOWN;
        const bool focused = GetFocus() == window || GetFocus() == info.hwndItem;
        Theme::Fill(dc, r, ControlBackground(window));
        Theme::Rounded(dc, r, Design::Field(Theme::IsDark()), Theme::Scale(window, 16));
        Outline(dc, r, focused ? p.Accent : state.Hover ? p.Muted : p.Line, Theme::Scale(window, 16));
        const int arrow = Theme::Scale(window, 30);
        if (!editable)
        {
            RECT label = r; label.left += Theme::Scale(window, 10); label.right -= arrow;
            auto oldFont = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, IsWindowEnabled(window) ? p.Text : p.Muted);
            auto text = Text(window);
            DrawTextW(dc, text.c_str(), -1, &label, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(dc, oldFont);
        }
        const int x = r.right - arrow / 2, y = r.bottom / 2;
        const int half = Theme::Scale(window, 4);
        Line(dc, x - half, y - half / 2, x, y + half / 2, p.Muted);
        Line(dc, x, y + half / 2, x + half, y - half / 2, p.Muted);
    }

    inline void SetComboHeight(HWND window, int height)
    {
        RECT rect{}; GetWindowRect(window, &rect);
        const int selection = static_cast<int>(SendMessageW(window, CB_GETITEMHEIGHT, static_cast<WPARAM>(-1), 0));
        if (selection == CB_ERR) return;
        const int frame = rect.bottom - rect.top - selection;
        // SetWindowPos resets the native selection field to its font height;
        // this must run afterwards, not before sizing the dropdown.
        SendMessageW(window, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), (std::max)(1, height - frame));
        // A single-line native edit paints from its top edge. Centre the edit
        // itself inside the enlarged field; making it taller does not centre text.
        COMBOBOXINFO info{sizeof(info)};
        if ((GetWindowLongPtrW(window, GWL_STYLE) & 3) == CBS_DROPDOWN &&
            GetComboBoxInfo(window, &info) && info.hwndItem)
        {
            auto dc = GetDC(info.hwndItem);
            auto font = reinterpret_cast<HFONT>(SendMessageW(info.hwndItem, WM_GETFONT, 0, 0));
            auto oldFont = SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
            TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
            SelectObject(dc, oldFont); ReleaseDC(info.hwndItem, dc);
            RECT client{}; GetClientRect(window, &client);
            const int textHeight = (std::min<int>)(info.rcItem.bottom - info.rcItem.top,
                metrics.tmHeight + Theme::Scale(window, 2));
            SetWindowPos(info.hwndItem, nullptr, info.rcItem.left,
                (client.bottom - textHeight) / 2, info.rcItem.right - info.rcItem.left,
                textHeight, SWP_NOZORDER | SWP_NOACTIVATE);
            SendMessageW(info.hwndItem, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                MAKELPARAM(Theme::Scale(window, 8), Theme::Scale(window, 6)));
        }
    }

    inline void PaintComboList(HWND window, HDC dc)
    {
        const auto palette = Theme::Colors();
        RECT client{}; GetClientRect(window, &client);
        Theme::Fill(dc, client, palette.Content);
        const int count = static_cast<int>(SendMessageW(window, LB_GETCOUNT, 0, 0));
        const int first = static_cast<int>(SendMessageW(window, LB_GETTOPINDEX, 0, 0));
        const int selected = static_cast<int>(SendMessageW(window, LB_GETCURSEL, 0, 0));
        auto oldFont = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)));
        SetBkMode(dc, TRANSPARENT);
        for (int index = (std::max)(0, first); index < count; ++index)
        {
            RECT row{};
            if (SendMessageW(window, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&row)) == LB_ERR || row.top >= client.bottom) break;
            if (index == selected)
            {
                RECT highlight = row; InflateRect(&highlight, -Theme::Scale(window, 3), -Theme::Scale(window, 1));
                Theme::Rounded(dc, highlight, palette.Selected, Theme::Scale(window, 12));
            }
            const auto length = SendMessageW(window, LB_GETTEXTLEN, index, 0);
            if (length < 0 || length > 65535) continue;
            std::wstring label(static_cast<size_t>(length) + 1, L'\0');
            SendMessageW(window, LB_GETTEXT, index, reinterpret_cast<LPARAM>(label.data()));
            row.left += Theme::Scale(window, 10); row.right -= Theme::Scale(window, 10);
            SetTextColor(dc, IsWindowEnabled(window) ? palette.Text : palette.Muted);
            DrawTextW(dc, label.c_str(), -1, &row, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        SelectObject(dc, oldFont);
    }

    inline LRESULT CALLBACK ControlProc(HWND window, UINT message, WPARAM wp, LPARAM lp,
        UINT_PTR, DWORD_PTR data)
    {
        auto state = reinterpret_cast<ControlState*>(data);
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(window, ControlProc, ControlId);
            delete state;
            return DefSubclassProc(window, message, wp, lp);
        }
        const bool native = Theme::Current().HighContrast;
        if (message == WM_LBUTTONDOWN) state->MouseFocus = true;
        if (message == WM_SETFOCUS) state->MouseFocus = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (message == WM_KEYDOWN || (message == WM_UPDATEUISTATE &&
            LOWORD(wp) == UIS_CLEAR && (HIWORD(wp) & UISF_HIDEFOCUS))) state->MouseFocus = false;
        if (!native && message == WM_MOUSEMOVE && !state->Hover)
        {
            state->Hover = true;
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, window, 0 };
            TrackMouseEvent(&track); InvalidateRect(window, nullptr, FALSE);
        }
        if (message == WM_MOUSELEAVE)
        { state->Hover = false; InvalidateRect(window, nullptr, FALSE); }
        if (!native && state->Edit && message == WM_NCCALCSIZE)
        {
            const auto result = DefSubclassProc(window, message, wp, lp);
            auto r = wp ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(lp)->rgrc[0] : reinterpret_cast<RECT*>(lp);
            if (!(GetWindowLongPtrW(window, GWL_STYLE) & ES_MULTILINE))
            {
                HDC dc = GetDC(window);
                auto old = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)));
                TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
                SelectObject(dc, old); ReleaseDC(window, dc);
                const int pad = (std::max<int>)(Theme::Scale(window, 2), (r->bottom - r->top - metrics.tmHeight) / 2);
                r->top += pad; r->bottom -= pad;
            }
            r->left += Theme::Scale(window, 10); r->right -= Theme::Scale(window, 8);
            return result;
        }
        if (!native && state->ComboList && message == WM_NCPAINT)
        {
            auto dc = GetWindowDC(window);
            RECT rect{}; GetWindowRect(window, &rect); OffsetRect(&rect, -rect.left, -rect.top);
            RECT client{}; GetClientRect(window, &client); MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&client), 2);
            RECT outer{}; GetWindowRect(window, &outer); OffsetRect(&client, -outer.left, -outer.top);
            ExcludeClipRect(dc, client.left, client.top, client.right, client.bottom);
            Theme::Fill(dc, rect, Theme::Colors().Content);
            Outline(dc, rect, Theme::Colors().Line, Theme::Scale(window, 16));
            ReleaseDC(window, dc); return 0;
        }
        if (!native && state->Edit && message == WM_NCPAINT)
        {
            HDC dc = GetWindowDC(window);
            RECT r{}; GetWindowRect(window, &r); OffsetRect(&r, -r.left, -r.top);
            RECT client{}; GetClientRect(window, &client);
            MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&client), 2);
            RECT outer{}; GetWindowRect(window, &outer);
            OffsetRect(&client, -outer.left, -outer.top);
            ExcludeClipRect(dc, client.left, client.top, client.right, client.bottom);
            const auto p = Theme::Colors();
            Theme::Fill(dc, r, ControlBackground(window));
            Theme::Rounded(dc, r, Design::Field(Theme::IsDark()), Theme::Scale(window, 16));
            Outline(dc, r, GetFocus() == window ? p.Accent : p.Line, Theme::Scale(window, 16));
            ReleaseDC(window, dc); return 0;
        }
        if (!native && state->Combo && (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX))
        {
            auto dc = reinterpret_cast<HDC>(wp);
            SetTextColor(dc, Theme::Colors().Text); SetBkColor(dc, Design::Field(Theme::IsDark()));
            SetDCBrushColor(dc, Design::Field(Theme::IsDark()));
            return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
        }
        if (!native && message == WM_ERASEBKGND && (state->Button || state->Combo || state->ComboList)) return 1;
        if (!native && message == WM_PAINT && (state->Button || state->Combo || state->ComboList))
        {
            PAINTSTRUCT ps{}; auto dc = BeginPaint(window, &ps);
            if (state->Button)
            {
                RECT client{}; GetClientRect(window, &client);
                auto buffer = CreateCompatibleDC(dc);
                auto bitmap = CreateCompatibleBitmap(dc, (std::max)(1L, client.right), (std::max)(1L, client.bottom));
                if (buffer && bitmap)
                {
                    auto previous = SelectObject(buffer, bitmap);
                    PaintButton(window, buffer, *state);
                    BitBlt(dc, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);
                    SelectObject(buffer, previous);
                }
                else PaintButton(window, dc, *state);
                if (bitmap) DeleteObject(bitmap);
                if (buffer) DeleteDC(buffer);
            }
            else if (state->ComboList) PaintComboList(window, dc);
            else PaintCombo(window, dc, *state);
            EndPaint(window, &ps); return 0;
        }
        if (!native && message == WM_PRINTCLIENT && (state->Combo || state->Button || state->ComboList))
        {
            if (state->Combo) PaintCombo(window, reinterpret_cast<HDC>(wp), *state);
            else if (state->ComboList) PaintComboList(window, reinterpret_cast<HDC>(wp));
            else PaintButton(window, reinterpret_cast<HDC>(wp), *state);
            return 0;
        }
        if (!native && state->Button && message == WM_NCPAINT) return 0;
        // Dialog default-button changes request immediate native painting.
        // Preserve the style/state transition, then paint the entire themed
        // surface once instead of leaving a classic border underneath it.
        const bool restyle = !native && state->Button && message == BM_SETSTYLE;
        auto result = DefSubclassProc(window, message, wp, restyle ? FALSE : lp);
        DWORD_PTR alive{};
        if (!GetWindowSubclass(window, ControlProc, ControlId, &alive) || alive != data) return result;
        if (message == WM_LBUTTONDOWN) state->MouseFocus = true;
        if (state->ComboList && !native && message == WM_WINDOWPOSCHANGED)
        {
            RECT rect{}; GetWindowRect(window, &rect);
            const int width = rect.right - rect.left, height = rect.bottom - rect.top;
            if (width > 0 && height > 0 && (state->RegionWidth != width || state->RegionHeight != height))
            {
                state->RegionWidth = width; state->RegionHeight = height;
                auto region = CreateRoundRectRgn(0, 0, width + 1, height + 1, Theme::Scale(window, 16), Theme::Scale(window, 16));
                if (!SetWindowRgn(window, region, TRUE)) DeleteObject(region);
            }
        }
        if (state->ComboList && (message == WM_MOUSEMOVE || message == WM_VSCROLL ||
            message == LB_SETCURSEL || message == WM_SHOWWINDOW || message == WM_CHAR))
            RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
        if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_ENABLE ||
            message == BM_SETSTYLE || message == BM_SETCHECK || message == BM_SETSTATE || message == WM_LBUTTONDOWN ||
            message == WM_LBUTTONUP || message == WM_KEYDOWN || message == WM_KEYUP ||
            message == CB_SETCURSEL || message == WM_SETTEXT || message == WM_UPDATEUISTATE)
            RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | (state->Edit ? RDW_FRAME : 0) | ((state->Button || state->ComboList) ? RDW_UPDATENOW : 0));
        return result;
    }

    inline void AttachControl(HWND window)
    {
        DWORD_PTR existing{};
        if (GetWindowSubclass(window, ControlProc, ControlId, &existing)) return;
        auto state = new ControlState;
        state->ComboList = ClassIs(window, L"ComboLBox");
        state->Button = ClassIs(window, L"Button");
        state->Combo = ClassIs(window, L"ComboBox");
        state->Edit = ClassIs(window, L"Edit") && !ClassIs(GetParent(window), L"ComboBox");
        if (!state->Button && !state->Combo && !state->Edit && !state->ComboList) { delete state; return; }
        if (state->Button)
        {
            SetWindowLongPtrW(window, GWL_STYLE, GetWindowLongPtrW(window, GWL_STYLE) | WS_CLIPSIBLINGS);
            auto type = GetWindowLongPtrW(window, GWL_STYLE) & BS_TYPEMASK;
            if (type == BS_GROUPBOX) SetWindowPos(window, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            if ((type > BS_GROUPBOX && type != BS_AUTORADIOBUTTON) || type == BS_OWNERDRAW || (GetWindowLongPtrW(window, GWL_STYLE) & (BS_ICON | BS_BITMAP)))
            { delete state; return; }
        }
        SetWindowSubclass(window, ControlProc, ControlId, reinterpret_cast<DWORD_PTR>(state));
        if (state->Edit)
        {
            SetWindowLongPtrW(window, GWL_EXSTYLE, GetWindowLongPtrW(window, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
            SetWindowLongPtrW(window, GWL_STYLE, GetWindowLongPtrW(window, GWL_STYLE) & ~WS_BORDER);
        }
        if (state->Combo)
        {
            if (!(GetWindowLongPtrW(GetParent(window), GWL_STYLE) & WS_CHILD))
            {
                SendMessageW(window, CB_SETITEMHEIGHT, 0, Theme::Scale(window, 24));
                SetComboHeight(window, Theme::Scale(window, GetPropW(GetAncestor(window, GA_ROOT), CustomLayoutProperty) ? 36 : 28));
            }
            COMBOBOXINFO info{ sizeof(info) };
            if (GetComboBoxInfo(window, &info) && info.hwndItem)
                SendMessageW(info.hwndItem, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 6));
        }
    }

    inline void Refresh(HWND window, Surface& surface)
    {
        Theme::Reload();
        if (surface.Background) DeleteObject(surface.Background);
        if (surface.FieldBrush) DeleteObject(surface.FieldBrush);
        surface.Background = CreateSolidBrush(Theme::Colors().Chrome);
        surface.FieldBrush = CreateSolidBrush(Design::Field(Theme::IsDark()));
        if (surface.Font) DeleteObject(surface.Font);
        surface.Font = CreateFontW(-Theme::Scale(window, 13), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, Design::FontFamily);
        if (!surface.NativeLayout && !(GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD))
            SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(surface.Font), FALSE);
        if (!surface.NativeLayout && !(GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD)) EnumChildWindows(window, [](HWND child, LPARAM font) -> BOOL
        {
            if ((GetWindowLongPtrW(child, GWL_STYLE) & WS_CHILD) && ClassIs(child, L"#32770")) return TRUE;
            for (auto parent = GetParent(child); parent; parent = GetParent(parent))
                if ((GetWindowLongPtrW(parent, GWL_STYLE) & WS_CHILD) && ClassIs(parent, L"#32770")) return TRUE;
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), FALSE);
            return TRUE;
        }, reinterpret_cast<LPARAM>(surface.Font));
        const auto p = Theme::Colors();
        const BOOL dark = Theme::IsDark();
        const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &p.Chrome, sizeof(p.Chrome));
        DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &p.Text, sizeof(p.Text));
        DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &p.Line, sizeof(p.Line));
        DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
        const DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_NONE;
        DwmSetWindowAttribute(window, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        MARGINS margins{}; DwmExtendFrameIntoClientArea(window, &margins);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    }

    inline void ClipToViewport(HWND child, HWND parent, int bottom)
    {
        RECT actual{}; GetWindowRect(child, &actual);
        MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&actual), 2);
        RECT viewport{}; GetClientRect(parent, &viewport); viewport.bottom = bottom;
        RECT clip{};
        if (!IntersectRect(&clip, &actual, &viewport)) SetRectEmpty(&clip);
        else OffsetRect(&clip, -actual.left, -actual.top);
        auto region = CreateRectRgnIndirect(&clip);
        if (!SetWindowRgn(child, region, TRUE)) DeleteObject(region);
    }

    inline void Layout(HWND window, Surface& surface)
    {
        if (surface.Custom || surface.LayingOut) return;
        surface.LayingOut = true;
        RECT client{}; GetClientRect(window, &client);
        const int footer = Theme::Scale(window, 64);
        const int height = (std::max<int>)(1, client.bottom - footer);
        const int content = Theme::Scale(window, surface.Height - 64);
        const int range = (std::max<int>)(0, content - height);
        surface.Scroll = (std::min)(surface.Scroll, range);
        SCROLLINFO scroll{ sizeof(scroll), SIF_RANGE | SIF_PAGE | SIF_POS };
        scroll.nMax = content - 1; scroll.nPage = height; scroll.nPos = surface.Scroll;
        SetScrollInfo(window, SB_VERT, &scroll, TRUE);
        const int contentWidth = Theme::Scale(window, surface.Width);
        surface.HorizontalScroll = (std::clamp<int>)(surface.HorizontalScroll, 0, (std::max<int>)(0, contentWidth - client.right));
        SCROLLINFO horizontal{sizeof(horizontal), SIF_RANGE | SIF_PAGE | SIF_POS};
        horizontal.nMax = contentWidth - 1; horizontal.nPage = client.right; horizontal.nPos = surface.HorizontalScroll;
        SetScrollInfo(window, SB_HORZ, &horizontal, TRUE);
        for (const auto& placement : surface.Children)
        {
            RECT r = placement.Bounds;
            r.left = Theme::Scale(window, r.left); r.right = Theme::Scale(window, r.right);
            r.top = Theme::Scale(window, r.top); r.bottom = Theme::Scale(window, r.bottom);
            if (placement.Footer)
                OffsetRect(&r, client.right - Theme::Scale(window, surface.Width),
                    client.bottom - Theme::Scale(window, surface.Height));
            else OffsetRect(&r, -surface.HorizontalScroll, -surface.Scroll);
            SetWindowPos(placement.Window, nullptr, r.left, r.top, r.right - r.left,
                placement.Combo ? (std::max<int>)(Theme::Scale(window, 220), r.bottom - r.top) : r.bottom - r.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            if (placement.Combo) SetComboHeight(placement.Window, Theme::Scale(window, Design::CompactFieldHeight));
            if (!placement.Footer) ClipToViewport(placement.Window, window, height);
        }
        surface.LayingOut = false;
    }

    inline LRESULT CALLBACK SurfaceProc(HWND window, UINT message, WPARAM wp, LPARAM lp,
        UINT_PTR, DWORD_PTR data)
    {
        auto surface = reinterpret_cast<Surface*>(data);
        if (message == WM_NCDESTROY)
        {
            RemovePropW(window, SurfaceProperty);
            RemoveWindowSubclass(window, SurfaceProc, SurfaceId);
            if (surface->Font) DeleteObject(surface->Font);
            if (surface->Background) DeleteObject(surface->Background);
            if (surface->FieldBrush) DeleteObject(surface->FieldBrush);
            delete surface;
            return DefSubclassProc(window, message, wp, lp);
        }
        if (message == WM_COMMAND && HIWORD(wp) == CBN_DROPDOWN && lp)
        {
            COMBOBOXINFO combo{sizeof(combo)};
            if (GetComboBoxInfo(reinterpret_cast<HWND>(lp), &combo) && combo.hwndList) AttachControl(combo.hwndList);
        }
        if (!Theme::Current().HighContrast)
        {
            if (message == WM_ERASEBKGND)
            {
                RECT r{}; GetClientRect(window, &r); FillRect(reinterpret_cast<HDC>(wp), &r, surface->Background);
                return 1;
            }
            if (message >= WM_CTLCOLORMSGBOX && message <= WM_CTLCOLORSTATIC)
            {
                auto dc = reinterpret_cast<HDC>(wp);
                const bool field = message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX;
                SetTextColor(dc, IsWindowEnabled(reinterpret_cast<HWND>(lp)) ? Theme::Colors().Text : Theme::Colors().Muted);
                SetBkColor(dc, field ? Design::Field(Theme::IsDark()) : ControlBackground(reinterpret_cast<HWND>(lp)));
                SetBkMode(dc, TRANSPARENT);
                if (!field)
                {
                    SetDCBrushColor(dc, ControlBackground(reinterpret_cast<HWND>(lp)));
                    return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
                }
                return reinterpret_cast<LRESULT>(surface->FieldBrush);
            }
        }
        if (message == WM_HSCROLL && !surface->Custom)
        {
            switch (LOWORD(wp))
            {
            case SB_LINELEFT: surface->HorizontalScroll -= Theme::Scale(window, 32); break;
            case SB_LINERIGHT: surface->HorizontalScroll += Theme::Scale(window, 32); break;
            case SB_PAGELEFT: surface->HorizontalScroll -= Theme::Scale(window, 160); break;
            case SB_PAGERIGHT: surface->HorizontalScroll += Theme::Scale(window, 160); break;
            case SB_THUMBTRACK:
            { SCROLLINFO info{sizeof(info), SIF_TRACKPOS}; GetScrollInfo(window, SB_HORZ, &info); surface->HorizontalScroll = info.nTrackPos; break; }
            default: break;
            }
            Layout(window, *surface); InvalidateRect(window, nullptr, TRUE); return 0;
        }
        if (message == WM_VSCROLL && !surface->Custom)
        {
            int position = surface->Scroll;
            RECT r{}; GetClientRect(window, &r);
            switch (LOWORD(wp))
            {
            case SB_LINEUP: position -= Theme::Scale(window, 32); break;
            case SB_LINEDOWN: position += Theme::Scale(window, 32); break;
            case SB_PAGEUP: position -= r.bottom / 2; break;
            case SB_PAGEDOWN: position += r.bottom / 2; break;
            case SB_THUMBTRACK:
            { SCROLLINFO si{ sizeof(si), SIF_TRACKPOS }; GetScrollInfo(window, SB_VERT, &si); position = si.nTrackPos; break; }
            default: break;
            }
            surface->Scroll = (std::max<int>)(0, position); Layout(window, *surface); InvalidateRect(window, nullptr, TRUE); return 0;
        }
        if (message == WM_MOUSEWHEEL && !surface->Custom)
        {
            const int distance = GET_WHEEL_DELTA_WPARAM(wp) * Theme::Scale(window, 48) / WHEEL_DELTA;
            if (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) surface->HorizontalScroll -= distance;
            else surface->Scroll = (std::max<int>)(0, surface->Scroll - distance);
            Layout(window, *surface); InvalidateRect(window, nullptr, TRUE); return 0;
        }
        auto result = DefSubclassProc(window, message, wp, lp);
        DWORD_PTR alive{};
        if (!GetWindowSubclass(window, SurfaceProc, SurfaceId, &alive) || alive != data) return result;
        if (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED)
        { Refresh(window, *surface); PostMessageW(window, ReflowMessage, 0, 0); }
        if (message == DpiChangedMessage)
        {
            const auto r = reinterpret_cast<RECT*>(lp);
            SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            Refresh(window, *surface); Layout(window, *surface);
        }
        if (message == WM_SIZE || message == ReflowMessage) Layout(window, *surface);
        if (message == WM_COMMAND && !surface->Custom &&
            (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == BN_SETFOCUS || HIWORD(wp) == CBN_SETFOCUS))
        {
            RECT focused{}; GetWindowRect(reinterpret_cast<HWND>(lp), &focused);
            MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&focused), 2);
            RECT client{}; GetClientRect(window, &client);
            const int bottom = client.bottom - Theme::Scale(window, 72);
            if (focused.top < Theme::Scale(window, 12)) surface->Scroll += focused.top - Theme::Scale(window, 12);
            else if (focused.bottom > bottom) surface->Scroll += focused.bottom - bottom;
            if (focused.left < Theme::Scale(window, 12)) surface->HorizontalScroll += focused.left - Theme::Scale(window, 12);
            else if (focused.right > client.right - Theme::Scale(window, 12)) surface->HorizontalScroll += focused.right - client.right + Theme::Scale(window, 12);
            surface->Scroll = (std::max<int>)(0, surface->Scroll); Layout(window, *surface);
        }
        return result;
    }

    inline void Attach(HWND window)
    {
        if (!ClassIs(window, L"#32770") || GetPropW(window, SurfaceProperty) ||
            GetPropW(window, L"MiaoZip.SystemDialogTheme") || Theme::Current().HighContrast) return;
        // Property sheets contain child dialogs; their existing layout owns geometry.
        auto surface = new Surface;
        surface->NativeLayout = NativeLayoutDepth != 0;
        surface->Custom = surface->NativeLayout || GetPropW(window, CustomLayoutProperty) != nullptr ||
            (GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD) != 0 ||
            FindWindowExW(window, nullptr, WC_TABCONTROLW, nullptr) != nullptr;
        SetPropW(window, SurfaceProperty, surface);
        SetWindowLongPtrW(window, GWL_STYLE, GetWindowLongPtrW(window, GWL_STYLE) | WS_CLIPCHILDREN);
        SetWindowSubclass(window, SurfaceProc, SurfaceId, reinterpret_cast<DWORD_PTR>(surface));
        RECT original{}; GetClientRect(window, &original);
        const UINT dpi = (std::max<UINT>)(96u, Theme::Dpi(window));
        const int oldWidth = MulDiv(original.right, 96, dpi);
        const int oldHeight = MulDiv(original.bottom, 96, dpi);
        surface->Width = MulDiv(oldWidth, 118, 100) + 24;
        surface->Height = MulDiv(oldHeight, 135, 100) + 24;
        for (auto child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        {
            RECT r{}; GetWindowRect(child, &r); MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&r), 2);
            r.left = MulDiv(r.left, 96, dpi); r.top = MulDiv(r.top, 96, dpi);
            r.right = MulDiv(r.right, 96, dpi); r.bottom = MulDiv(r.bottom, 96, dpi);
            const bool button = ClassIs(child, L"Button");
            const auto type = GetWindowLongPtrW(child, GWL_STYLE) & BS_TYPEMASK;
            const bool push = button && (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON);
            const bool footer = push && r.top >= oldHeight - 44;
            const bool combo = ClassIs(child, L"ComboBox");
            r.left = MulDiv(r.left, 118, 100) + 12; r.right = MulDiv(r.right, 118, 100) + 12;
            r.top = MulDiv(r.top, 135, 100) + 12; r.bottom = MulDiv(r.bottom, 135, 100) + 12;
            if (combo) r.bottom = r.top + Design::CompactFieldHeight;
            else if (push) r.bottom = r.top + Design::ButtonHeight;
            else if (ClassIs(child, L"Edit") && !(GetWindowLongPtrW(child, GWL_STYLE) & ES_MULTILINE)) r.bottom = r.top + Design::CompactFieldHeight;
            else if (button && type != BS_GROUPBOX) r.bottom = (std::max<int>)(r.bottom, r.top + 22);
            if (footer) { r.top = surface->Height - 48; r.bottom = r.top + 36; }
            surface->Children.push_back({ child, r, footer, combo });
        }
        Refresh(window, *surface);
        if (!surface->NativeLayout)
            EnumChildWindows(window, [](HWND child, LPARAM) -> BOOL { AttachControl(child); return TRUE; }, 0);
        if (!surface->Custom)
        {
            RECT size{ 0, 0, Theme::Scale(window, surface->Width), Theme::Scale(window, surface->Height) };
            Theme::AdjustForDpi(&size, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)),
                FALSE, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)), dpi);
            RECT work{}; MONITORINFO monitor{ sizeof(monitor) };
            GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor); work = monitor.rcWork;
            const int width = (std::min)(static_cast<int>(size.right - size.left), static_cast<int>(work.right - work.left - Theme::Scale(window, 24)));
            const int height = (std::min)(static_cast<int>(size.bottom - size.top), static_cast<int>(work.bottom - work.top - Theme::Scale(window, 24)));
            SetWindowPos(window, nullptr, work.left + (work.right - work.left - width) / 2,
                work.top + (work.bottom - work.top - height) / 2, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            Layout(window, *surface);
        }
    }
}
