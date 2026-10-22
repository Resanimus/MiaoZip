// Panel.cpp

#include "StdAfx.h"
#include "../../../../../../MiaoZip.Features/TreeSurface.h"

#include <WindowsX.h>
// #include <stdio.h>

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/Thread.h"

#include "../../PropID.h"

#include "resource.h"
#include "../GUI/ExtractRes.h"

#include "../Common/ArchiveName.h"
#include "../Common/CompressCall.h"
#include "../Common/ZipRegistry.h"

#include "../Agent/IFolderArchive.h"

#include "App.h"
#include "ExtractCallback.h"
#include "FSFolder.h"
#include "FormatUtils.h"
#include "Panel.h"
#include "RootFolder.h"
// **************** NanaZip Modification Start ****************
#include <K7Base.h>
#include <K7User.h>
// **************** NanaZip Modification End ****************

#include "PropertyNameRes.h"

#include <Mile.Xaml.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <uxtheme.h>
#include <ShlObj.h>
#include "../../../../../../MiaoZip.Features/NativeSurface.h"
#pragma comment(lib, "uxtheme.lib")
// Note: All HWND should belong to the same main window thread.
extern std::vector<HWND> g_K7ControlList;

using namespace NWindows;
using namespace NControl;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

static const UINT_PTR kTimerID = 1;
static const UINT kTimerElapse = 1000;

static DWORD kStyles[4] = { LVS_ICON, LVS_SMALLICON, LVS_LIST, LVS_REPORT };

// static const int kCreateFolderID = 101;

extern HINSTANCE g_hInstance;
extern DWORD g_ComCtl32Version;

static const int AddressBarHeight = 56;
static const int StatusBarHeight = 40;

void CPanel::Release()
{
  // It's for unloading COM dll's: don't change it.
  CloseOpenFolders();
  _sevenZipContextMenu.Release();
  _systemContextMenu.Release();
}

CPanel::~CPanel()
{
  CloseOpenFolders();
}

HWND CPanel::GetParent() const
{
  HWND h = CWindow2::GetParent();
  return (h == 0) ? _mainWindow : h;
}

#define kClassName L"NanaZip::Panel"


HRESULT CPanel::Create(HWND mainWindow, HWND parentWindow, UINT id,
    const UString &currentFolderPrefix,
    const UString &arcFormat,
    CPanelCallback *panelCallback, CAppState *appState,
    bool needOpenArc,
    COpenResult &openRes)
{
  _mainWindow = mainWindow;
  _processTimer = true;
  _processNotify = true;
  _processStatusBar = true;

  _panelCallback = panelCallback;
  _appState = appState;
  // _index = index;
  _baseID = id;
  // _comboBoxID = _baseID + 3;
  // _statusBarID = _comboBoxID + 1;

  UString cfp = currentFolderPrefix;

  if (!currentFolderPrefix.IsEmpty())
    if (currentFolderPrefix[0] == L'.')
    {
      FString cfpF;
      if (NFile::NDir::MyGetFullPathName(us2fs(currentFolderPrefix), cfpF))
        cfp = fs2us(cfpF);
    }

  RINOK(BindToPath(cfp, arcFormat, openRes));

  if (needOpenArc && !openRes.ArchiveIsOpened)
    return S_OK;

  if (!CreateEx(0, kClassName, 0, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
      0, 0, _xSize, 260,
      parentWindow, (HMENU)(UINT_PTR)id, g_hInstance))
    return E_FAIL;
  PanelCreated = true;

  return S_OK;
}

// extern UInt32 g_NumMessages;

LRESULT CPanel::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  // g_NumMessages++;
  switch (message)
  {
    // **************** NanaZip Modification Start ****************
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: PaintMiaoZipPanel(); return 0;
    case WM_MEASUREITEM:
    {
        auto item = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (item && item->CtlType == ODT_LISTVIEW && item->CtlID == _baseID + 1)
        { item->itemHeight = MiaoZip::Theme::Scale(*this, 34); return TRUE; }
        break;
    }
    case WM_DRAWITEM:
        if (DrawMiaoZipRow(reinterpret_cast<DRAWITEMSTRUCT*>(lParam))) return TRUE;
        break;
    // **************** NanaZip Modification End ****************
    case kShiftSelectMessage:
      OnShiftSelectMessage();
      return 0;
    case kReLoadMessage:
      RefreshListCtrl(_selectedState);
      return 0;
    case kSetFocusToListView:
      _listView.SetFocus();
      return 0;
    case kOpenItemChanged:
      return OnOpenItemChanged(lParam);
    case kRefresh_StatusBar:
      if (_processStatusBar)
        Refresh_StatusBar();
      return 0;
    #ifdef UNDER_CE
    case kRefresh_HeaderComboBox:
      LoadFullPathAndShow();
      return 0;
    #endif
    case WM_TIMER:
      OnTimer();
      return 0;
    case WM_CONTEXTMENU:
      if (OnContextMenu(HANDLE(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
        return 0;
      break;
    /*
    case WM_DROPFILES:
      CompressDropFiles(HDROP(wParam));
      return 0;
    */
  }
  return CWindow2::OnMessage(message, wParam, lParam);
}

LRESULT CMyListView::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_MOUSEMOVE)
    MiaoZip::Theme::TrackRow(*this, lParam);
  else if (message == WM_MOUSELEAVE)
    MiaoZip::Theme::ClearHotRow(*this);
  else if (message == WM_PAINT && (GetStyle() & LVS_TYPEMASK) == LVS_REPORT)
  {
    const LRESULT result = CListView2::OnMessage(message, wParam, lParam);
    HDC dc = GetDC(*this); RECT area{}; ::GetClientRect(*this, &area);
    const int count = ListView_GetItemCount(*this);
    RECT last{};
    if (count > 0 && ListView_GetItemRect(*this, count - 1, &last, LVIR_BOUNDS))
      area.top = MyMax(static_cast<int>(last.bottom), 0);
    else
    {
      const HWND header = ListView_GetHeader(*this);
      ::GetWindowRect(header, &last);
      MapWindowPoints(HWND_DESKTOP, *this, reinterpret_cast<POINT*>(&last), 2);
      area.top = last.bottom;
    }
    if (area.top < area.bottom)
      MiaoZip::Theme::Fill(dc, area, MiaoZip::Theme::Colors().Content);
    if (count == 0)
    {
      const auto old = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(*this, WM_GETFONT, 0, 0)));
      SetBkMode(dc, TRANSPARENT); SetTextColor(dc, MiaoZip::Theme::Colors().Muted);
      DrawTextW(dc, L"没有可显示的项目", -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, old);
    }
    ReleaseDC(*this, dc); return result;
  }
  if (message == WM_CHAR)
  {
    UINT scanCode = (UINT)((lParam >> 16) & 0xFF);
    bool extended = ((lParam & 0x1000000) != 0);
    UINT virtualKey = MapVirtualKey(scanCode, 1);
    if (virtualKey == VK_MULTIPLY || virtualKey == VK_ADD ||
        virtualKey == VK_SUBTRACT)
      return 0;
    if ((wParam == '/' && extended)
        || wParam == '\\' || wParam == '/')
    {
      _panel->OpenDrivesFolder();
      return 0;
    }
  }
  else if (message == WM_SYSCHAR)
  {
    // For Alt+Enter Beep disabling
    UINT scanCode = (UINT)(lParam >> 16) & 0xFF;
    UINT virtualKey = MapVirtualKey(scanCode, 1);
    if (virtualKey == VK_RETURN || virtualKey == VK_MULTIPLY ||
        virtualKey == VK_ADD || virtualKey == VK_SUBTRACT)
      return 0;
  }
  /*
  else if (message == WM_SYSKEYDOWN)
  {
    // return 0;
  }
  */
  else if (message == WM_KEYDOWN)
  {
    bool alt = IsKeyDown(VK_MENU);
    bool ctrl = IsKeyDown(VK_CONTROL);
    bool shift = IsKeyDown(VK_SHIFT);
    switch (wParam)
    {
      /*
      case VK_RETURN:
      {
        if (shift && !alt && !ctrl)
        {
          _panel->OpenSelectedItems(false);
          return 0;
        }
        break;
      }
      */
      case VK_NEXT:
      {
        if (ctrl && !alt && !shift)
        {
          _panel->OpenFocusedItemAsInternal();
          return 0;
        }
        break;
      }
      case VK_PRIOR:
      if (ctrl && !alt && !shift)
      {
        _panel->OpenParentFolder();
        return 0;
      }
    }
  }
  #ifdef UNDER_CE
  else if (message == WM_KEYUP)
  {
    if (wParam == VK_F2) // it's VK_TSOFT2
    {
      // Activate Menu
      ::PostMessage(g_HWND, WM_SYSCOMMAND, SC_KEYMENU, 0);
      return 0;
    }
  }
  #endif
  else if (message == WM_SETFOCUS)
  {
    _panel->_lastFocusedIsList = true;
    _panel->_panelCallback->PanelWasFocused();
  }
  return CListView2::OnMessage(message, wParam, lParam);
}

