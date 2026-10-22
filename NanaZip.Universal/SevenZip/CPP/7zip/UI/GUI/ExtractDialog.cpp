// ExtractDialog.cpp

#include "StdAfx.h"
#include "../../../../../../MiaoZip.Features/ExtractionPreferences.h"
#include "../../../../../../MiaoZip.Features/Theme.h"
#include "../../../../../../MiaoZip.Features/ExtractDialogLayout.h"
#include "../../../../../../MiaoZip.Features/NativeDialogSurface.h"
#include <windowsx.h>

#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/ResourceString.h"

// **************** NanaZip Modification Start ****************
//#ifndef Z7_NO_REGISTRY
//#include "../FileManager/HelpUtils.h"
//#endif
// **************** NanaZip Modification End ****************


#include "../FileManager/BrowseDialog.h"
#include "../FileManager/LangUtils.h"
#include "../FileManager/resourceGui.h"

#include "ExtractDialog.h"
#include "ExtractDialogRes.h"
#include "ExtractRes.h"

using namespace NWindows;
using namespace NFile;
using namespace NName;

extern HINSTANCE g_hInstance;

#ifndef Z7_SFX

static const UInt32 kPathMode_IDs[] =
{
  IDS_EXTRACT_PATHS_FULL,
  IDS_EXTRACT_PATHS_NO,
  IDS_EXTRACT_PATHS_ABS
};

static const UInt32 kOverwriteMode_IDs[] =
{
  IDS_EXTRACT_OVERWRITE_ASK,
  IDS_EXTRACT_OVERWRITE_WITHOUT_PROMPT,
  IDS_EXTRACT_OVERWRITE_SKIP_EXISTING,
  IDS_EXTRACT_OVERWRITE_RENAME,
  IDS_EXTRACT_OVERWRITE_RENAME_EXISTING
};

static const
  // NExtract::NPathMode::EEnum
  int
  kPathModeButtonsVals[] =
{
  NExtract::NPathMode::kFullPaths,
  NExtract::NPathMode::kNoPaths,
  NExtract::NPathMode::kAbsPaths
};

static const
  int
  // NExtract::NOverwriteMode::EEnum
  kOverwriteButtonsVals[] =
{
  NExtract::NOverwriteMode::kAsk,
  NExtract::NOverwriteMode::kOverwrite,
  NExtract::NOverwriteMode::kSkip,
  NExtract::NOverwriteMode::kRename,
  NExtract::NOverwriteMode::kRenameExisting
};

#endif

#ifdef Z7_LANG

static const UInt32 kLangIDs[] =
{
  IDT_EXTRACT_EXTRACT_TO,
  IDT_EXTRACT_PATH_MODE,
  IDT_EXTRACT_OVERWRITE_MODE,
  // **************** 7-Zip ZS Modification Start ****************
  IDX_EXTRACT_OPEN_TRG_FLD,
  // **************** 7-Zip ZS Modification End ****************
  // IDX_EXTRACT_ALT_STREAMS,
  IDX_EXTRACT_NT_SECUR,
  IDX_EXTRACT_ELIM_DUP,
  // **************** NanaZip Modification Start ****************
  IDX_EXTRACT_OPEN_FOLDER,
  // **************** NanaZip Modification End ****************
  IDG_PASSWORD,
  IDX_PASSWORD_SHOW
};
#endif

// static const int kWildcardsButtonIndex = 2;

#ifndef Z7_NO_REGISTRY
static const unsigned kHistorySize = 16;
#endif

#ifndef Z7_SFX

// it's used in CompressDialog also
void AddComboItems(NControl::CComboBox &combo, const UInt32 *langIDs, unsigned numItems, const int *values, int curVal);
void AddComboItems(NControl::CComboBox &combo, const UInt32 *langIDs, unsigned numItems, const int *values, int curVal)
{
  unsigned curSel = 0;
  for (unsigned i = 0; i < numItems; i++)
  {
    UString s = LangString(langIDs[i]);
    s.RemoveChar(L'&');
    combo.AddString_SetItemData(s, (LPARAM)i);
    if (values[i] == curVal)
      curSel = i;
  }
  combo.SetCurSel(curSel);
}

// it's used in CompressDialog also
bool GetBoolsVal(const CBoolPair &b1, const CBoolPair &b2);
bool GetBoolsVal(const CBoolPair &b1, const CBoolPair &b2)
{
  if (b1.Def) return b1.Val;
  if (b2.Def) return b2.Val;
  return b1.Val;
}

