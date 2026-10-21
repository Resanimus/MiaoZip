#include "pch.h"
#include "MainWindowToolBarPage.h"
#if __has_include("MainWindowToolBarPage.g.cpp")
#include "MainWindowToolBarPage.g.cpp"
#endif

#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include "NanaZip.Modern.h"
#include "GlassTheme.h"
#include "../NanaZip.DailyExperience/Interaction.h"

#include <Mile.Helpers.CppBase.h>
#include <Mile.Helpers.CppWinRT.h>

namespace winrt::Mile
{
    using namespace ::Mile;
}

namespace winrt
{
    using Windows::Foundation::Point;
    using Windows::UI::Xaml::Automation::AutomationProperties;
    using Windows::UI::Xaml::Controls::AppBarButton;
    using Windows::UI::Xaml::Controls::ToolTipService;
    using Windows::UI::Xaml::Media::GeneralTransform;
}

namespace
{
    namespace ToolBarCommandID
    {
        enum
        {
            Add = 1070,
            Extract = 1071,
            Test = 1072,
            SmartExtract = 1073,
            Copy = 546,
            Move = 547,
            Delete = 548,
            Info = 551,
            Options = 900,
            Benchmark = 901,
            About = 961
        };
    }

    namespace MenuIndex
    {
        enum
        {
            File = 0,
            Edit,
            View,
            Bookmarks
        };
    }
}

namespace winrt::NanaZip::Modern::implementation
{
    MainWindowToolBarPage::MainWindowToolBarPage(
        _In_ HWND WindowHandle,
        _In_ HMENU MoreMenu) :
        m_WindowHandle(WindowHandle),
        m_MoreMenu(MoreMenu)
    {

    }

    void MainWindowToolBarPage::InitializeComponent()
    {
        MainWindowToolBarPageT::InitializeComponent();
        ApplyMiaoZipXamlTheme(*this);
        RefreshThemeChecks();
        this->ActualThemeChanged([this](auto&&, auto&&) { UpdateMiaoZipMaterial(); RefreshThemeChecks(); });
        this->SizeChanged([this](auto&&, auto&&)
        {
            this->SearchHost().Width(this->ActualWidth() < 1000 ? 156 : 200);
        });

        winrt::AppBarButton ToolBarButtons[10] =
        {
            this->AddButton(),
            this->ExtractButton(),
            this->TestButton(),
            this->CopyButton(),
            this->MoveButton(),
            this->DeleteButton(),
            this->InfoButton(),
            this->OptionsButton(),
            this->BenchmarkButton(),
            this->AboutButton()
        };

        const UINT32 ToolBarLegacyStringResources[10] =
        {
            7200, // Add
            7201, // Extract
            7202, // Test
            7203, // Copy
            7204, // Move
            7205, // Delete
            7206, // Info
            900, // Options
            901, // Benchmark
            961 // About
        };

        const std::size_t ToolBarButtonCount =
            sizeof(ToolBarButtons) / sizeof(*ToolBarButtons);

        for (size_t i = 0; i < ToolBarButtonCount; ++i)
        {
            winrt::hstring Resource = winrt::hstring(::K7ModernGetLegacyStringResource(
                ToolBarLegacyStringResources[i]));
            ToolBarButtons[i].Label(Resource);
            winrt::AutomationProperties::SetName(
                ToolBarButtons[i],
                Resource);
            winrt::ToolTipService::SetToolTip(
                ToolBarButtons[i],
                winrt::box_value(Resource));
        }

        this->ExtractButton().Label(L"解压");
        winrt::AutomationProperties::SetName(this->ExtractButton(), L"解压");
        winrt::ToolTipService::SetToolTip(this->ExtractButton(), winrt::box_value(L"解压选中的文件或压缩包"));

        this->MenuButton().Label(Mile::WinRT::GetLocalizedString(
            L"NanaZip.Modern/MainWindowToolBarPage/MainMenuLabel", L"Menu"));
        this->FilterTextBox().PlaceholderText(L"");
        winrt::AutomationProperties::SetName(this->EditModeButton(), L"包内编辑开关");
        winrt::ToolTipService::SetToolTip(this->EditModeButton(), winrt::box_value(L"开启后双击小文档，保存后自动回写。支持 ZIP、非固实 7z；单文档 20 MiB。关闭开关暂停同步，恢复文件始终保留。"));
        winrt::ToolTipService::SetToolTip(this->RecoveryButton(), winrt::box_value(L"打开持久工作副本、回写快照与原包备份"));
        winrt::ToolTipService::SetToolTip(this->FilterTextBox(), winrt::box_value(L"筛选文件名：Enter 应用，Esc 清空"));
        winrt::AutomationProperties::SetName(this->FilterTextBox(),
            Mile::WinRT::GetLocalizedString(
                L"NanaZip.Modern/MainWindowToolBarPage/FilterHint",
                L"Filter the current folder by name. Enter applies; Escape clears. Folders remain navigable through the address bar."));

    }

