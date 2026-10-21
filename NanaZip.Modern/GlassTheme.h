#pragma once
#include "../MiaoZip.Features/Theme.h"
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Media.h>

// Keep XAML islands opaque when the optional system material is disabled.
inline void UpdateMiaoZipMaterial()
{
    using namespace winrt::Windows::UI::Xaml;
    MiaoZip::Theme::Reload();
    const bool glass = MiaoZip::Theme::Current().Glass;
    const auto application = Application::Current();
    if (!application) return;
    for (const auto& resources : application.Resources().MergedDictionaries())
    {
        for (const auto name : { L"Default", L"Light" })
        {
            const auto key = winrt::box_value(name);
            if (!resources.ThemeDictionaries().HasKey(key)) continue;
            const auto theme = resources.ThemeDictionaries().Lookup(key).try_as<ResourceDictionary>();
            const auto chromeKey = winrt::box_value(L"MiaoChromeBrush");
            if (!theme || !theme.HasKey(chromeKey)) continue;
            const auto chrome = theme.Lookup(chromeKey).try_as<Media::SolidColorBrush>();
            const auto host = theme.Lookup(winrt::box_value(L"MiaoAddressHostBrush")).try_as<Media::SolidColorBrush>();
            if (!chrome || !host) continue;
            const bool light = std::wcscmp(name, L"Light") == 0;
            const winrt::Windows::UI::Color solid = light
                ? winrt::Windows::UI::Color{ 255, 243, 247, 253 }
                : winrt::Windows::UI::Color{ 255, 22, 26, 33 };
            auto tint = solid;
            tint.A = glass ? (light ? 0x70 : 0xC0) : 255;
            if (glass && !light) { tint.R = 26; tint.G = 31; tint.B = 39; }
            chrome.Color(tint);
            auto address = solid; address.A = glass ? 0 : 255;
            host.Color(address);
        }
    }
}

inline void ApplyMiaoZipXamlTheme(winrt::Windows::UI::Xaml::FrameworkElement const& element)
{
    UpdateMiaoZipMaterial();
    element.RequestedTheme(MiaoZip::Theme::Current().HighContrast
        ? winrt::Windows::UI::Xaml::ElementTheme::Default
        : MiaoZip::Theme::IsDark() ? winrt::Windows::UI::Xaml::ElementTheme::Dark
        : winrt::Windows::UI::Xaml::ElementTheme::Light);
}