void CExtractDialog::CheckButton_TwoBools(UINT id, const CBoolPair &b1, const CBoolPair &b2)
{
  CheckButton(id, GetBoolsVal(b1, b2));
}

void CExtractDialog::GetButton_Bools(UINT id, CBoolPair &b1, CBoolPair &b2)
{
  const bool val = IsButtonCheckedBool(id);
  const bool oldVal = GetBoolsVal(b1, b2);
  if (val != oldVal)
    b1.Def = b2.Def = true;
  b1.Val = b2.Val = val;
}

#endif

bool CExtractDialog::OnInit()
{
  #ifdef Z7_LANG
  {
    UString s;
    LangString_OnlyFromLangFile(IDD_EXTRACT, s);
    if (s.IsEmpty())
      GetText(s);
    if (!ArcPath.IsEmpty())
    {
      s += " : ";
      s += ArcPath;
    }
    SetText(s);
    // LangSetWindowText(*this, IDD_EXTRACT);
    LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  }
  #endif

  #ifndef Z7_SFX
  _passwordControl.Attach(GetItem(IDE_EXTRACT_PASSWORD));
  _passwordControl.SetText(Password);
  _passwordControl.SetPasswordChar(TEXT('*'));
  _pathName.Attach(GetItem(IDE_EXTRACT_NAME));
  #endif

  #ifdef Z7_NO_REGISTRY

  PathMode = NExtract::NPathMode::kFullPaths;
  OverwriteMode = NExtract::NOverwriteMode::kAsk;

  #else

  _info.Load();

  if (_info.PathMode == NExtract::NPathMode::kCurPaths)
    _info.PathMode = NExtract::NPathMode::kFullPaths;

  if (!PathMode_Force && _info.PathMode_Force)
    PathMode = _info.PathMode;
  if (!OverwriteMode_Force && _info.OverwriteMode_Force)
    OverwriteMode = _info.OverwriteMode;

  // CheckButton_TwoBools(IDX_EXTRACT_ALT_STREAMS, AltStreams, _info.AltStreams);
  // **************** 7-Zip ZS Modification Start ****************
  CheckButton_TwoBools(IDX_EXTRACT_OPEN_TRG_FLD, OpnTrgFold, _info.OpnTrgFold);
  // **************** 7-Zip ZS Modification End ****************
  CheckButton_TwoBools(IDX_EXTRACT_NT_SECUR,    NtSecurity, _info.NtSecurity);
  CheckButton_TwoBools(IDX_EXTRACT_ELIM_DUP,    ElimDup,    _info.ElimDup);
  // **************** NanaZip Modification Start ****************
  CheckButton_TwoBools(IDX_EXTRACT_OPEN_FOLDER, OpenFolder, _info.OpenFolder);
  // **************** NanaZip Modification End ****************

  CheckButton(IDX_PASSWORD_SHOW, _info.ShowPassword.Val);
  UpdatePasswordControl();

  #endif

  _path.Attach(GetItem(IDC_EXTRACT_PATH));

  UString pathPrefix = DirPath;

  #ifndef Z7_SFX

  // **************** NanaZip Modification Start ****************
  #if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  if (_info.SplitDest.Val)
  {
    CheckButton(IDX_EXTRACT_NAME_ENABLE, true);
    UString pathName;
    SplitPathToParts_Smart(DirPath, pathPrefix, pathName);
    if (pathPrefix.IsEmpty())
      pathPrefix = pathName;
    else
      _pathName.SetText(pathName);
  }
  else
    ShowItem_Bool(IDE_EXTRACT_NAME, false);
  #endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  UString pathName;
  SplitPathToParts_Smart(DirPath, pathPrefix, pathName);
  if (pathPrefix.IsEmpty())
    pathPrefix = pathName;
  else
    _pathName.SetText(pathName);

  if (_info.SplitDest.Val)
    CheckButton(IDX_EXTRACT_NAME_ENABLE, true);
  else
    ShowItem_Bool(IDE_EXTRACT_NAME, false);
  // **************** NanaZip Modification End ****************

  #endif

  _path.SetText(pathPrefix);

  #ifndef Z7_NO_REGISTRY
  for (unsigned i = 0; i < _info.Paths.Size() && i < kHistorySize; i++)
    _path.AddString(_info.Paths[i]);
  #endif

  /*
  if (_info.Paths.Size() > 0)
    _path.SetCurSel(0);
  else
    _path.SetCurSel(-1);
  */

  #ifndef Z7_SFX

  _pathMode.Attach(GetItem(IDC_EXTRACT_PATH_MODE));
  _overwriteMode.Attach(GetItem(IDC_EXTRACT_OVERWRITE_MODE));

  AddComboItems(_pathMode, kPathMode_IDs, Z7_ARRAY_SIZE(kPathMode_IDs), kPathModeButtonsVals, PathMode);
  AddComboItems(_overwriteMode, kOverwriteMode_IDs, Z7_ARRAY_SIZE(kOverwriteMode_IDs), kOverwriteButtonsVals, OverwriteMode);

  #endif

  HICON icon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON));
  SetIcon(ICON_BIG, icon);

  // CWindow filesWindow = GetItem(IDC_EXTRACT_RADIO_FILES);
  // filesWindow.Enable(_enableFilesButton);