/*
static LRESULT APIENTRY ComboBoxSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(hwnd);
  CMyComboBox *w = (CMyComboBox *)(tempDialog.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}

LRESULT CMyComboBox::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}

#ifndef UNDER_CE

static LRESULT APIENTRY ComboBoxEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(hwnd);
  CMyComboBoxEdit *w = (CMyComboBoxEdit *)(tempDialog.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}
#endif

LRESULT CMyComboBoxEdit::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  // See MSDN / Subclassing a Combo Box / Creating a Combo-box Toolbar
  switch (message)
  {
    case WM_SYSKEYDOWN:
      switch (wParam)
      {
        case VK_F1:
        case VK_F2:
        {
          // check ALT
          if ((lParam & (1<<29)) == 0)
            break;
          bool alt = IsKeyDown(VK_MENU);
          bool ctrl = IsKeyDown(VK_CONTROL);
          bool shift = IsKeyDown(VK_SHIFT);
          if (alt && !ctrl && !shift)
          {
            _panel->_panelCallback->SetFocusToPath(wParam == VK_F1 ? 0 : 1);
            return 0;
          }
          break;
        }
      }
      break;
    case WM_KEYDOWN:
      switch (wParam)
      {
        case VK_TAB:
          // SendMessage(hwndMain, WM_ENTER, 0, 0);
          _panel->SetFocusToList();
          return 0;
        case VK_F9:
        {
          bool alt = IsKeyDown(VK_MENU);
          bool ctrl = IsKeyDown(VK_CONTROL);
          bool shift = IsKeyDown(VK_SHIFT);
          if (!alt && !ctrl && !shift)
          {
            g_App.SwitchOnOffOnePanel();
            return 0;
          }
          break;
        }
      }
      break;
    case WM_CHAR:
      switch (wParam)
      {
        case VK_TAB:
        case VK_ESCAPE:
          return 0;
      }
  }
  #ifndef _UNICODE
  if (g_IsNT)
    return CallWindowProcW(_origWindowProc, *this, message, wParam, lParam);
  else
  #endif
    return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}
*/

