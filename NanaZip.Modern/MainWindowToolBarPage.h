#pragma once

#include "MainWindowToolBarPage.g.h"

#include <Windows.h>

#include <winrt/Windows.System.h>

namespace winrt
{
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::RoutedEventArgs;
}
namespace winrt::NanaZip::Modern::implementation
{
    struct MainWindowToolBarPage : MainWindowToolBarPageT<MainWindowToolBarPage>
    {
    public:

        MainWindowToolBarPage(
            _In_ HWND WindowHandle = nullptr,
            _In_ HMENU MoreMenu = nullptr);

        void InitializeComponent();
        void ThemeChoiceClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&);
        void TransparencyClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&);
        void RefreshThemeChecks();
        void OverflowMenuClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&);

        void CreateButtonClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&);
        void ShowNativeMenu(HMENU menu);

        void AddButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void ExtractButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void EditModeButtonClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&);
        void RecoveryButtonClick(winrt::IInspectable const&, winrt::RoutedEventArgs const&);
        void FilterTextBoxKeyDown(
            winrt::IInspectable const& sender,
            winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void TestButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void CopyButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void MoveButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void DeleteButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void InfoButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void OptionsButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void BenchmarkButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void AboutButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

        void MoreButtonClick(
            winrt::IInspectable const& sender,
            winrt::RoutedEventArgs const& e);

    private:

        winrt::Windows::UI::Xaml::Controls::MenuFlyout m_OverflowMenu{ nullptr };
        winrt::Windows::UI::Xaml::Controls::MenuFlyout m_MainMenu{ nullptr };
        HWND m_WindowHandle;
        HMENU m_MoreMenu;


    };
}

namespace winrt::NanaZip::Modern::factory_implementation
{
    struct MainWindowToolBarPage : MainWindowToolBarPageT<
        MainWindowToolBarPage, implementation::MainWindowToolBarPage>
    {
    };
}