#ifndef Z7_SFX
  MiaoZipManualSubfolder = IsButtonCheckedBool(IDX_EXTRACT_NAME_ENABLE);
  SetWindowTextW(GetItem(IDX_MIAOZIP_ORGANIZE), L"自动整理外层文件夹");
  CheckButton(IDX_MIAOZIP_ORGANIZE, SmartExtract.Def ? SmartExtract.Val : MiaoZip::Extraction::OrganizeOuterFolder());
  UpdateMiaoZipOrganization(false);
#endif
#ifndef Z7_SFX
  // **************** NanaZip Modification Start ****************
  SetPropW(*this, L"MiaoZip.DialogLayout", reinterpret_cast<HANDLE>(1));
  SetWindowTextW(*this, L"解压 — MiaoZip");
  SetWindowTextW(GetItem(IDT_EXTRACT_EXTRACT_TO), L"目标文件夹");
  SetWindowTextW(GetItem(IDX_EXTRACT_NAME_ENABLE), L"解压到指定子文件夹");
  SetWindowTextW(GetItem(IDB_EXTRACT_SET_PATH), L"浏览…");
  SetWindowTextW(GetItem(IDG_MIAOZIP_PATH_OPTIONS), L"路径与文件");
  SetWindowTextW(GetItem(IDG_MIAOZIP_COMPLETION), L"完成选项");
  SetWindowTextW(GetItem(IDX_MIAOZIP_EXIT), L"完成后自动关闭 MiaoZip");
  SetWindowTextW(GetItem(IDOK), L"解压");
  CheckButton(IDX_MIAOZIP_EXIT, MiaoZip::Extraction::ExitAfterExtraction());
  SetWindowLongPtrW(*this, GWL_STYLE, GetWindowLongPtrW(*this, GWL_STYLE) | WS_THICKFRAME | WS_MAXIMIZEBOX);
  const UINT dpi = MiaoZip::Theme::Dpi(*this);
  RECT size{0, 0, MiaoZip::Theme::Scale(*this, 780), MiaoZip::Theme::Scale(*this, 612)};
  MiaoZip::Theme::AdjustForDpi(&size, static_cast<DWORD>(GetWindowLongPtrW(*this, GWL_STYLE)), FALSE,
      static_cast<DWORD>(GetWindowLongPtrW(*this, GWL_EXSTYLE)), dpi);
  MONITORINFO monitor{sizeof(monitor)};
  GetMonitorInfoW(MonitorFromWindow(*this, MONITOR_DEFAULTTONEAREST), &monitor);
  const auto& work = monitor.rcWork;
  const int width = (std::min)(size.right - size.left, work.right - work.left - 24L);
  const int height = (std::min)(size.bottom - size.top, work.bottom - work.top - 24L);
  MiaoZipLayoutReady = true;
  SetWindowPos(*this, nullptr, work.left + (work.right - work.left - width) / 2,
      work.top + (work.bottom - work.top - height) / 2, width, height,
      SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  LayoutMiaoZipForm();
  // **************** NanaZip Modification End ****************
#endif
  NormalizePosition();

  return CModalDialog::OnInit();
}

