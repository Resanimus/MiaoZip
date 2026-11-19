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
#include "../MiaoZip.Features/LiveEditLimits.h"
#include "../MiaoZip.Features/MenuModel.h"
#include <functional>
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
    void ApplyMenuTypography(const winrt::Windows::UI::Xaml::Controls::Control& item)
    {
        item.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily(L"Microsoft YaHei UI"));
        item.FontSize(13);
    }

    // Bottom placement is a preference in XAML. Limit the first-level
    // presenter to the remaining work area so it scrolls instead of flipping
    // above its button when the main window is near the bottom of a monitor.
    winrt::Windows::UI::Xaml::Style ConstrainMenuBelow(
        HWND window,
        const winrt::Windows::UI::Xaml::FrameworkElement& page,
        const winrt::Windows::UI::Xaml::FrameworkElement& anchor,
        const winrt::Windows::UI::Xaml::Style& baseStyle)
    {
        using namespace winrt::Windows::UI::Xaml;
        using namespace winrt::Windows::UI::Xaml::Controls;
        const HWND host = ::FindWindowExW(window, nullptr, L"Mile.Xaml.ContentWindow", nullptr);
        if (!host) return baseStyle;
        POINT origin{};
        if (!::ClientToScreen(host, &origin)) return baseStyle;
        const double scale = static_cast<double>(::GetDpiForWindow(window)) / 96.0;
        const auto bottom = anchor.TransformToVisual(page).TransformPoint(
            {0, static_cast<float>(anchor.ActualHeight())});
        POINT screenPoint{origin.x + static_cast<LONG>(bottom.X * scale),
            origin.y + static_cast<LONG>(bottom.Y * scale)};
        MONITORINFO monitor{sizeof(monitor)};
        if (!::GetMonitorInfoW(::MonitorFromPoint(screenPoint, MONITOR_DEFAULTTONEAREST), &monitor))
            return baseStyle;
        // Leave room for the flyout offset and its rounded border/shadow.
        double available = (monitor.rcWork.bottom - screenPoint.y) / scale - 20.0;
        if (available < 1.0) available = 1.0;
        Style style{winrt::xaml_typename<MenuFlyoutPresenter>()};
        style.BasedOn(baseStyle);
        style.Setters().Append(Setter(FrameworkElement::MinHeightProperty(), winrt::box_value(0.0)));
        style.Setters().Append(Setter(FrameworkElement::MaxHeightProperty(), winrt::box_value(available)));
        return style;
    }

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
        const auto editHint = L"开启后双击文档，保存后自动回写。支持 ZIP、非固实 7z。单文件上限 " +
            std::to_wstring(MiaoZip::DocumentLimitMiB) + L" MiB，压缩包上限 " +
            std::to_wstring(MiaoZip::ArchiveLimitMiB) + L" MiB。关闭开关暂停同步，恢复文件始终保留。";
        winrt::ToolTipService::SetToolTip(this->EditModeButton(), winrt::box_value(winrt::hstring(editHint)));
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
        menuStyle.Setters().Append(Setter(Controls::Control::FontFamilyProperty(), box_value(Media::FontFamily(L"Microsoft YaHei UI"))));
        menuStyle.Setters().Append(Setter(Controls::Control::FontSizeProperty(), box_value(13.0)));
        menuStyle.Setters().Append(Setter(Controls::Control::PaddingProperty(), box_value(Thickness{6,6,6,6})));
        for (const auto& item : this->ThemeButton().Flyout().as<Controls::MenuFlyout>().Items())
            if (const auto control = item.try_as<Controls::Control>()) ApplyMenuTypography(control);
        this->ThemeButton().Flyout().as<Controls::MenuFlyout>().MenuFlyoutPresenterStyle(menuStyle);
        this->SystemThemeItem().IsChecked(preferences.Appearance == MiaoZip::Theme::Mode::System);
        this->LightThemeItem().IsChecked(preferences.Appearance == MiaoZip::Theme::Mode::Light);
        this->DarkThemeItem().IsChecked(preferences.Appearance == MiaoZip::Theme::Mode::Dark);
        this->TransparencyItem().IsChecked(preferences.Glass);
    }

    void MainWindowToolBarPage::OverflowMenuClick(winrt::IInspectable const& sender, winrt::RoutedEventArgs const&)
    {
        using namespace winrt::Windows::UI::Xaml::Controls;
        using Action = void (MainWindowToolBarPage::*)(winrt::IInspectable const&, winrt::RoutedEventArgs const&);
        struct Command { AppBarButton Button; Action Invoke; };
        m_OverflowMenu = MenuFlyout();
        m_OverflowMenu.Placement(winrt::Windows::UI::Xaml::Controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
        m_OverflowMenu.ShouldConstrainToRootBounds(false);
        const auto anchor = sender.as<winrt::Windows::UI::Xaml::FrameworkElement>();
        m_OverflowMenu.MenuFlyoutPresenterStyle(ConstrainMenuBelow(m_WindowHandle,
            this->Content().as<winrt::Windows::UI::Xaml::FrameworkElement>(), anchor,
            this->ThemeButton().Flyout().as<MenuFlyout>().MenuFlyoutPresenterStyle()));
        const auto add = [this](const Command& command)
        {
            if (command.Button.Visibility() == winrt::Windows::UI::Xaml::Visibility::Collapsed) return;
            MenuFlyoutItem item;
            ApplyMenuTypography(item);
            item.Text(command.Button.Label()); item.IsEnabled(command.Button.IsEnabled());
            if (const auto symbol = command.Button.Icon().try_as<SymbolIcon>()) item.Icon(SymbolIcon(symbol.Symbol()));
            else if (const auto font = command.Button.Icon().try_as<FontIcon>())
            { FontIcon icon; icon.Glyph(font.Glyph()); icon.FontFamily(font.FontFamily()); item.Icon(icon); }
            else if (const auto path = command.Button.Icon().try_as<PathIcon>())
            { PathIcon icon; icon.Data(path.Data()); item.Icon(icon); }
            item.Click([weak = get_weak(), invoke = command.Invoke](auto const& source, auto const& event)
            { if (const auto self = weak.get()) (self.get()->*invoke)(source, event); });
            m_OverflowMenu.Items().Append(item);
        };
        const Command primary[] = {
            {ExtractButton(), &MainWindowToolBarPage::ExtractButtonClick},
            {AddButton(), &MainWindowToolBarPage::AddButtonClick},
            {TestButton(), &MainWindowToolBarPage::TestButtonClick},
            {RecoveryButton(), &MainWindowToolBarPage::RecoveryButtonClick},
            {MenuButton(), &MainWindowToolBarPage::MoreButtonClick}};
        for (const auto& command : primary) if (command.Button.IsInOverflow()) add(command);
        if (m_OverflowMenu.Items().Size()) m_OverflowMenu.Items().Append(MenuFlyoutSeparator());
        add({CopyButton(), &MainWindowToolBarPage::CopyButtonClick});
        add({MoveButton(), &MainWindowToolBarPage::MoveButtonClick});
        add({DeleteButton(), &MainWindowToolBarPage::DeleteButtonClick});
        m_OverflowMenu.Items().Append(MenuFlyoutSeparator());
        add({InfoButton(), &MainWindowToolBarPage::InfoButtonClick});
        add({OptionsButton(), &MainWindowToolBarPage::OptionsButtonClick});
        add({BenchmarkButton(), &MainWindowToolBarPage::BenchmarkButtonClick});
        add({AboutButton(), &MainWindowToolBarPage::AboutButtonClick});
        m_OverflowMenu.ShowAt(anchor);
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

    void MainWindowToolBarPage::ShowNativeMenu(HMENU menu)
    {
        if (m_MainMenu) m_MainMenu.Hide();
        POINT point{::MulDiv(24, ::GetDpiForWindow(m_WindowHandle), 96),
            ::MulDiv(104, ::GetDpiForWindow(m_WindowHandle), 96)};
        ::MapWindowPoints(m_WindowHandle, HWND_DESKTOP, &point, 1);
        const auto command = ::TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_RETURNCMD,
            point.x, point.y, m_WindowHandle, nullptr);
        if (command) ::PostMessageW(m_WindowHandle, WM_COMMAND, command, 0);
    }

    void MainWindowToolBarPage::MoreButtonClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&)
    {
        using namespace winrt::Windows::UI::Xaml;
        using namespace winrt::Windows::UI::Xaml::Controls;
        MiaoZip::Menus::SnapshotScope snapshotScope(m_WindowHandle);
        const auto entries = MiaoZip::Menus::Snapshot(m_WindowHandle, m_MoreMenu);
        m_MainMenu = MenuFlyout();
        m_MainMenu.Placement(Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
        m_MainMenu.ShouldConstrainToRootBounds(false);
        const auto anchor = MenuButton().IsInOverflow()
            ? MainCommands().as<FrameworkElement>() : MenuButton().as<FrameworkElement>();
        m_MainMenu.MenuFlyoutPresenterStyle(ConstrainMenuBelow(m_WindowHandle,
            this->Content().as<FrameworkElement>(), anchor,
            ThemeButton().Flyout().as<MenuFlyout>().MenuFlyoutPresenterStyle()));
        using Items = winrt::Windows::Foundation::Collections::IVector<MenuFlyoutItemBase>;
        std::function<void(const std::vector<MiaoZip::Menus::Entry>&, const Items&)> append;
        append = [&](const auto& source, const Items& target)
        {
            for (const auto& entry : source)
            {
                if (entry.Separator) { target.Append(MenuFlyoutSeparator()); continue; }
                if (!entry.Children.empty())
                {
                    MenuFlyoutSubItem item;
                    ApplyMenuTypography(item);
                    item.Resources().Insert(box_value(xaml_typename<MenuFlyoutPresenter>()), m_MainMenu.MenuFlyoutPresenterStyle());
                    item.Text(entry.Caption.Text); item.AccessKey(entry.Caption.AccessKey);
                    item.IsEnabled(entry.Enabled); item.RequestedTheme(RequestedTheme());
                    append(entry.Children, item.Items()); target.Append(item); continue;
                }
                const auto invoke = [weak = get_weak(), command = entry.Command, native = entry.Native](auto const&, auto const&)
                {
                    if (const auto self = weak.get())
                    {
                        if (native) self->ShowNativeMenu(native);
                        else ::PostMessageW(self->m_WindowHandle, WM_COMMAND, command, 0);
                    }
                };
                // Commands with a check retain their visual state. The original
                // WM_COMMAND handler owns the setting and refreshes it on reopen.
                if (entry.Checked || entry.Radio)
                {
                    ToggleMenuFlyoutItem item;
                    ApplyMenuTypography(item);
                    item.Text(entry.Caption.Text); item.AccessKey(entry.Caption.AccessKey);
                    item.KeyboardAcceleratorTextOverride(entry.Caption.Shortcut);
                    item.IsChecked(entry.Checked); item.IsEnabled(entry.Enabled);
                    item.RequestedTheme(RequestedTheme()); item.Click(invoke); target.Append(item);
                }
                else
                {
                    MenuFlyoutItem item;
                    ApplyMenuTypography(item);
                    item.Text(entry.Caption.Text); item.AccessKey(entry.Caption.AccessKey);
                    item.KeyboardAcceleratorTextOverride(entry.Caption.Shortcut);
                    item.IsEnabled(entry.Enabled); item.RequestedTheme(RequestedTheme());
                    item.Click(invoke); target.Append(item);
                }
            }
        };
        append(entries, m_MainMenu.Items());
        m_MainMenu.ShowAt(anchor);
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