bool CPanel::OnCreate(CREATESTRUCT * /* createStruct */)
{
  // _virtualMode = false;
  // _sortIndex = 0;
  _sortID = kpidName;
  _ascending = true;
  _lastFocusedIsList = true;

  DWORD style = WS_CHILD | WS_VISIBLE; //  | WS_BORDER ; // | LVS_SHAREIMAGELISTS; //  | LVS_SHOWSELALWAYS;

  style |= LVS_SHAREIMAGELISTS;
  // style  |= LVS_AUTOARRANGE;
  style |= WS_CLIPCHILDREN;
  style |= WS_CLIPSIBLINGS;

  const UInt32 kNumListModes = ARRAY_SIZE(kStyles);
  if (_ListViewMode >= kNumListModes)
    _ListViewMode = kNumListModes - 1;

  style |= kStyles[_ListViewMode]
    | WS_TABSTOP
    | LVS_EDITLABELS | LVS_OWNERDRAWFIXED | LVS_SHOWSELALWAYS;
  if (_mySelectMode)
    style |= LVS_SINGLESEL;

  /*
  if (_virtualMode)
    style |= LVS_OWNERDATA;
  */

  DWORD exStyle;
  exStyle = 0;

  if (!_listView.CreateEx(exStyle, style, 0, 0, 116, 260,
      *this, (HMENU)(UINT_PTR)(_baseID + 1), g_hInstance, NULL))
    return false;

  _miaoTree = CreateWindowExW(0, WC_TREEVIEWW, L"压缩包目录",
      WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
      0, 36, 210, 400, *this, reinterpret_cast<HMENU>(_baseID + 9), g_hInstance, nullptr);
  if (_miaoTree)
  {
      TreeView_SetImageList(_miaoTree, GetSysImageList(true), TVSIL_NORMAL);
      SendMessageW(_miaoTree, WM_SETFONT, SendMessageW(_listView, WM_GETFONT, 0, 0), TRUE);
      SetWindowTheme(_miaoTree, L"Explorer", nullptr);
      TreeView_SetItemHeight(_miaoTree, MulDiv(25, GetDpiForWindow(*this), 96));
  }
  _listView.SetUnicodeFormat();
  _listView._panel = this;
  _listView.SetWindowProc();

  _listView.SetImageList(GetSysImageList(true), LVSIL_SMALL);
  _listView.SetImageList(GetSysImageList(false), LVSIL_NORMAL);

  // _exStyle |= LVS_EX_HEADERDRAGDROP;
  // DWORD extendedStyle = _listView.GetExtendedListViewStyle();
  // extendedStyle |= _exStyle;
  //  _listView.SetExtendedListViewStyle(extendedStyle);
  SetExtendedStyle();

  _listView.Show(SW_SHOW);
  _listView.InvalidateRect(NULL, true);
  _listView.Update();

  // Ensure that the common control DLL is loaded.
  INITCOMMONCONTROLSEX icex;

  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC  = ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);

  TBBUTTON tbb [ ] =
  {
    // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
    {0, kParentFolderID, TBSTATE_ENABLED, BTNS_BUTTON, { 0, 0 }, 0, 0 },
    // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
    // {VIEW_NEWFOLDER, kCreateFolderID, TBSTATE_ENABLED, BTNS_BUTTON, 0L, 0},
  };

  _addressBarControl = winrt::NanaZip::Modern::AddressBar{};
  _addressBarWindow = ::CreateWindowEx(
      WS_EX_NOREDIRECTIONBITMAP,
      L"Mile.Xaml.ContentWindow",
      nullptr,
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
      0,
      0,
      0,
      0,
      *this,
      nullptr,
      nullptr,
      winrt::get_abi(_addressBarControl)
  );

  {
      using winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
      using winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs;
      using winrt::Windows::UI::Xaml::Hosting::XamlSourceFocusNavigationReason;
      using winrt::Windows::UI::Xaml::Input::KeyboardNavigationMode;

      DesktopWindowXamlSource XamlSource = nullptr;
      winrt::copy_from_abi(
          XamlSource,
          ::GetPropW(_addressBarWindow, L"XamlWindowSource"));

      XamlSource.Content().TabFocusNavigation(KeyboardNavigationMode::Local);

      XamlSource.TakeFocusRequested(
          [this](
              DesktopWindowXamlSource const& sender,
              DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& e)
      {
          UNREFERENCED_PARAMETER(sender);
          XamlSourceFocusNavigationReason Reason = e.Request().Reason();
          switch (Reason)
          {
          case XamlSourceFocusNavigationReason::First:
          case XamlSourceFocusNavigationReason::Last:
          {
              std::size_t Count = g_K7ControlList.size();
              std::size_t Index = static_cast<std::size_t>(-1);
              for (size_t i = 0; i < Count; ++i)
              {
                  if (this->_addressBarWindow == g_K7ControlList[i])
                  {
                      Index = i;
                      break;
                  }
              }
              if (static_cast<std::size_t>(-1) != Index)
              {
                  std::size_t NextIndex = (XamlSourceFocusNavigationReason::Last == Reason)
                      ? ((Index == 0) ? (Count - 1) : (Index - 1))
                      : ((Index + 1) % Count);
                  ::SetFocus(g_K7ControlList[NextIndex]);
              }
              break;
          }
          default:
              break;
          }
      });
  }

  _sysImageList = GetSysImageList(true);

  _addressBarControl.UpButtonClicked(
      [this](auto&&, auto&&)
      {
          this->OpenParentFolder();
          this->SetFocusToList();
      });

  _addressBarControl.QuerySubmitted({ this, &CPanel::OnAddressBarQuerySubmitted });

  _addressBarControl.GotFocus(
      [&](auto&&, auto&&)
      {
          _panelCallback->PanelWasFocused();
      }
  );

  _items =
      winrt::single_threaded_observable_vector<
      winrt::NanaZip::Modern::AddressBarItem>();
  _addressBarControl.ItemsSource(_items);
  _addressBarControl.DropDownOpened({ this, &CPanel::OnDropDownOpened });

  _addressBarControl.KeyUp(
      [&]
      (
          auto&&,
          winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs
          const& args
      )
      {
          if (args.Key() == winrt::Windows::System::VirtualKey::Escape)
          {
              _addressBarControl.Text(_currentFolderPrefix.Ptr());
              PostMsg(kSetFocusToListView);
          }
      }
  );

  _statusBarControl = winrt::NanaZip::Modern::StatusBar{};
  _statusBarWindow = ::CreateWindowEx(
      WS_EX_NOREDIRECTIONBITMAP,
      L"Mile.Xaml.ContentWindow",
      nullptr,
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
      0,
      0,
      0,
      0,
      *this,
      nullptr,
      nullptr,
      winrt::get_abi(_statusBarControl)
  );

  {
      using winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource;
      using winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs;
      using winrt::Windows::UI::Xaml::Hosting::XamlSourceFocusNavigationReason;
      using winrt::Windows::UI::Xaml::Input::KeyboardNavigationMode;

      DesktopWindowXamlSource XamlSource = nullptr;
      winrt::copy_from_abi(
          XamlSource,
          ::GetPropW(_statusBarWindow, L"XamlWindowSource"));

      XamlSource.Content().TabFocusNavigation(KeyboardNavigationMode::Local);

      XamlSource.TakeFocusRequested(
          [this](
              DesktopWindowXamlSource const& sender,
              DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& e)
      {
          UNREFERENCED_PARAMETER(sender);
          XamlSourceFocusNavigationReason Reason = e.Request().Reason();
          switch (Reason)
          {
          case XamlSourceFocusNavigationReason::First:
          case XamlSourceFocusNavigationReason::Last:
          {
              std::size_t Count = g_K7ControlList.size();
              std::size_t Index = static_cast<std::size_t>(-1);
              for (size_t i = 0; i < Count; ++i)
              {
                  if (this->_statusBarWindow == g_K7ControlList[i])
                  {
                      Index = i;
                      break;
                  }
              }
              if (static_cast<std::size_t>(-1) != Index)
              {
                  std::size_t NextIndex = (XamlSourceFocusNavigationReason::Last == Reason)
                      ? ((Index == 0) ? (Count - 1) : (Index - 1))
                      : ((Index + 1) % Count);
                  ::SetFocus(g_K7ControlList[NextIndex]);
              }
              break;
          }
          default:
              break;
          }
      });
  }

  g_K7ControlList.push_back(_addressBarWindow);
  g_K7ControlList.push_back(_listView);
  g_K7ControlList.push_back(_statusBarWindow);

  // #ifndef UNDER_CE
  // if (g_ComCtl32Version >= MAKELONG(71, 4))
  // #endif
  // {
  //   icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  //   icex.dwICC  = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
  //   InitCommonControlsEx(&icex);

  //   // if there is no CCS_NOPARENTALIGN, there is space of some pixels after rebar (Incorrect GetWindowRect ?)

  //   _headerReBar.Attach(::CreateWindowEx(WS_EX_TOOLWINDOW,
  //     REBARCLASSNAME,
  //     NULL, WS_VISIBLE | WS_BORDER | WS_CHILD |
  //     WS_CLIPCHILDREN | WS_CLIPSIBLINGS
  //     | CCS_NODIVIDER
  //     | CCS_NOPARENTALIGN
  //     | CCS_TOP
  //     | CCS_NORESIZE
  //     | RBS_VARHEIGHT
  //     | RBS_BANDBORDERS
  //     ,0,0,0,0, *this, NULL, g_hInstance, NULL));
  // }

  // DWORD toolbarStyle =  WS_CHILD | WS_VISIBLE ;
  // if (_headerReBar)
  // {
  //   toolbarStyle |= 0
  //     // | WS_CLIPCHILDREN
  //     // | WS_CLIPSIBLINGS

  //     | TBSTYLE_TOOLTIPS
  //     | CCS_NODIVIDER
  //     | CCS_NORESIZE
  //     | TBSTYLE_FLAT
  //     ;
  // }

  // _headerToolBar.Attach(::CreateToolbarEx ((*this), toolbarStyle,
  //     _baseID + 2, 11,
  //     g_hInstance,
  //     IDB_VIEW_SMALL_COLOR,
  //     nullptr, 0,
  //     0, 0, 0, 0, sizeof(TBBUTTON)));

  // TBADDBITMAP ToolbarAddBitmap;
  // ToolbarAddBitmap.hInst = g_hInstance;
  // ToolbarAddBitmap.nID = IDB_PARENT_FOLDER;
  // SendMessageW(
  //   _headerToolBar,
  //   TB_ADDBITMAP,
  //   (WPARAM)1,
  //   (LPARAM)&ToolbarAddBitmap);

  // SendMessageW(
  //   _headerToolBar,
  //   TB_ADDBUTTONS,
  //   ARRAY_SIZE(tbb),
  //   (LPARAM)&tbb);

  // #ifndef UNDER_CE
  // // Load ComboBoxEx class
  // icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  // icex.dwICC = ICC_USEREX_CLASSES;
  // InitCommonControlsEx(&icex);
  // #endif

  // _headerComboBox.CreateEx(0,
  //     #ifdef UNDER_CE
  //     WC_COMBOBOXW
  //     #else
  //     WC_COMBOBOXEXW
  //     #endif
  //     , NULL,
  //   WS_BORDER | WS_VISIBLE |WS_CHILD | CBS_DROPDOWN | CBS_AUTOHSCROLL,
  //     0, 0, 100, 520,
  //     ((_headerReBar == 0) ? (HWND)*this : _headerToolBar),
  //     (HMENU)(UINT_PTR)(_comboBoxID),
  //     g_hInstance, NULL);
  //
  // #ifndef UNDER_CE
  // _headerComboBox.SetUnicodeFormat(true);
  // _headerComboBox.SetImageList(GetSysImageList(true));
  // _headerComboBox.SetExtendedStyle(CBES_EX_PATHWORDBREAKPROC, CBES_EX_PATHWORDBREAKPROC);
  // /*
  // _headerComboBox.SetUserDataLongPtr(LONG_PTR(&_headerComboBox));
  // _headerComboBox._panel = this;
  // _headerComboBox._origWindowProc =
  //     (WNDPROC)_headerComboBox.SetLongPtr(GWLP_WNDPROC,
  //     LONG_PTR(ComboBoxSubclassProc));
  // */
  // _comboBoxEdit.Attach(_headerComboBox.GetEditControl());
  // // _comboBoxEdit.SendMessage(CCM_SETUNICODEFORMAT, (WPARAM)(BOOL)TRUE, 0);
  // _comboBoxEdit.SetUserDataLongPtr(LONG_PTR(&_comboBoxEdit));
  // _comboBoxEdit._panel = this;
  //  #ifndef _UNICODE
  //  if (g_IsNT)
  //    _comboBoxEdit._origWindowProc =
  //     (WNDPROC)_comboBoxEdit.SetLongPtrW(GWLP_WNDPROC, LONG_PTR(ComboBoxEditSubclassProc));
  //  else
  //  #endif
  //    _comboBoxEdit._origWindowProc =
  //     (WNDPROC)_comboBoxEdit.SetLongPtr(GWLP_WNDPROC, LONG_PTR(ComboBoxEditSubclassProc));
  // #endif

  // if (_headerReBar)
  // {
  //   REBARINFO     rbi;
  //   rbi.cbSize = sizeof(REBARINFO);  // Required when using this struct.
  //   rbi.fMask  = 0;
  //   rbi.himl   = (HIMAGELIST)NULL;
  //   _headerReBar.SetBarInfo(&rbi);

  //   // Send the TB_BUTTONSTRUCTSIZE message, which is required for
  //   // backward compatibility.
  //   // _headerToolBar.SendMessage(TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
  //   SIZE size;
  //   _headerToolBar.GetMaxSize(&size);

  //   REBARBANDINFO rbBand;
  //   rbBand.cbSize = sizeof(REBARBANDINFO);  // Required
  //   rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
  //   rbBand.fStyle = RBBS_NOGRIPPER;
  //   rbBand.cxMinChild = size.cx;
  //   rbBand.cyMinChild = size.cy;
  //   rbBand.cyChild = size.cy;
  //   rbBand.cx = size.cx;
  //   rbBand.hwndChild  = _headerToolBar;
  //   _headerReBar.InsertBand(-1, &rbBand);

  //   RECT rc;
  //   ::GetWindowRect(_headerComboBox, &rc);
  //   rbBand.cxMinChild = 30;
  //   rbBand.cyMinChild = rc.bottom - rc.top;
  //   rbBand.cx = 1000;
  //   rbBand.hwndChild  = _headerComboBox;
  //   _headerReBar.InsertBand(-1, &rbBand);
  //   // _headerReBar.MaximizeBand(1, false);
  // }
  // _statusBar.Create(WS_CHILD | WS_VISIBLE, L"Status", (*this), _statusBarID);
  // _statusBar2.Create(WS_CHILD | WS_VISIBLE, L"Status", (*this), _statusBarID + 1);

  // const int sizes[] = {220, 320, 420, -1};
  // _statusBar.SetParts(4, sizes);
  // _statusBar2.SetParts(5, sizes);

  /*
  RECT rect;
  GetClientRect(&rect);
  OnSize(0, RECT_SIZE_X(rect), RECT_SIZE_Y(rect));
  */

  SetTimer(kTimerID, kTimerElapse);

  // InitListCtrl();
  ApplyMiaoZipTheme();
  RefreshListCtrl();

  return true;
}

