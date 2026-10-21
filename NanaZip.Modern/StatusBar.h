#pragma once

#include "StatusBar.g.h"
#include "ControlMacros.h"
#include "GlassTheme.h"

namespace winrt::NanaZip::Modern::implementation
{
    struct StatusBar : StatusBarT<StatusBar>
    {
        StatusBar() { ApplyMiaoZipXamlTheme(*this); }

        DEPENDENCY_PROPERTY_HEADER(Text1, winrt::hstring);
        DEPENDENCY_PROPERTY_HEADER(Text2, winrt::hstring);
        DEPENDENCY_PROPERTY_HEADER(Text3, winrt::hstring);
        DEPENDENCY_PROPERTY_HEADER(Text4, winrt::hstring);
    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct StatusBar : StatusBarT<StatusBar, implementation::StatusBar>
    { };
}
