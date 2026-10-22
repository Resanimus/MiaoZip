// Presentation for standard application menu items only. Existing owner-drawn
// shell items and their data are preserved and remain the extension's concern.
#pragma once
#include "NativeDialogSurface.h"
#include "MenuModel.h"
#include <memory>
#include <oleacc.h>
#include <cwctype>

namespace MiaoZip::Menus
{
    struct Item
    {
        MSAAMENUINFO Accessible{}; // First member, as required by MSAA.
        std::wstring AccessibleName;
        HMENU Menu{};
        UINT Position = 0;
        UINT OriginalType = 0;
        ULONG_PTR OriginalData = 0;
        std::wstring Label;
        HMENU Submenu{};
        HBITMAP Bitmap{};
    };
    struct Context
    {
        std::vector<std::unique_ptr<Item>> Items;
        std::vector<std::pair<HMENU, HBRUSH>> Backgrounds;
        HFONT Font{};
        HBRUSH Brush{};
        ~Context()
        { if (Font) DeleteObject(Font); if (Brush) DeleteObject(Brush); }
    };
    inline constexpr wchar_t Property[] = L"MiaoZip.MenuSurface";
    inline Context* Current(HWND owner)
    { return reinterpret_cast<Context*>(GetPropW(owner, Property)); }
    inline Item* Owned(HWND owner, ULONG_PTR data)
    {
        auto context = Current(owner);
        if (context) for (const auto& item : context->Items)
            if (reinterpret_cast<ULONG_PTR>(item.get()) == data) return item.get();
        return nullptr;
    }
    inline void Prepare(HWND owner, HMENU menu)
    {
        BOOL screenReader = FALSE;
        SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReader, 0);
        if (!IsMenu(menu) || GetPropW(owner, SnapshotProperty) || Theme::Current().HighContrast || screenReader) return;
        auto context = Current(owner);
        if (!context)
        {
            context = new Context;
            SetPropW(owner, Property, context);
            context->Font = CreateFontW(-Theme::Scale(owner, 13), 0, 0, 0, FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, Design::FontFamily);
            context->Brush = CreateSolidBrush(Theme::Colors().Content);
        }
        if (context->Items.empty())
        {
            if (context->Brush) DeleteObject(context->Brush);
            context->Brush = CreateSolidBrush(Theme::Colors().Content);
        }
        MENUINFO original{sizeof(original)}; original.fMask = MIM_BACKGROUND;
        if (GetMenuInfo(menu, &original)) context->Backgrounds.emplace_back(menu, original.hbrBack);
        for (int index = 0; index < GetMenuItemCount(menu); ++index)
        {
            wchar_t label[2048]{};
            MENUITEMINFOW info{sizeof(info)};
            info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STRING | MIIM_SUBMENU | MIIM_BITMAP;
            info.dwTypeData = label; info.cch = 2048;
            if (!GetMenuItemInfoW(menu, index, TRUE, &info) || (info.fType & MFT_OWNERDRAW) ||
                info.hbmpItem == HBMMENU_CALLBACK) continue;
            auto item = std::make_unique<Item>();
            item->Menu = menu; item->Position = index; item->OriginalType = info.fType;
            item->OriginalData = info.dwItemData; item->Label = label;
            item->Submenu = info.hSubMenu; item->Bitmap = info.hbmpItem;
            for (size_t letter = 0; letter < item->Label.size() && item->Label[letter] != L'\t'; ++letter)
            {
                if (item->Label[letter] == L'&' && letter + 1 < item->Label.size())
                {
                    if (item->Label[letter + 1] == L'&') ++letter;
                    else continue;
                }
                item->AccessibleName += item->Label[letter];
            }
            item->Accessible.dwMSAASignature = MSAA_MENU_SIG;
            item->Accessible.cchWText = static_cast<DWORD>(item->AccessibleName.size());
            item->Accessible.pszWText = item->AccessibleName.data();
            MENUITEMINFOW change{sizeof(change)};
            change.fMask = MIIM_FTYPE | MIIM_DATA;
            change.fType = info.fType | MFT_OWNERDRAW;
            change.dwItemData = reinterpret_cast<ULONG_PTR>(item.get());
            if (SetMenuItemInfoW(menu, index, TRUE, &change)) context->Items.push_back(std::move(item));
        }
        MENUINFO info{sizeof(info)}; info.fMask = MIM_BACKGROUND; info.hbrBack = context->Brush;
        SetMenuInfo(menu, &info);
    }
    inline void Restore(HWND owner, HMENU menu, bool all = false)
    {
        auto context = Current(owner); if (!context) return;
        auto& items = context->Items;
        for (auto i = items.begin(); i != items.end();)
        {
            auto& item = **i;
            if (!all && item.Menu != menu) { ++i; continue; }
            MENUITEMINFOW info{sizeof(info)}; info.fMask = MIIM_DATA;
            if (IsMenu(item.Menu) && GetMenuItemInfoW(item.Menu, item.Position, TRUE, &info) &&
                info.dwItemData == reinterpret_cast<ULONG_PTR>(&item))
            {
                info.fMask = MIIM_FTYPE | MIIM_DATA;
                info.fType = item.OriginalType; info.dwItemData = item.OriginalData;
                SetMenuItemInfoW(item.Menu, item.Position, TRUE, &info);
            }
            i = items.erase(i);
        }
        for (auto i = context->Backgrounds.begin(); i != context->Backgrounds.end();)
        {
            if (!all && i->first != menu) { ++i; continue; }
            if (IsMenu(i->first))
            {
                MENUINFO background{sizeof(background)};
                background.fMask = MIM_BACKGROUND; background.hbrBack = i->second;
                SetMenuInfo(i->first, &background);
            }
            i = context->Backgrounds.erase(i);
        }
        if (all) { RemovePropW(owner, Property); delete context; }
    }
    inline bool Character(HWND owner, WPARAM key, HMENU menu, LRESULT& result)
    {
        const auto context = Current(owner);
        if (!context) return false;
        std::vector<UINT> matches;
        int highlighted = -1;
        for (const auto& item : context->Items)
        {
            if (item->Menu != menu) continue;
            const auto state = GetMenuState(menu, item->Position, MF_BYPOSITION);
            if (state & MFS_HILITE) highlighted = item->Position;
            if (state & (MFS_DISABLED | MFS_GRAYED)) continue;
            for (size_t index = 0; index + 1 < item->Label.size(); ++index)
            {
                if (item->Label[index] != L'&') continue;
                if (item->Label[index + 1] == L'&') { ++index; continue; }
                if (std::towupper(item->Label[index + 1]) == std::towupper(LOWORD(key)))
                { matches.push_back(item->Position); break; }
            }
        }
        if (matches.empty()) return false;
        UINT chosen = matches.front();
        for (auto position : matches) if (static_cast<int>(position) > highlighted) { chosen = position; break; }
        result = MAKELRESULT(chosen, matches.size() == 1 ? MNC_EXECUTE : MNC_SELECT);
        return true;
    }

    inline bool Measure(HWND owner, MEASUREITEMSTRUCT* measure)
    {
        if (!measure || measure->CtlType != ODT_MENU) return false;
        const auto item = Owned(owner, measure->itemData); if (!item) return false;
        HDC dc = GetDC(owner); auto old = SelectObject(dc, Current(owner)->Font);
        SIZE size{}; GetTextExtentPoint32W(dc, item->Label.c_str(), static_cast<int>(item->Label.size()), &size);
        SelectObject(dc, old); ReleaseDC(owner, dc);
        measure->itemHeight = Theme::Scale(owner, (item->OriginalType & MFT_SEPARATOR) ? 12 : 34);
        measure->itemWidth = size.cx + Theme::Scale(owner, 76);
        return true;
    }
    inline bool Draw(HWND owner, DRAWITEMSTRUCT* draw)
    {
        if (!draw || draw->CtlType != ODT_MENU) return false;
        const auto item = Owned(owner, draw->itemData); if (!item) return false;
        auto dc = draw->hDC; const auto p = Theme::Colors(); auto rect = draw->rcItem;
        Theme::Fill(dc, rect, p.Content);
        const auto scale = [owner](int value){return Theme::Scale(owner, value);};
        if (item->OriginalType & MFT_SEPARATOR)
        {
            Dialogs::Line(dc, rect.left + scale(12), (rect.top + rect.bottom) / 2,
                rect.right - scale(12), (rect.top + rect.bottom) / 2, p.Line); return true;
        }
        const bool disabled = (draw->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
        if ((draw->itemState & ODS_SELECTED) && !disabled)
        { auto highlight = rect; InflateRect(&highlight,-scale(4),-scale(2)); Theme::Rounded(dc,highlight,p.Selected,scale(12)); }
        auto oldFont = SelectObject(dc, Current(owner)->Font);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,disabled ? p.Muted : p.Text);
        const int mid = (rect.top + rect.bottom) / 2;
        if (draw->itemState & ODS_CHECKED)
        {
            Dialogs::Line(dc,rect.left+scale(13),mid,rect.left+scale(17),mid+scale(4),p.Accent,scale(2));
            Dialogs::Line(dc,rect.left+scale(17),mid+scale(4),rect.left+scale(24),mid-scale(4),p.Accent,scale(2));
        }
        else if (reinterpret_cast<ULONG_PTR>(item->Bitmap) > 16)
        {
            DrawStateW(dc,nullptr,nullptr,reinterpret_cast<LPARAM>(item->Bitmap),0,
                rect.left+scale(12),mid-scale(8),scale(16),scale(16),DST_BITMAP | (disabled ? DSS_DISABLED : DSS_NORMAL));
        }
        auto label = rect; label.left += scale(36); label.right -= scale(30);
        const auto tab = item->Label.find(L'\t');
        const auto main = item->Label.substr(0,tab);
        UINT flags = DT_SINGLELINE | DT_VCENTER;
        if (draw->itemState & ODS_NOACCEL) flags |= DT_HIDEPREFIX;
        DrawTextW(dc,main.c_str(),-1,&label,flags);
        if (tab != std::wstring::npos)
        { SetTextColor(dc,p.Muted); DrawTextW(dc,item->Label.c_str()+tab+1,-1,&label,flags|DT_RIGHT); }
        if (item->Submenu)
        {
            const int x=rect.right-scale(15);
            Dialogs::Line(dc,x-scale(2),mid-scale(4),x+scale(2),mid,p.Muted);
            Dialogs::Line(dc,x+scale(2),mid,x-scale(2),mid+scale(4),p.Muted);
        }
        SelectObject(dc,oldFont); return true;
    }
}