    void MainWindowToolBarPage::RefreshThemeChecks()
    {
        const auto preferences = MiaoZip::Theme::Current();
        // Popup presenters have their own visual root in a desktop XAML island.
        // Give them the same explicit theme as the page when it changes.
        using namespace winrt::Windows::UI::Xaml;
        const auto theme = this->RequestedTheme();
        this->MainCommands().RequestedTheme(theme);
        this->ThemeButton().RequestedTheme(theme);
        winrt::Windows::UI::Xaml::Style overflowStyle{ xaml_typename<Controls::CommandBarOverflowPresenter>() };
        overflowStyle.Setters().Append(Setter(FrameworkElement::RequestedThemeProperty(), box_value(theme)));
        overflowStyle.Setters().Append(Setter(FrameworkElement::MinWidthProperty(), box_value(220.0)));
        this->MainCommands().CommandBarOverflowPresenterStyle(overflowStyle);
        winrt::Windows::UI::Xaml::Style menuStyle{ xaml_typename<Controls::MenuFlyoutPresenter>() };
        menuStyle.Setters().Append(Setter(FrameworkElement::RequestedThemeProperty(), box_value(theme)));
        this->ThemeButton().Flyout().as<Controls::MenuFlyout>().MenuFlyoutPresenterStyle(menuStyle);
        this->SystemThemeItem().IsChecked(preferences.Appearance == MiaoZip::Theme::Mode::System);
        this->LightThemeItem().IsChecked(preferences.Appearance == MiaoZip::Theme::Mode::Light);
        this->DarkThemeItem().IsChecked(preferences.Appearance == MiaoZip::Theme::Mode::Dark);
        this->TransparencyItem().IsChecked(preferences.Glass);
    }

    void MainWindowToolBarPage::ThemeChoiceClick(winrt::IInspectable const& sender, winrt::RoutedEventArgs const&)
    {
        const auto item = sender.as<winrt::Windows::UI::Xaml::Controls::ToggleMenuFlyoutItem>();
        const auto value = winrt::unbox_value<winrt::hstring>(item.Tag());
        const auto mode = value == L"1" ? MiaoZip::Theme::Mode::Light :
            value == L"2" ? MiaoZip::Theme::Mode::Dark : MiaoZip::Theme::Mode::System;
        MiaoZip::Theme::Save(mode, MiaoZip::Theme::Current().Glass);
        ApplyMiaoZipXamlTheme(*this); RefreshThemeChecks();
        ::PostMessageW(this->m_WindowHandle, WM_COMMAND, MAKEWPARAM(1076, BN_CLICKED), 0);
    }