#ifndef Z7_SFX
// **************** NanaZip Modification Start ****************
void CExtractDialog::LayoutMiaoZipForm()
{
    if (!MiaoZipLayoutReady || MiaoZipLayingOut) return;
    MiaoZipLayingOut = true;
    RECT client{}; ::GetClientRect(*this, &client);
    const auto dpi = MiaoZip::Theme::Dpi(*this);
    const auto layout = MiaoZip::Dialogs::ArrangeExtraction(
        MulDiv(client.right, 96, dpi), MulDiv(client.bottom, 96, dpi));
    MiaoZipScroll = (std::clamp<int>)(MiaoZipScroll, 0, layout.ScrollMaximum);
    SCROLLINFO scroll{sizeof(scroll), SIF_RANGE | SIF_PAGE | SIF_POS};
    scroll.nMax = layout.ContentHeight - 1;
    scroll.nPage = layout.ViewportHeight;
    scroll.nPos = MiaoZipScroll;
    SetScrollInfo(*this, SB_VERT, &scroll, TRUE);
    for (const auto& item : layout.Items)
    {
        const HWND child = GetItem(item.Id);
        const bool combo = item.Id == IDC_EXTRACT_PATH || item.Id == IDC_EXTRACT_PATH_MODE || item.Id == IDC_EXTRACT_OVERWRITE_MODE;
        SetWindowPos(child, nullptr, MulDiv(item.X, dpi, 96),
            MulDiv(item.Y - (item.Footer ? 0 : MiaoZipScroll), dpi, 96), MulDiv(item.Width, dpi, 96),
            MulDiv(combo ? 240 : item.Height, dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        if (combo) MiaoZip::Dialogs::SetComboHeight(child, MulDiv(item.Height, dpi, 96));
        if (!item.Footer)
        {
            RECT actual{}; ::GetWindowRect(child, &actual);
            MapWindowPoints(nullptr, *this, reinterpret_cast<POINT*>(&actual), 2);
            RECT viewport = client; viewport.bottom -= MulDiv(76, dpi, 96);
            RECT clip{};
            if (!IntersectRect(&clip, &actual, &viewport)) SetRectEmpty(&clip);
            else OffsetRect(&clip, -actual.left, -actual.top);
            auto region = CreateRectRgnIndirect(&clip);
            if (!SetWindowRgn(child, region, TRUE)) DeleteObject(region);
        }
    }
    MiaoZipLayingOut = false;
    ::InvalidateRect(*this, nullptr, TRUE);
}

bool CExtractDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_GETMINMAXINFO)
    {
        auto info = reinterpret_cast<MINMAXINFO*>(lParam);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(*this, MONITOR_DEFAULTTONEAREST), &monitor);
        info->ptMinTrackSize = {
            (std::min<LONG>)(MiaoZip::Theme::Scale(*this, 500), monitor.rcWork.right - monitor.rcWork.left - 24),
            (std::min<LONG>)(MiaoZip::Theme::Scale(*this, 400), monitor.rcWork.bottom - monitor.rcWork.top - 24)};
        return true;
    }
    if (MiaoZipLayoutReady && (message == WM_SIZE || message == WM_APP + 0x2b1)) LayoutMiaoZipForm();
    if (MiaoZipLayoutReady && message == WM_MOUSEWHEEL)
    {
        MiaoZipScroll -= GET_WHEEL_DELTA_WPARAM(wParam) * 48 / WHEEL_DELTA;
        LayoutMiaoZipForm(); return true;
    }
    if (MiaoZipLayoutReady && message == WM_VSCROLL)
    {
        switch (LOWORD(wParam))
        {
        case SB_LINEUP: MiaoZipScroll -= 32; break;
        case SB_LINEDOWN: MiaoZipScroll += 32; break;
        case SB_PAGEUP: MiaoZipScroll -= 180; break;
        case SB_PAGEDOWN: MiaoZipScroll += 180; break;
        case SB_THUMBTRACK:
        { SCROLLINFO info{sizeof(info), SIF_TRACKPOS}; GetScrollInfo(*this, SB_VERT, &info); MiaoZipScroll = info.nTrackPos; break; }
        default: break;
        }
        LayoutMiaoZipForm(); return true;
    }
    if (MiaoZipLayoutReady && message == WM_COMMAND &&
        (HIWORD(wParam) == EN_SETFOCUS || HIWORD(wParam) == BN_SETFOCUS || HIWORD(wParam) == CBN_SETFOCUS))
    {
        const int id = LOWORD(wParam);
        if (id != IDOK && id != IDCANCEL)
        {
            RECT focused{}; ::GetWindowRect(reinterpret_cast<HWND>(lParam), &focused);
            MapWindowPoints(nullptr, *this, reinterpret_cast<POINT*>(&focused), 2);
            RECT client{}; ::GetClientRect(*this, &client);
            const auto dpi = MiaoZip::Theme::Dpi(*this);
            const int top = MulDiv(focused.top, 96, dpi), bottom = MulDiv(focused.bottom, 96, dpi);
            const int edge = MulDiv(client.bottom, 96, dpi) - 84;
            if (top < 16) MiaoZipScroll += top - 16;
            else if (bottom > edge) MiaoZipScroll += bottom - edge;
            LayoutMiaoZipForm();
        }
    }
    return CModalDialog::OnMessage(message, wParam, lParam);
}
// **************** NanaZip Modification End ****************

