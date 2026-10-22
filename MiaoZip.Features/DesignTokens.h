// MiaoZip design values, extracted from Theme.h and GlassTheme.xaml.
#pragma once
#include <Windows.h>

namespace MiaoZip::Design
{
    inline constexpr int ControlRadius = 8;
    inline constexpr int PanelRadius = 12;
    inline constexpr int FieldHeight = 36;
    inline constexpr int CompactFieldHeight = 28;
    inline constexpr int ButtonHeight = 36;
    inline constexpr int Spacing = 12;
    inline constexpr int PagePadding = 24;
    inline constexpr wchar_t FontFamily[] = L"Microsoft YaHei UI";

    struct Palette
    {
        COLORREF Chrome, Sidebar, Content, Text, Muted, Line, Hover, Selected, Accent;
    };

    inline Palette Colors(bool dark, bool highContrast)
    {
        if (highContrast)
            return { GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOW),
                GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOWTEXT),
                GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT),
                GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHT),
                GetSysColor(COLOR_HIGHLIGHTTEXT) };
        if (dark)
            return { RGB(22,26,33), RGB(24,29,37), RGB(26,31,39),
                RGB(231,235,242), RGB(166,175,191), RGB(45,52,64),
                RGB(36,45,58), RGB(38,61,88), RGB(155,178,204) };
        return { RGB(243,247,253), RGB(231,238,247), RGB(255,255,255),
            RGB(32,45,64), RGB(104,122,145), RGB(222,230,241),
            RGB(243,247,253), RGB(205,221,238), RGB(71,100,134) };
    }

    inline COLORREF AccentFill(bool dark)
    { return dark ? RGB(165,186,212) : RGB(93,120,151); }
    inline COLORREF AccentText(bool dark)
    { return dark ? RGB(16,36,61) : RGB(255,255,255); }
    inline COLORREF Pressed(bool dark)
    { return dark ? RGB(43,53,68) : RGB(230,238,249); }
    inline COLORREF Field(bool dark)
    { return dark ? RGB(21,26,34) : RGB(251,253,255); }
}