    void MainWindowToolBarPage::TransparencyClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&)
    {
        MiaoZip::Theme::Save(MiaoZip::Theme::Current().Appearance, this->TransparencyItem().IsChecked());
        ApplyMiaoZipXamlTheme(*this); RefreshThemeChecks();
        ::PostMessageW(this->m_WindowHandle, WM_COMMAND, MAKEWPARAM(1076, BN_CLICKED), 0);
    }

    void MainWindowToolBarPage::AddButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Add,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::ExtractButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Extract,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::EditModeButtonClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&)
    {
        const auto checked = this->EditModeButton().IsChecked();
        const bool enabled = checked && checked.Value();
        this->EditModeButton().Label(L"包内编辑");
        winrt::AutomationProperties::SetName(this->EditModeButton(), enabled ? L"包内编辑：开启" : L"包内编辑：关闭");
        ::PostMessageW(this->m_WindowHandle, WM_COMMAND, MAKEWPARAM(1074, BN_CLICKED), checked && checked.Value() ? 1 : 0);
    }

    void MainWindowToolBarPage::RecoveryButtonClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&)
    {
        ::PostMessageW(this->m_WindowHandle, WM_COMMAND, MAKEWPARAM(1075, BN_CLICKED), 0);
    }

    void MainWindowToolBarPage::FilterTextBoxKeyDown(
        winrt::IInspectable const& sender,
        winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        if (e.Key() != winrt::Windows::System::VirtualKey::Enter &&
            e.Key() != winrt::Windows::System::VirtualKey::Escape)
        {
            return;
        }
        if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            this->FilterTextBox().Text(L"");
        }
        std::wstring Filter = this->FilterTextBox().Text().c_str();
        COPYDATASTRUCT Data = {};
        Data.dwData = ::NanaZip::DailyExperience::FileNameFilterMessage;
        Data.cbData = static_cast<DWORD>((Filter.size() + 1) * sizeof(wchar_t));
        Data.lpData = Filter.data();
        ::SendMessageW(this->m_WindowHandle, WM_COPYDATA, 0,
            reinterpret_cast<LPARAM>(&Data));
        e.Handled(true);
    }

    void MainWindowToolBarPage::TestButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Test,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::CopyButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Copy,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::MoveButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Move,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::DeleteButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Delete,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::InfoButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Info,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::OptionsButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Options,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::BenchmarkButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::Benchmark,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::AboutButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        ::PostMessageW(
            this->m_WindowHandle,
            WM_COMMAND,
            MAKEWPARAM(
                ToolBarCommandID::About,
                BN_CLICKED),
            0);
    }

    void MainWindowToolBarPage::MoreButtonClick(
        winrt::IInspectable const& sender,
        winrt::RoutedEventArgs const& e)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(e);

        winrt::AppBarButton Button = sender.as<winrt::AppBarButton>();

        winrt::GeneralTransform Transform =
            Button.TransformToVisual(this->Content());
        winrt::Point LogicalPoint = Transform.TransformPoint(
            winrt::Point(0.0f, 0.0f));

        UINT DpiValue = ::GetDpiForWindow(this->m_WindowHandle);

        POINT MenuPosition = {};
        MenuPosition.x = ::MulDiv(
            static_cast<int>(LogicalPoint.X),
            DpiValue,
            USER_DEFAULT_SCREEN_DPI);
        MenuPosition.y = ::MulDiv(
            60,
            DpiValue,
            USER_DEFAULT_SCREEN_DPI);
        ::MapWindowPoints(
            this->m_WindowHandle,
            HWND_DESKTOP,
            &MenuPosition,
            1);

        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::File)),
            MenuIndex::File);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::Edit)),
            MenuIndex::Edit);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::View)),
            MenuIndex::View);
        ::SendMessageW(
            this->m_WindowHandle,
            WM_INITMENUPOPUP,
            reinterpret_cast<WPARAM>(::GetSubMenu(
                this->m_MoreMenu,
                MenuIndex::Bookmarks)),
            MenuIndex::Bookmarks);

        WPARAM Command = ::TrackPopupMenuEx(
            this->m_MoreMenu,
            TPM_LEFTALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
            MenuPosition.x,
            MenuPosition.y,
            this->m_WindowHandle,
            nullptr);
        if (Command)
        {
            ::PostMessageW(this->m_WindowHandle, WM_COMMAND, Command, 0);
        }
    }


}

EXTERN_C LPVOID WINAPI K7ModernCreateMainWindowToolBarPage(
    _In_ HWND ParentWindowHandle,
    _In_ HMENU MoreMenuHandle)
{
    using Interface =
        winrt::NanaZip::Modern::MainWindowToolBarPage;
    using Implementation =
        winrt::NanaZip::Modern::implementation::MainWindowToolBarPage;

    Interface Window = winrt::make<Implementation>(
        ParentWindowHandle,
        MoreMenuHandle);
    return winrt::detach_abi(Window);
}