void CPanel::OnDestroy()
{
  SaveListViewInfo();
  CWindow2::OnDestroy();
  if (_miaoFont) { DeleteObject(_miaoFont); _miaoFont = nullptr; }
  if (_miaoHeaderFont) { DeleteObject(_miaoHeaderFont); _miaoHeaderFont = nullptr; }
}

void CPanel::ChangeWindowSize(int xSize, int ySize)
{
  if (!(HWND)*this) return;
  const int gap = MiaoZip::Theme::Scale(*this, 12);
  const int top = MiaoZip::Theme::Scale(*this, AddressBarHeight) + gap;
  const int footer = MiaoZip::Theme::Scale(*this, StatusBarHeight);
  const int height = MyMax(ySize - top - footer - gap, 0);
  const int sidebar = xSize >= MiaoZip::Theme::Scale(*this, 680)
      ? MyMin(MiaoZip::Theme::Scale(*this, 220), xSize / 4) : 0;
  if (_miaoTree)
  {
    if (sidebar)
    {
      MoveWindow(_miaoTree, gap * 2, top + MiaoZip::Theme::Scale(*this, 37),
          MyMax(sidebar - gap * 2, 0), MyMax(height - MiaoZip::Theme::Scale(*this, 49), 0), TRUE);
      ShowWindow(_miaoTree, SW_SHOW);
    }
    else ShowWindow(_miaoTree, SW_HIDE);
  }
  const int left = sidebar ? sidebar + gap * 2 : gap;
  const int width = MyMax(xSize - left - gap, 0);
  _listView.Move(left + 1, top + 1, MyMax(width - 2, 0), MyMax(height - 2, 0));
  // The name column takes the remaining room; other columns retain their user widths.
  if ((_listView.GetStyle() & LVS_TYPEMASK) == LVS_REPORT)
  {
    const HWND header = ListView_GetHeader(_listView);
    const int count = Header_GetItemCount(header);
    if (count > 0 && count <= 5)
    {
      int remaining = MyMax(width - GetSystemMetrics(SM_CXVSCROLL) - 2, 0);
      for (int column = 1; column < count; ++column)
        remaining -= ListView_GetColumnWidth(_listView, column);
      const int nameWidth = MyMax(MiaoZip::Theme::Scale(*this, 180), remaining);
      if (ListView_GetColumnWidth(_listView, 0) != nameWidth)
        ListView_SetColumnWidth(_listView, 0, nameWidth);
    }
  }
  if (width > 0 && height > 0)
    SetWindowRgn(_listView, CreateRoundRectRgn(0, 0, width - 1, height - 1,
        MiaoZip::Theme::Scale(*this, 16), MiaoZip::Theme::Scale(*this, 16)), TRUE);
  if (_statusBarWindow)
    ::SetWindowPos(_statusBarWindow, nullptr, 0, MyMax(ySize - footer, 0), xSize, footer, SWP_SHOWWINDOW);
  ::InvalidateRect(*this, nullptr, FALSE);
}