void CExtractDialog::UpdateMiaoZipOrganization(bool rememberManual)
{
  const bool organize = IsButtonCheckedBool(IDX_MIAOZIP_ORGANIZE);
  if (organize && rememberManual) MiaoZipManualSubfolder = IsButtonCheckedBool(IDX_EXTRACT_NAME_ENABLE);
  CheckButton(IDX_EXTRACT_NAME_ENABLE, organize ? false : MiaoZipManualSubfolder);
  EnableItem(IDX_EXTRACT_NAME_ENABLE, !organize);
  ShowItem_Bool(IDE_EXTRACT_NAME, !organize && MiaoZipManualSubfolder);
}

void CExtractDialog::UpdatePasswordControl()
{
  _passwordControl.SetPasswordChar(IsShowPasswordChecked() ? 0 : TEXT('*'));
  UString password;
  _passwordControl.GetText(password);
  _passwordControl.SetText(password);
}
#endif

bool CExtractDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_EXTRACT_SET_PATH:
      OnButtonSetPath();
      return true;
    #ifndef Z7_SFX
    case IDX_MIAOZIP_ORGANIZE:
      UpdateMiaoZipOrganization(true);
      return true;
    case IDX_EXTRACT_NAME_ENABLE:
      ShowItem_Bool(IDE_EXTRACT_NAME, IsButtonCheckedBool(IDX_EXTRACT_NAME_ENABLE));
      return true;
    case IDX_PASSWORD_SHOW:
    {
      UpdatePasswordControl();
      return true;
    }
    #endif
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CExtractDialog::OnButtonSetPath()
{
  UString currentPath;
  _path.GetText(currentPath);
  UString title = LangString(IDS_EXTRACT_SET_FOLDER);
  UString resultPath;
  if (!MyBrowseForFolder(*this, title, currentPath, resultPath))
    return;
  #ifndef Z7_NO_REGISTRY
  _path.SetCurSel(-1);
  #endif
  _path.SetText(resultPath);
}

void AddUniqueString(UStringVector &list, const UString &s);
void AddUniqueString(UStringVector &list, const UString &s)
{
  FOR_VECTOR (i, list)
    if (s.IsEqualTo_NoCase(list[i]))
      return;
  list.Add(s);
}

