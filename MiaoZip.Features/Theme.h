// MiaoZip appearance preferences shared by the native and XAML surfaces.
#pragma once
#include <Windows.h>
#include <dwmapi.h>
#include "DesignTokens.h"
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace MiaoZip::Theme
{
    enum class Mode : DWORD { System = 0, Light = 1, Dark = 2 };
    struct Preferences
    {
        Mode Appearance = Mode::System;
        bool Glass = true;
        bool Dark = false;
        bool HighContrast = false;
    };

    inline DWORD ReadNumber(HKEY root, const wchar_t* key,
        const wchar_t* name, DWORD fallback)
    {
        DWORD value = fallback, size = sizeof(value);
        return RegGetValueW(root, key, name, RRF_RT_REG_DWORD,
            nullptr, &value, &size) == ERROR_SUCCESS ? value : fallback;
    }

    inline Preferences Read()
    {
        Preferences result;
        const DWORD mode = ReadNumber(HKEY_CURRENT_USER,
            L"Software\\MiaoZip\\Appearance", L"Mode", 0);
        result.Appearance = mode <= 2 ? static_cast<Mode>(mode) : Mode::System;
        result.Glass = ReadNumber(HKEY_CURRENT_USER,
            L"Software\\MiaoZip\\Appearance", L"Transparency", 1) != 0;
        HIGHCONTRASTW contrast{ sizeof(contrast) };
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0))
            result.HighContrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
        result.Dark = result.Appearance == Mode::Dark ||
            (result.Appearance == Mode::System && ReadNumber(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                L"AppsUseLightTheme", 1) == 0);
        if (result.HighContrast) { result.Dark = false; result.Glass = false; }
        return result;
    }

    inline Preferences& Current()
    {
        static thread_local Preferences result = Read();
        return result;
    }
    inline void Reload() { Current() = Read(); }
    inline bool IsDark() { return Current().Dark; }

    inline bool Save(Mode mode, bool glass)
    {
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MiaoZip\\Appearance",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
            return false;
        const DWORD appearance = static_cast<DWORD>(mode), transparency = glass ? 1 : 0;
        const auto first = RegSetValueExW(key, L"Mode", 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&appearance), sizeof(appearance));
        const auto second = RegSetValueExW(key, L"Transparency", 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&transparency), sizeof(transparency));
        RegCloseKey(key);
        Reload();
        return first == ERROR_SUCCESS && second == ERROR_SUCCESS;
    }

    using Palette = Design::Palette;
    inline Palette Colors()
    {
        return Design::Colors(IsDark(), Current().HighContrast);
    }

    // The inherited 7-Zip translation units still set an older WINVER.
    // Resolve the per-monitor APIs without changing the archive engine's macros.
    inline UINT Dpi(HWND window)
    {
        using Function = UINT (WINAPI*)(HWND);
        static const auto function = reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
        if (function) { const UINT dpi = function(window); if (dpi) return dpi; }
        HDC dc = GetDC(window); const UINT dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(window, dc);
        return dpi ? dpi : 96;
    }
    inline BOOL AdjustForDpi(RECT* rect, DWORD style, BOOL menu, DWORD extendedStyle, UINT dpi)
    {
        using Function = BOOL (WINAPI*)(LPRECT,DWORD,BOOL,DWORD,UINT);
        static const auto function = reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi"));
        return function ? function(rect,style,menu,extendedStyle,dpi) : AdjustWindowRectEx(rect,style,menu,extendedStyle);
    }

    inline bool ApplyBackdrop(HWND window)
    {
        Reload();
        const auto settings = Current();
        const BOOL dark = settings.Dark;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        const auto palette = Colors();
        DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &palette.Text, sizeof(palette.Text));
        const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        const DWM_SYSTEMBACKDROP_TYPE backdrop = settings.Glass
            ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
        const bool active = SUCCEEDED(DwmSetWindowAttribute(window,
            DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop))) && settings.Glass;
        const COLORREF caption = active ? static_cast<COLORREF>(-2) : palette.Chrome;
        DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
        SetPropW(window, L"MiaoZip.GlassActive", active ? reinterpret_cast<HANDLE>(1) : nullptr);
        MARGINS margins{};
        if (active)
        {
            margins.cyTopHeight = MulDiv(152, Dpi(window), 96);
            margins.cyBottomHeight = MulDiv(40, Dpi(window), 96);
        }
        DwmExtendFrameIntoClientArea(window, &margins);
        return active;
    }

    inline int Scale(HWND window, int value)
    {
        const UINT dpi = Dpi(window);
        return MulDiv(value, dpi ? dpi : 96, 96);
    }

    inline void Fill(HDC dc, const RECT& rectangle, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rectangle, brush);
        DeleteObject(brush);
    }

    inline void Rounded(HDC dc, RECT rectangle, COLORREF color, int radius)
    {
        HBRUSH brush = CreateSolidBrush(color);
        auto oldBrush = SelectObject(dc, brush);
        auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, rectangle.left, rectangle.top, rectangle.right,
            rectangle.bottom, radius, radius);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
    }
}