bool CPanel::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  if ((HWND)*this == 0)
    return true;

  if (_addressBarWindow)
  {
      int ControlDpi = ::GetDpiForWindow(_addressBarWindow);
      ::SetWindowPos(
          _addressBarWindow,
          nullptr,
          MulDiv(8, ControlDpi, 96),
          MulDiv(6, ControlDpi, 96),
          MyMax(xSize - MulDiv(16, ControlDpi, 96), 0),
          MulDiv(44, ControlDpi, 96),
          SWP_SHOWWINDOW
      );
  }

  ChangeWindowSize(xSize, ySize);
  return true;
}

bool CPanel::OnNotifyReBar(LPNMHDR header, LRESULT & /* result */)
{
  switch (header->code)
  {
    case RBN_HEIGHTCHANGE:
    {
      RECT rect;
      GetWindowRect(&rect);
      ChangeWindowSize(RECT_SIZE_X(rect), RECT_SIZE_Y(rect));
      return false;
    }
  }
  return false;
}

/*
UInt32 g_OnNotify = 0;
UInt32 g_LVIF_TEXT = 0;
UInt32 g_Time = 0;

void Print_OnNotify(const char *name)
{
  char s[256];
  DWORD tim = GetTickCount();
  sprintf(s,
      "Time = %7u ms, Notify = %9u, TEXT = %9u, %s",
      tim - g_Time,
      g_OnNotify,
      g_LVIF_TEXT,
      name);
  g_Time = tim;
  OutputDebugStringA(s);
  g_OnNotify = 0;
  g_LVIF_TEXT = 0;
}
*/

bool CPanel::OnNotify(UINT /* controlID */, LPNMHDR header, LRESULT &result)
{
  /*
  g_OnNotify++;

  if (header->hwndFrom == _listView)
  {
    if (header->code == LVN_GETDISPINFOW)
    {
      LV_DISPINFOW *dispInfo = (LV_DISPINFOW *)header;
        if ((dispInfo->item.mask & LVIF_TEXT))
          g_LVIF_TEXT++;
    }
  }
  */

  if (NotifyMiaoZipTree(header, result)) return true;
  if (!_processNotify)
    return false;

  else if (header->hwndFrom == _listView)
    return OnNotifyList(header, result);
  else if (::GetParent(header->hwndFrom) == _listView)
  {
    // NMHDR:code is UINT
    // NM_RCLICK is unsigned in windows sdk
    // NM_RCLICK is int      in MinGW
    if (header->code == (UINT)NM_RCLICK)
      return OnRightClick((MY_NMLISTVIEW_NMITEMACTIVATE *)header, result);
  }
  return false;
}

bool CPanel::OnCommand(int code, int itemID, LPARAM lParam, LRESULT &result)
{
  if (itemID == kParentFolderID)
  {
    OpenParentFolder();
    result = 0;
    return true;
  }
  /*
  if (itemID == kCreateFolderID)
  {
    CreateFolder();
    result = 0;
    return true;
  }
  */
  return CWindow2::OnCommand(code, itemID, lParam, result);
}



/*
void CPanel::MessageBox_Info(LPCWSTR message, LPCWSTR caption) const
  { ::MessageBoxW((HWND)*this, message, caption, MB_OK); }
void CPanel::MessageBox_Warning(LPCWSTR message) const
  { ::MessageBoxW((HWND)*this, message, L"MiaoZip", MB_OK | MB_ICONWARNING); }
*/

void CPanel::MessageBox_Error_Caption(LPCWSTR message, LPCWSTR caption) const
  { ::MessageBoxW((HWND)*this, message, caption, MB_OK | MB_ICONSTOP); }

void CPanel::MessageBox_Error(LPCWSTR message) const
  { MessageBox_Error_Caption(message, L"MiaoZip"); }

static UString ErrorHResult_To_Message(HRESULT errorCode)
{
  if (errorCode == 0)
    errorCode = E_FAIL;
  return HResultToMessage(errorCode);
}

void CPanel::MessageBox_Error_HRESULT_Caption(HRESULT errorCode, LPCWSTR caption) const
{
  MessageBox_Error_Caption(ErrorHResult_To_Message(errorCode), caption);
}

void CPanel::MessageBox_Error_HRESULT(HRESULT errorCode) const
  { MessageBox_Error_HRESULT_Caption(errorCode, L"MiaoZip"); }

void CPanel::MessageBox_Error_2Lines_Message_HRESULT(LPCWSTR message, HRESULT errorCode) const
{
  UString m = message;
  m.Add_LF();
  m += ErrorHResult_To_Message(errorCode);
  MessageBox_Error(m);
}

void CPanel::MessageBox_LastError(LPCWSTR caption) const
  { MessageBox_Error_HRESULT_Caption(::GetLastError(), caption); }

void CPanel::MessageBox_LastError() const
  { MessageBox_LastError(L"MiaoZip"); }