void CExtractDialog::OnOK()
{
  #ifndef Z7_SFX
  ExitAfterExtraction = IsButtonCheckedBool(IDX_MIAOZIP_EXIT);
  MiaoZip::Extraction::SaveExitAfterExtraction(ExitAfterExtraction);
  SmartExtract.Def = true;
  SmartExtract.Val = IsButtonCheckedBool(IDX_MIAOZIP_ORGANIZE);
  MiaoZip::Extraction::SaveOrganizeOuterFolder(SmartExtract.Val);
  int pathMode2 = kPathModeButtonsVals[_pathMode.GetCurSel()];
  if (PathMode != NExtract::NPathMode::kCurPaths ||
      pathMode2 != NExtract::NPathMode::kFullPaths)
    PathMode = (NExtract::NPathMode::EEnum)pathMode2;

  OverwriteMode = (NExtract::NOverwriteMode::EEnum)kOverwriteButtonsVals[_overwriteMode.GetCurSel()];

  // _filesMode = (NExtractionDialog::NFilesMode::EEnum)GetFilesMode();

  _passwordControl.GetText(Password);

  #endif

  #ifndef Z7_NO_REGISTRY

  // GetButton_Bools(IDX_EXTRACT_ALT_STREAMS, AltStreams, _info.AltStreams);
  // **************** 7-Zip ZS Modification Start ****************
  GetButton_Bools(IDX_EXTRACT_OPEN_TRG_FLD, OpnTrgFold,  _info.OpnTrgFold);
  // **************** 7-Zip ZS Modification End ****************
  GetButton_Bools(IDX_EXTRACT_NT_SECUR,    NtSecurity, _info.NtSecurity);
  GetButton_Bools(IDX_EXTRACT_ELIM_DUP,    ElimDup,    _info.ElimDup);
  // **************** NanaZip Modification Start ****************
  GetButton_Bools(IDX_EXTRACT_OPEN_FOLDER, OpenFolder, _info.OpenFolder);
  // **************** NanaZip Modification End ****************

  bool showPassword = IsShowPasswordChecked();
  if (showPassword != _info.ShowPassword.Val)
  {
    _info.ShowPassword.Def = true;
    _info.ShowPassword.Val = showPassword;
  }

  if (_info.PathMode != pathMode2)
  {
    _info.PathMode_Force = true;
    _info.PathMode = (NExtract::NPathMode::EEnum)pathMode2;
    /*
    // we allow kAbsPaths in registry.
    if (_info.PathMode == NExtract::NPathMode::kAbsPaths)
      _info.PathMode = NExtract::NPathMode::kFullPaths;
    */
  }

  if (!OverwriteMode_Force && _info.OverwriteMode != OverwriteMode)
    _info.OverwriteMode_Force = true;
  _info.OverwriteMode = OverwriteMode;


  #else

  ElimDup.Val = IsButtonCheckedBool(IDX_EXTRACT_ELIM_DUP);

  #endif

  UString s;

  #ifdef Z7_NO_REGISTRY

  _path.GetText(s);

  #else

  int currentItem = _path.GetCurSel();
  if (currentItem == CB_ERR)
  {
    _path.GetText(s);
    if (_path.GetCount() >= (int)kHistorySize)
      currentItem = _path.GetCount() - 1;
  }
  else
    _path.GetLBText(currentItem, s);

  #endif

  s.Trim();
  NName::NormalizeDirPathPrefix(s);

  // **************** 7-Zip ZS Modification Start ****************
  DirPath = s; // s remains path without subpath (to store it to history below)
  // **************** 7-Zip ZS Modification End ****************
  #ifndef Z7_SFX

  const bool splitDest = IsButtonCheckedBool(IDX_EXTRACT_NAME_ENABLE);
  if (splitDest)
  {
    UString pathName;
    _pathName.GetText(pathName);
    pathName.Trim();
    // **************** 7-Zip ZS Modification Start ****************
    //s += pathName;
    //NName::NormalizeDirPathPrefix(s);
    DirPath += pathName;
    NName::NormalizeDirPathPrefix(DirPath);
    // **************** 7-Zip ZS Modification End ****************
  }
  const bool savedSplitDest = SmartExtract.Val ? MiaoZipManualSubfolder : splitDest;
  if (savedSplitDest != _info.SplitDest.Val)
  {
    _info.SplitDest.Def = true;
    _info.SplitDest.Val = savedSplitDest;
  }

  #endif

  // **************** 7-Zip ZS Modification Start ****************
  // DirPath = s;
  // **************** 7-Zip ZS Modification End ****************

  #ifndef Z7_NO_REGISTRY
  _info.Paths.Clear();
  #ifndef Z7_SFX
  AddUniqueString(_info.Paths, s);
  #endif
  for (int i = 0; i < _path.GetCount(); i++)
    if (i != currentItem)
    {
      UString sTemp;
      _path.GetLBText(i, sTemp);
      sTemp.Trim();
      AddUniqueString(_info.Paths, sTemp);
    }
  _info.Save();
  #endif

  CModalDialog::OnOK();
}

// **************** NanaZip Modification Start ****************
//#ifndef Z7_NO_REGISTRY
//#define kHelpTopic "fm/plugins/7-zip/extract.htm"
//void CExtractDialog::OnHelp()
//{
//  ShowHelpWindow(kHelpTopic);
//  CModalDialog::OnHelp();
//}
//#endif
// **************** NanaZip Modification End ****************
