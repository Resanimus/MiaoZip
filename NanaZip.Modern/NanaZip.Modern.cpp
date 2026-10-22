/*
 * PROJECT:    NanaZip.Modern
 * FILE:       NanaZip.Modern.cpp
 * PURPOSE:    Implementation for NanaZip Modern Experience
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "pch.h"

#include "NanaZip.Modern.h"

#include <Mile.Helpers.h>
#include <Mile.Xaml.h>

#include "App.h"
#include "SponsorPage.h"
#include "AboutPage.h"
#include "InformationPage.h"
#include "ProgressPage.h"
#include "CopyLocationPage.h"
#include "../MiaoZip.Features/Theme.h"

#pragma comment(lib, "comctl32.lib")

#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>

#include <mutex>
#include <map>

namespace winrt
{
    using Windows::ApplicationModel::Resources::Core::ResourceManager;
    using Windows::ApplicationModel::Resources::Core::ResourceMap;
}

namespace
{
    static winrt::ResourceMap GetMainResourceMap()
    {
        static winrt::ResourceMap CachedResult = ([]() -> winrt::ResourceMap
        {
            try
            {
                return winrt::ResourceManager::Current().MainResourceMap();
            }
            catch (...)
            {
                // Do nothing.
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static std::mutex g_CachedLanguageStringResourcesMutex;
    static std::map<UINT32, winrt::hstring> g_CachedLanguageStringResources;
}

EXTERN_C LPCWSTR WINAPI K7ModernGetLegacyStringResource(
    _In_ UINT32 ResourceId)
{
    {
        std::lock_guard Lock(g_CachedLanguageStringResourcesMutex);
        auto Iterator = g_CachedLanguageStringResources.find(ResourceId);
        if (g_CachedLanguageStringResources.end() != Iterator)
        {
            return Iterator->second.c_str();
        }
    }

    static winrt::ResourceMap LegacyResourceMap = ([]() -> winrt::ResourceMap
    {
        winrt::ResourceMap MainResourceMap = ::GetMainResourceMap();
        if (MainResourceMap)
        {
            return MainResourceMap.GetSubtree(L"Legacy");
        }
        return nullptr;
    }());
    if (!LegacyResourceMap)
    {
        return nullptr;
    }

    winrt::hstring ResourceName = L"Resource" + winrt::to_hstring(ResourceId);
    if (!LegacyResourceMap.HasKey(ResourceName))
    {
        return nullptr;
    }

    winrt::hstring Content = LegacyResourceMap.Lookup(
        ResourceName).Candidates().GetAt(0).ValueAsString();
    std::lock_guard Lock(g_CachedLanguageStringResourcesMutex);
    auto Iterator = g_CachedLanguageStringResources.emplace(
        ResourceId,
        std::move(Content));
    return Iterator.first->second.c_str();
}

namespace
{
    static winrt::NanaZip::Modern::App g_AppInstance = nullptr;
}

EXTERN_C BOOL WINAPI K7ModernAvailable()
{
    return nullptr != g_AppInstance;
}

EXTERN_C HRESULT WINAPI K7ModernInitialize()
{
    if (g_AppInstance)
    {
        return S_OK;
    }
    if (!::GetMainResourceMap())
    {
        // NanaZip.Modern requires resources.pri to get XAML resources.
        return E_NOINTERFACE;
    }
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        using Implementation = winrt::NanaZip::Modern::implementation::App;
        g_AppInstance = winrt::make<Implementation>();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
    return S_OK;
}

EXTERN_C HRESULT WINAPI K7ModernUninitialize()
{
    if (!g_AppInstance)
    {
        return S_OK;
    }
    try
    {
        g_AppInstance.Close();
        g_AppInstance = nullptr;
        winrt::uninit_apartment();
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
    return S_OK;
}

namespace winrt
{
    using Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
}

namespace
{
    HWND K7ModernCreateXamlWindow(
        _In_opt_ HWND ParentWindowHandle,
        _In_ DWORD ExtendedWindowStyle,
        _In_ DWORD WindowStyle)
    {
        HWND WindowHandle = ::CreateWindowExW(
            ExtendedWindowStyle,
            L"Mile.Xaml.ContentWindow",
            nullptr,
            WindowStyle,
            CW_USEDEFAULT,
            0,
            CW_USEDEFAULT,
            0,
            ParentWindowHandle,
            nullptr,
            nullptr,
            nullptr);
        if (!WindowHandle)
        {
            return nullptr;
        }
        if (!::SetWindowSubclass(
            WindowHandle,
            [](
                _In_ HWND hWnd,
                _In_ UINT uMsg,
                _In_ WPARAM wParam,
                _In_ LPARAM lParam,
                _In_ UINT_PTR uIdSubclass,
                _In_ DWORD_PTR dwRefData) -> LRESULT
        {
            UNREFERENCED_PARAMETER(uIdSubclass);
            UNREFERENCED_PARAMETER(dwRefData);

            switch (uMsg)
            {
            case WM_GETMINMAXINFO:
            {
                auto info = reinterpret_cast<MINMAXINFO*>(lParam);
                MONITORINFO monitor{sizeof(monitor)};
                ::GetMonitorInfoW(::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
                info->ptMinTrackSize = {
                    (std::min<LONG>)(MiaoZip::Theme::Scale(hWnd, 480), monitor.rcWork.right - monitor.rcWork.left - 24),
                    (std::min<LONG>)(MiaoZip::Theme::Scale(hWnd, 320), monitor.rcWork.bottom - monitor.rcWork.top - 24)};
                return 0;
            }
            case WM_DPICHANGED:
            {
                auto rect = reinterpret_cast<RECT*>(lParam);
                ::SetWindowPos(hWnd, nullptr, rect->left, rect->top, rect->right - rect->left,
                    rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
                break;
            }
            case WM_CLOSE:
            {
                HWND ParentWindow = ::GetWindow(hWnd, GW_OWNER);
                if (ParentWindow)
                {
                    ::EnableWindow(ParentWindow, TRUE);
                }
                break;
            }
            default:
                break;
            }

            return ::DefSubclassProc(
                hWnd,
                uMsg,
                wParam,
                lParam);
        },
            0,
            0))
        {
            ::DestroyWindow(WindowHandle);
            return nullptr;
        }
        MiaoZip::Theme::Reload();
        const auto palette = MiaoZip::Theme::Colors();
        const BOOL dark = MiaoZip::Theme::IsDark();
        const DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_NONE;
        const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
        ::DwmSetWindowAttribute(WindowHandle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        ::DwmSetWindowAttribute(WindowHandle, DWMWA_CAPTION_COLOR, &palette.Chrome, sizeof(palette.Chrome));
        ::DwmSetWindowAttribute(WindowHandle, DWMWA_TEXT_COLOR, &palette.Text, sizeof(palette.Text));
        ::DwmSetWindowAttribute(WindowHandle, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        ::DwmSetWindowAttribute(WindowHandle, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        MARGINS margins{}; ::DwmExtendFrameIntoClientArea(WindowHandle, &margins);
        return WindowHandle;
    }

    HWND K7ModernCreateXamlDialog(HWND ParentWindowHandle)
    {
        return K7ModernCreateXamlWindow(ParentWindowHandle, WS_EX_DLGMODALFRAME,
            WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
    }

    int K7ModernShowXamlWindow(
        _In_opt_ HWND WindowHandle,
        _In_ int Width,
        _In_ int Height,
        _In_ HWND ParentWindowHandle)
    {
        if (!WindowHandle)
        {
            return -1;
        }

        UINT DpiValue = ::GetDpiForWindow(WindowHandle);

        int ScaledWidth = ::MulDiv(Width, DpiValue, USER_DEFAULT_SCREEN_DPI);
        int ScaledHeight = ::MulDiv(Height, DpiValue, USER_DEFAULT_SCREEN_DPI);

        RECT ParentRect = {};
        if (ParentWindowHandle)
        {
            ::GetWindowRect(ParentWindowHandle, &ParentRect);
        }
        else
        {
            HMONITOR MonitorHandle = ::MonitorFromWindow(
                WindowHandle,
                MONITOR_DEFAULTTONEAREST);
            if (MonitorHandle)
            {
                MONITORINFO MonitorInfo;
                MonitorInfo.cbSize = sizeof(MONITORINFO);
                if (::GetMonitorInfoW(MonitorHandle, &MonitorInfo))
                {
                    ParentRect = MonitorInfo.rcWork;
                }
            }
        }

        MONITORINFO monitor{sizeof(monitor)};
        ::GetMonitorInfoW(::MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST), &monitor);
        const RECT work = monitor.rcWork;
        ScaledWidth = (std::min)(ScaledWidth, static_cast<int>(work.right - work.left - 24));
        ScaledHeight = (std::min)(ScaledHeight, static_cast<int>(work.bottom - work.top - 24));
        int ParentWidth = ParentRect.right - ParentRect.left;
        int ParentHeight = ParentRect.bottom - ParentRect.top;

        ::SetWindowPos(
            WindowHandle,
            nullptr,
            (std::clamp<int>)(ParentRect.left + ((ParentWidth - ScaledWidth) / 2), static_cast<int>(work.left), static_cast<int>(work.right - ScaledWidth)),
            (std::clamp<int>)(ParentRect.top + ((ParentHeight - ScaledHeight) / 2), static_cast<int>(work.top), static_cast<int>(work.bottom - ScaledHeight)),
            ScaledWidth,
            ScaledHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);

        ::ShowWindow(WindowHandle, SW_SHOW);
        ::UpdateWindow(WindowHandle);

        return ::MileXamlContentWindowDefaultMessageLoop();
    }

    int K7ModernShowXamlDialog(
        _In_opt_ HWND WindowHandle,
        _In_ int Width,
        _In_ int Height,
        _In_ LPVOID Content,
        _In_ HWND ParentWindowHandle)
    {
        if (!WindowHandle)
        {
            return -1;
        }

        ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, TRUE);

        HMENU MenuHandle = ::GetSystemMenu(WindowHandle, FALSE);
        if (MenuHandle)
        {
            ::RemoveMenu(MenuHandle, 0, MF_SEPARATOR);
            ::RemoveMenu(MenuHandle, SC_RESTORE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_SIZE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_MINIMIZE, MF_BYCOMMAND);
            ::RemoveMenu(MenuHandle, SC_MAXIMIZE, MF_BYCOMMAND);
        }

        if (ParentWindowHandle)
        {
            ::EnableWindow(ParentWindowHandle, FALSE);
        }

        if (FAILED(::MileXamlSetXamlContentForContentWindow(
            WindowHandle,
            Content)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }

        int Result = ::K7ModernShowXamlWindow(
            WindowHandle,
            Width,
            Height,
            ParentWindowHandle);

        return Result;
    }

    winrt::DesktopWindowXamlSource K7ModernGetDesktopWindowXamlSource(
        _In_ HWND WindowHandle)
    {
        winrt::DesktopWindowXamlSource XamlSource = nullptr;
        winrt::copy_from_abi(
            XamlSource,
            ::GetPropW(WindowHandle, L"XamlWindowSource"));
        return XamlSource;
    }
}

EXTERN_C INT WINAPI K7ModernShowSponsorDialog(
    _In_opt_ HWND ParentWindowHandle)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::SponsorPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::SponsorPage;

    Interface Window = winrt::make<Implementation>(WindowHandle);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        460,
        320,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowAboutDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR ExtendedMessage)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::AboutPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::AboutPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        ExtendedMessage);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        560,
        400,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowInformationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Content)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    using Interface =
        winrt::NanaZip::Modern::InformationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::InformationPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        Content);

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        560,
        560,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C VOID WINAPI K7ModernUpdateProgressWindowStatus(
    _In_ HWND WindowHandle,
    _In_ PK7_PROGRESS_WINDOW_STATUS Status)
{
    if (!WindowHandle || !Status)
    {
        return;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return;
    }

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return;
    }
    winrt::get_self<Implementation>(InstanceObject)->UpdateStatus(Status);
}

EXTERN_C VOID WINAPI K7ModernSetProgressWindowPausedMode(
    _In_ HWND WindowHandle,
    _In_ BOOL Paused)
{
    if (!WindowHandle)
    {
        return;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return;
    }

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return;
    }
    winrt::get_self<Implementation>(InstanceObject)->SetPausedMode(Paused);
}

EXTERN_C INT WINAPI K7ModernShowProgressWindow(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_ BOOL ShowCompressionInformation,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext)
{
    HWND WindowHandle = ::K7ModernCreateXamlWindow(
        ParentWindowHandle,
        WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME,
        WS_OVERLAPPEDWINDOW);
    if (!WindowHandle)
    {
        return -1;
    }

    ::MileAllowNonClientDefaultDrawingForWindow(WindowHandle, TRUE);

    using Interface =
        winrt::NanaZip::Modern::ProgressPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::ProgressPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        ShowCompressionInformation);

    if (FAILED(::MileXamlSetXamlContentForContentWindow(
        WindowHandle,
        winrt::get_abi(Window))))
    {
        ::DestroyWindow(WindowHandle);
        return -1;
    }

    if (WindowSubclassHandler)
    {
        if (!::SetWindowSubclass(
            WindowHandle,
            WindowSubclassHandler,
            1,
            reinterpret_cast<DWORD_PTR>(WindowSubclassContext)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }
    }

    int Result = ::K7ModernShowXamlWindow(
        WindowHandle,
        640,
        488,
        ParentWindowHandle);

    return Result;
}

EXTERN_C INT WINAPI K7ModernShowCopyLocationDialog(
    _In_opt_ HWND ParentWindowHandle,
    _In_opt_ LPCWSTR Title,
    _In_opt_ LPCWSTR Subtitle,
    _In_opt_ LPCWSTR AdditionalInformation,
    _In_opt_ LPCWSTR InitialPath,
    _In_ BOOL ShowExtractAll,
    _In_ SUBCLASSPROC WindowSubclassHandler,
    _In_ LPVOID WindowSubclassContext)
{
    HWND WindowHandle = ::K7ModernCreateXamlDialog(ParentWindowHandle);
    if (!WindowHandle)
    {
        return -1;
    }

    // Give the owned dialog an accessible window title as well as its XAML heading.
    ::SetWindowTextW(WindowHandle, Title ? Title : L"MiaoZip");

    using Interface =
        winrt::NanaZip::Modern::CopyLocationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CopyLocationPage;

    Interface Window = winrt::make<Implementation>(
        WindowHandle,
        Title,
        Subtitle,
        AdditionalInformation,
        InitialPath,
        ShowExtractAll);

    if (WindowSubclassHandler)
    {
        if (!::SetWindowSubclass(
            WindowHandle,
            WindowSubclassHandler,
            1,
            reinterpret_cast<DWORD_PTR>(WindowSubclassContext)))
        {
            ::DestroyWindow(WindowHandle);
            return -1;
        }
    }

    int Result = ::K7ModernShowXamlDialog(
        WindowHandle,
        640,
        440,
        winrt::get_abi(Window),
        ParentWindowHandle);

    return Result;
}

EXTERN_C LPCWSTR WINAPI K7ModernGetCopyLocationDialogPath(
    _In_ HWND WindowHandle)
{
    if (!WindowHandle)
    {
        return nullptr;
    }

    winrt::DesktopWindowXamlSource XamlSource =
        ::K7ModernGetDesktopWindowXamlSource(WindowHandle);
    if (!XamlSource)
    {
        return nullptr;
    }

    using Interface =
        winrt::NanaZip::Modern::CopyLocationPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::CopyLocationPage;
    Interface InstanceObject = XamlSource.Content().as<Interface>();
    if (!InstanceObject)
    {
        return nullptr;
    }
    return winrt::get_self<Implementation>(InstanceObject)->GetPath();
}