void CPanel::MessageBox_Error_LangID(UINT resourceID) const
  { MessageBox_Error(LangString(resourceID)); }

void CPanel::MessageBox_Error_UnsupportOperation() const
  { MessageBox_Error_LangID(IDS_OPERATION_IS_NOT_SUPPORTED); }




void CPanel::SetFocusToList()
{
  _listView.SetFocus();
  // SetCurrentPathText();
}

void CPanel::SetFocusToLastRememberedItem()
{
  if (_lastFocusedIsList)
    SetFocusToList();
  /*
  else
    _headerComboBox.SetFocus();
    _addressBarControl.Focus(winrt::Windows::UI::Xaml::FocusState::Programmatic);
  */
}

UString CPanel::GetFolderTypeID() const
{
  {
    NCOM::CPropVariant prop;
    if (_folder->GetFolderProperty(kpidType, &prop) == S_OK)
      if (prop.vt == VT_BSTR)
        return (const wchar_t *)prop.bstrVal;
  }
  return UString();
}

bool CPanel::IsFolderTypeEqTo(const char *s) const
{
  return StringsAreEqual_Ascii(GetFolderTypeID(), s);
}

bool CPanel::IsRootFolder() const { return IsFolderTypeEqTo("RootFolder"); }
bool CPanel::IsFSFolder() const { return IsFolderTypeEqTo("FSFolder"); }
bool CPanel::IsFSDrivesFolder() const { return IsFolderTypeEqTo("FSDrives"); }
bool CPanel::IsAltStreamsFolder() const { return IsFolderTypeEqTo("AltStreamsFolder"); }
bool CPanel::IsArcFolder() const
{
  return GetFolderTypeID().IsPrefixedBy_Ascii_NoCase("NanaZip");
}

bool CPanel::IsHashFolder() const
{
  if (_folder)
  {
    NCOM::CPropVariant prop;
    if (_folder->GetFolderProperty(kpidIsHash, &prop) == S_OK)
      if (prop.vt == VT_BOOL)
        return VARIANT_BOOLToBool(prop.boolVal);
  }
  return false;
}

UString CPanel::GetFsPath() const
{
  if (IsFSDrivesFolder() && !IsDeviceDrivesPrefix() && !IsSuperDrivesPrefix())
    return UString();
  return _currentFolderPrefix;
}

UString CPanel::GetDriveOrNetworkPrefix() const
{
  if (!IsFSFolder())
    return UString();
  UString drive = GetFsPath();
  drive.DeleteFrom(NFile::NName::GetRootPrefixSize(drive));
  return drive;
}

void CPanel::SetListViewMode(UInt32 index)
{
  if (index >= 4)
    return;
  _ListViewMode = index;
  DWORD oldStyle = (DWORD)_listView.GetStyle();
  DWORD newStyle = kStyles[index];

  // DWORD tickCount1 = GetTickCount();
  if ((oldStyle & LVS_TYPEMASK) != newStyle)
    _listView.SetStyle((oldStyle & ~LVS_TYPEMASK) | newStyle);
  // RefreshListCtrlSaveFocused();
  /*
  DWORD tickCount2 = GetTickCount();
  char s[256];
  sprintf(s, "SetStyle = %5d",
      tickCount2 - tickCount1
      );
  OutputDebugStringA(s);
  */

}

void CPanel::ChangeFlatMode()
{
  _flatMode = !_flatMode;
  if (_parentFolders.Size() > 0)
    _flatModeForArc = _flatMode;
  else
    _flatModeForDisk = _flatMode;
  RefreshListCtrl_SaveFocused();
}

/*
void CPanel::Change_ShowNtfsStrems_Mode()
{
  _showNtfsStrems_Mode = !_showNtfsStrems_Mode;
  if (_parentFolders.Size() > 0)
    _showNtfsStrems_ModeForArc = _showNtfsStrems_Mode;
  else
    _showNtfsStrems_ModeForDisk = _showNtfsStrems_Mode;
  RefreshListCtrlSaveFocused();
}
*/

void CPanel::Post_Refresh_StatusBar()
{
  if (_processStatusBar)
    PostMsg(kRefresh_StatusBar);
}

// **************** NanaZip Modification Start ****************
void CPanel::AddToExistingArchive()
{
    if (!DoesItSupportOperations() || _parentFolders.Size() != 1)
    {
        MessageBox_Error_UnsupportOperation();
        return;
    }

    CFolderLink &Link = _parentFolders.Back();
    const UString &ArchivePath = Link.VirtualPath;

    // MyBrowseForFile doesn't have multiselect, so use IFileOpenDialog directly
    CMyComPtr<IFileOpenDialog> FileDialog;
    if (FAILED(::CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&FileDialog))))
    {
        return;
    }

    FILEOPENDIALOGOPTIONS DialogOptions;
    if (FAILED(FileDialog->GetOptions(&DialogOptions)))
    {
        return;
    }
    DialogOptions |= FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM;
    if (FAILED(FileDialog->SetOptions(DialogOptions)))
    {
        return;
    }
    {
        CMyComPtr<IShellItem> DefaultFolder;
        if (SUCCEEDED(::SHCreateItemFromParsingName(
            ArchivePath,
            nullptr,
            IID_PPV_ARGS(&DefaultFolder))))
        {
            FileDialog->SetDefaultFolder(DefaultFolder);
        }
    }

    CMyComPtr<IShellItemArray> Items;
    HRESULT dialogResult;
    {
        MiaoZip::SystemDialogThemeScope systemTheme;
        dialogResult = FileDialog->Show(GetParent());
    }
    if (FAILED(dialogResult) ||
        FAILED(FileDialog->GetResults(&Items)))
    {
        return;
    }

    UStringVector SelectedPaths;
    DWORD Count;
    if (FAILED(Items->GetCount(&Count)))
    {
        return;
    }
    for (DWORD i = 0; i < Count; ++i)
    {
        CMyComPtr<IShellItem> Item;
        if (SUCCEEDED(Items->GetItemAt(
            i,
            &Item)))
        {
            LPWSTR DisplayName = nullptr;
            if (SUCCEEDED(Item->GetDisplayName(
                SIGDN_FILESYSPATH,
                &DisplayName)))
            {
                SelectedPaths.Add(UString(DisplayName));
                ::CoTaskMemFree(DisplayName);
            }
        }
    }

    if (SelectedPaths.Size() == 0)
    {
        return;
    }
    // we can't use CompressFiles here, since that uses NanaZipG which doesn't
    // support adding files to an archive subdirectory, and there's no way to
    // just use the dialog from within FM without actually committing files
    CopyFromAsk(SelectedPaths);
}
// **************** NanaZip Modification End ****************

// **************** NanaZip Modification Start ****************
void CPanel::CreateArchive()
{
    UStringVector names;
    UString initialFolder;
    if (Is_IO_FS_Folder())
    {
        initialFolder = GetFsPath();
        CRecordVector<UInt32> selected;
        GetSelectedItemsIndices(selected);
        FOR_VECTOR(i, selected) names.Add(initialFolder + GetItemRelPath2(selected[i]));
    }
    else if (!_parentFolders.IsEmpty())
    {
        initialFolder = _parentFolders.Front().VirtualPath;
        const int slash = initialFolder.ReverseFind_PathSepar();
        if (slash >= 0) initialFolder.DeleteFrom(static_cast<unsigned>(slash + 1));
        else initialFolder.Empty();
    }
    if (names.IsEmpty())
    {
        CMyComPtr<IFileOpenDialog> dialog;
        HRESULT result = ::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(result)) { MessageBox_Error_HRESULT(result); return; }
        FILEOPENDIALOGOPTIONS options{};
        result = dialog->GetOptions(&options);
        if (SUCCEEDED(result)) result = dialog->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
        if (FAILED(result)) { MessageBox_Error_HRESULT(result); return; }
        dialog->SetTitle(L"创建压缩包：选择文件");
        dialog->SetOkButtonLabel(L"选择");
        if (!initialFolder.IsEmpty())
        {
            CMyComPtr<IShellItem> folder;
            if (SUCCEEDED(::SHCreateItemFromParsingName(initialFolder, nullptr, IID_PPV_ARGS(&folder)))) dialog->SetDefaultFolder(folder);
        }
        {
            MiaoZip::SystemDialogThemeScope systemTheme;
            result = dialog->Show(GetParent());
        }
        if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return;
        if (FAILED(result)) { MessageBox_Error_HRESULT(result); return; }
        CMyComPtr<IShellItemArray> items;
        result = dialog->GetResults(&items);
        DWORD count = 0;
        if (SUCCEEDED(result)) result = items->GetCount(&count);
        if (FAILED(result)) { MessageBox_Error_HRESULT(result); return; }
        for (DWORD index = 0; index < count; ++index)
        {
            CMyComPtr<IShellItem> item;
            result = items->GetItemAt(index, &item);
            LPWSTR path = nullptr;
            if (SUCCEEDED(result)) result = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (FAILED(result)) { MessageBox_Error_HRESULT(result); return; }
            names.Add(UString(path)); ::CoTaskMemFree(path);
        }
    }
    if (names.IsEmpty()) return;
    auto directory = std::filesystem::path(names.Front().Ptr()).parent_path();
    std::wstring base = CreateArchiveName(names).Ptr();
    auto available = base;
    for (unsigned suffix = 2;; ++suffix)
    {
        bool exists = false;
        for (const auto extension : {L".7z", L".zip", L".tar", L".wim", L".gz", L".xz", L".bz2", L".zst"})
        {
            std::error_code error;
            exists = exists || std::filesystem::exists(directory / (available + extension), error);
        }
        if (!exists) break;
        available = base + L" (" + std::to_wstring(suffix) + L")";
    }
    UString prefix(directory.c_str()); prefix.Add_PathSepar();
    const auto result = CompressFiles(prefix, UString(available.c_str()), L"", true, names, false, true, false);
    if (FAILED(result)) MessageBox_Error_HRESULT(result);
}
// **************** NanaZip Modification End ****************

void CPanel::AddToArchive()
{
  CRecordVector<UInt32> indices;
  GetOperatedItemIndices(indices);
  if (!Is_IO_FS_Folder())
  {
    // **************** NanaZip Modification Start ****************
    // MessageBox_Error_UnsupportOperation();
    if (IsArcFolder())
    {
        AddToExistingArchive();
    }
    else
    {
        MessageBox_Error_UnsupportOperation();
    }
    // **************** NanaZip Modification End ****************
    return;
  }
  if (indices.Size() == 0)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  UStringVector names;

  const UString curPrefix = GetFsPath();
  UString destCurDirPrefix = curPrefix;
  if (IsFSDrivesFolder())
    destCurDirPrefix = ROOT_FS_FOLDER;

  FOR_VECTOR (i, indices)
    names.Add(curPrefix + GetItemRelPath2(indices[i]));

  const UString arcName = CreateArchiveName(names);

  HRESULT res = CompressFiles(destCurDirPrefix, arcName, L"",
      true, // addExtension
      names, false, true, false);
  if (res != S_OK)
  {
    if (destCurDirPrefix.Len() >= MAX_PATH)
      MessageBox_Error_LangID(IDS_MESSAGE_UNSUPPORTED_OPERATION_FOR_LONG_PATH_FOLDER);
  }
  // KillSelection();
}

// function from ContextMenu.cpp
UString GetSubFolderNameForExtract(const UString &arcPath);

static UString GetSubFolderNameForExtract2(const UString &arcPath)
{
  int slashPos = arcPath.ReverseFind_PathSepar();
  UString s;
  UString name = arcPath;
  if (slashPos >= 0)
  {
    s = arcPath.Left((unsigned)(slashPos + 1));
    name = arcPath.Ptr((unsigned)(slashPos + 1));
  }
  s += GetSubFolderNameForExtract(name);
  return s;
}

void CPanel::GetFilePaths(const CRecordVector<UInt32> &indices, UStringVector &paths, bool allowFolders)
{
  const UString prefix = GetFsPath();
  FOR_VECTOR (i, indices)
  {
    int index = indices[i];
    if (!allowFolders && IsItem_Folder(index))
    {
      paths.Clear();
      break;
    }
    paths.Add(prefix + GetItemRelPath2(index));
  }
  if (paths.Size() == 0)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
}

// **************** NanaZip Modification Start ****************
// Expose the existing smart extraction engine in the main file manager.
void CPanel::SmartExtractArchives()
{
    UStringVector paths;
    UString outFolder;
    if (this->_parentFolders.Size() > 0)
    {
        if (this->_parentFolders.Size() > 1)
        {
            this->MessageBox_Error_LangID(IDS_OPERATION_IS_NOT_SUPPORTED);
            return;
        }
        UString archivePath = this->_parentFolders[0].VirtualPath;
        paths.Add(archivePath);
        int separator = archivePath.ReverseFind_PathSepar();
        if (separator < 0)
        {
            return;
        }
        outFolder = archivePath.Left(separator + 1);
    }
    else
    {
        CRecordVector<UInt32> indices;
        this->GetOperatedItemIndices(indices);
        this->GetFilePaths(indices, paths);
        outFolder = this->GetFsPath();
    }
    if (paths.IsEmpty())
    {
        return;
    }
    outFolder.Add_PathSepar();
    CContextMenuInfo ContextMenuInfo;
    ContextMenuInfo.Load();
    ::ExtractArchives(
        paths,
        outFolder,
        true,  // Let the user review destination and collision behavior.
        false,
        ContextMenuInfo.WriteZone,
        true,  // Smart extraction, already implemented by NanaZip.
        true); // The existing dialog lets the user turn this off.
}
// **************** NanaZip Modification End ****************

void CPanel::ExtractArchives()
{
  if (_parentFolders.Size() > 0)
  {
    // **************** NanaZip Modification Start ****************
    // _panelCallback->OnCopy(false, false);
    CopyFromArchive();
    // **************** NanaZip Modification End ****************
    return;
  }
  CRecordVector<UInt32> indices;
  GetOperatedItemIndices(indices);
  UStringVector paths;
  GetFilePaths(indices, paths);
  if (paths.IsEmpty())
    return;

  UString outFolder = GetFsPath();
  if (indices.Size() == 1)
    outFolder += GetSubFolderNameForExtract2(GetItemRelPath(indices[0]));
  else
    outFolder += '*';
  outFolder.Add_PathSepar();

  CContextMenuInfo ci;
  ci.Load();

  ::ExtractArchives(paths, outFolder
      , true // showDialog
      , false // elimDup
      , ci.WriteZone
      );
}

// **************** NanaZip Modification Start ****************
void CPanel::CopyFromArchive()
{
    if (!this->m_DailyNameFilter.IsEmpty())
    {
        CRecordVector<UInt32> visibleIndices;
        this->GetAllItemIndices(visibleIndices);
        if (visibleIndices.IsEmpty())
        {
            this->MessageBox_Error_LangID(IDS_SELECT_FILES);
            return;
        }
    }
    UString archivePath = this->_parentFolders[0].VirtualPath;
    // An active filter also limits the default extraction when nothing is selected.
    bool wantsExtractAll = this->m_DailyNameFilter.IsEmpty();

    if (this->_parentFolders.Size() > 1)
    {
        wantsExtractAll = false;
    }

    CRecordVector<UInt32> indices;
    this->GetOperatedItemIndices(indices);
    if (indices.Size() > 0)
    {
        wantsExtractAll = false;
    }

    UString path = this->GetFsPath();
    if (::IsPathSepar(path.Back()))
    {
        path.DeleteBack();
    }
    if (path != archivePath)
    {
        wantsExtractAll = false;
    }

    if (wantsExtractAll || !this->_panelCallback->OnCopyOrExtract())
    {
        UStringVector paths;
        paths.Add(archivePath);
        UString outFolder = ::GetSubFolderNameForExtract2(archivePath);

        CContextMenuInfo ci;
        ci.Load();

        ::ExtractArchives(
            paths,
            outFolder,
            true, // showDialog
            false, // elimDup
            ci.WriteZone);
    }
}
// **************** NanaZip Modification End ****************

/*
static void AddValuePair(UINT resourceID, UInt64 value, UString &s)
{
  AddLangString(s, resourceID);
  char sz[32];
  s += ": ";
  ConvertUInt64ToString(value, sz);
  s += sz;
  s.Add_LF();
}

// now we don't need CThreadTest, since now we call CopyTo for "test command

class CThreadTest: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  CRecordVector<UInt32> Indices;
  CExtractCallbackImp *ExtractCallbackSpec;
  CMyComPtr<IFolderArchiveExtractCallback> ExtractCallback;
  CMyComPtr<IArchiveFolder> ArchiveFolder;
};

HRESULT CThreadTest::ProcessVirt()
{
  RINOK(ArchiveFolder->Extract(&Indices[0], Indices.Size(),
      true, // includeAltStreams
      false, // replaceAltStreamColon
      NExtract::NPathMode::kFullPathnames,
      NExtract::NOverwriteMode::kAskBefore,
      NULL, // path
      BoolToInt(true), // testMode
      ExtractCallback));
  if (ExtractCallbackSpec->IsOK())
  {
    UString s;
    AddValuePair(IDS_PROP_FOLDERS, ExtractCallbackSpec->NumFolders, s);
    AddValuePair(IDS_PROP_FILES, ExtractCallbackSpec->NumFiles, s);
    // AddValuePair(IDS_PROP_SIZE, ExtractCallbackSpec->UnpackSize, s);
    // AddSizePair(IDS_COMPRESSED_COLON, Stat.PackSize, s);
    s.Add_LF();
    AddLangString(s, IDS_MESSAGE_NO_ERRORS);
    FinalMessage.OkMessage.Message = s;
  }
  return S_OK;
}

static void AddSizePair(UInt32 langID, UInt64 value, UString &s)
{
  char sz[32];
  AddLangString(s, langID);
  s += L' ';
  ConvertUInt64ToString(value, sz);
  s += sz;
  ConvertUInt64ToString(value >> 20, sz);
  s += " (";
  s += sz;
  s += " MB)";
  s.Add_LF();
}
*/

void CPanel::TestArchives()
{
  CRecordVector<UInt32> indices;
  GetOperatedIndicesSmart(indices);
  CMyComPtr<IArchiveFolder> archiveFolder;
  _folder.QueryInterface(IID_IArchiveFolder, &archiveFolder);
  if (archiveFolder)
  {
    CCopyToOptions options;
    options.streamMode = true;
    options.showErrorMessages = true;
    options.testMode = true;

    UStringVector messages;
    HRESULT res = CopyTo(options, indices, &messages);
    if (res != S_OK)
    {
      if (res != E_ABORT)
        MessageBox_Error_HRESULT(res);
    }
    return;

    /*
    {
    CThreadTest extracter;

    extracter.ArchiveFolder = archiveFolder;
    extracter.ExtractCallbackSpec = new CExtractCallbackImp;
    extracter.ExtractCallback = extracter.ExtractCallbackSpec;
    extracter.ExtractCallbackSpec->ProgressDialog = &extracter.ProgressDialog;
    if (!_parentFolders.IsEmpty())
    {
      const CFolderLink &fl = _parentFolders.Back();
      extracter.ExtractCallbackSpec->PasswordIsDefined = fl.UsePassword;
      extracter.ExtractCallbackSpec->Password = fl.Password;
    }

    if (indices.IsEmpty())
      return;

    extracter.Indices = indices;

    UString title = LangString(IDS_PROGRESS_TESTING);

    extracter.ProgressDialog.CompressingMode = false;
    extracter.ProgressDialog.MainWindow = GetParent();
    extracter.ProgressDialog.MainTitle = "NanaZip"; // LangString(IDS_APP_TITLE);
    extracter.ProgressDialog.MainAddTitle = title + L' ';

    extracter.ExtractCallbackSpec->OverwriteMode = NExtract::NOverwriteMode::kAskBefore;
    extracter.ExtractCallbackSpec->Init();

    if (extracter.Create(title, GetParent()) != S_OK)
      return;

    }
    RefreshTitleAlways();
    return;
    */
  }

  if (!IsFSFolder())
  {
    MessageBox_Error_UnsupportOperation();
    return;
  }
  UStringVector paths;
  GetFilePaths(indices, paths, true);
  if (paths.IsEmpty())
    return;
  ::TestArchives(paths);
}

#include "../../../../../../MiaoZip.Features/FolderTree.inl"

#include "../../../../../../MiaoZip.Features/PanelTheme.inl"
