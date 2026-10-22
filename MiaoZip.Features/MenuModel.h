// Snapshot application menus for XAML presentation without changing command IDs.
#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace MiaoZip::Menus
{
    inline constexpr wchar_t SnapshotProperty[] = L"MiaoZip.MenuSnapshot";
    struct SnapshotScope
    {
        HWND Owner;
        HANDLE Previous;
        explicit SnapshotScope(HWND owner) : Owner(owner), Previous(GetPropW(owner, SnapshotProperty))
        { SetPropW(Owner, SnapshotProperty, reinterpret_cast<HANDLE>(1)); }
        ~SnapshotScope()
        { if (Previous) SetPropW(Owner, SnapshotProperty, Previous); else RemovePropW(Owner, SnapshotProperty); }
        SnapshotScope(const SnapshotScope&) = delete;
        SnapshotScope& operator=(const SnapshotScope&) = delete;
    };
    struct Label
    {
        std::wstring Text, Shortcut, AccessKey;
    };
    inline Label ParseLabel(const std::wstring& source)
    {
        Label label;
        const auto tab = source.find(L'\t');
        const auto title = source.substr(0, tab);
        if (tab != std::wstring::npos) label.Shortcut = source.substr(tab + 1);
        for (size_t i = 0; i < title.size(); ++i)
        {
            if (i + 3 < title.size() && title[i] == L'(' && title[i + 1] == L'&' && title[i + 3] == L')')
            { label.AccessKey = title[i + 2]; i += 3; continue; }
            if (title[i] == L'&' && i + 1 < title.size())
            {
                if (title[i + 1] == L'&') { label.Text += L'&'; ++i; }
                else label.AccessKey = title[i + 1];
            }
            else label.Text += title[i];
        }
        return label;
    }
    struct Entry
    {
        Label Caption;
        UINT Command = 0;
        bool Separator = false, Enabled = true, Checked = false, Radio = false;
        HMENU Native = nullptr; // Opaque Shell extensions retain their native menu behaviour.
        std::vector<Entry> Children;
    };
    inline std::vector<Entry> Snapshot(HWND owner, HMENU menu, int position = -1, unsigned depth = 0)
    {
        struct Notification
        {
            HWND Owner; HMENU Menu; bool Active;
            ~Notification() { if (Active) SendMessageW(Owner, WM_UNINITMENUPOPUP, reinterpret_cast<WPARAM>(Menu), 0); }
        } notification{owner, menu, position >= 0};
        if (notification.Active) SendMessageW(owner, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(menu), MAKELPARAM(position, FALSE));
        std::vector<Entry> entries;
        for (int index = 0; index < GetMenuItemCount(menu); ++index)
        {
            MENUITEMINFOW info{sizeof(info)};
            info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_STRING | MIIM_BITMAP;
            if (!GetMenuItemInfoW(menu, index, TRUE, &info)) continue;
            std::wstring title(static_cast<size_t>(info.cch) + 1, L'\0');
            info.dwTypeData = title.data(); info.cch = static_cast<UINT>(title.size());
            if (!GetMenuItemInfoW(menu, index, TRUE, &info)) continue;
            title.resize(info.cch);
            Entry entry;
            entry.Caption = ParseLabel(title); entry.Command = info.wID;
            entry.Separator = (info.fType & MFT_SEPARATOR) != 0;
            entry.Enabled = (info.fState & (MFS_DISABLED | MFS_GRAYED)) == 0;
            entry.Checked = (info.fState & MFS_CHECKED) != 0;
            entry.Radio = (info.fType & MFT_RADIOCHECK) != 0;
            if (!entry.Separator)
            {
                const bool opaque = (info.fType & MFT_OWNERDRAW) || info.hbmpItem == HBMMENU_CALLBACK || title.empty();
                if (opaque || depth >= 8 || (depth > 0 && info.hSubMenu && info.wID == 0))
                {
                    entry.Native = info.hSubMenu ? info.hSubMenu : menu;
                    if (entry.Caption.Text.empty()) entry.Caption.Text = L"Windows 菜单…";
                }
                else if (info.hSubMenu) entry.Children = Snapshot(owner, info.hSubMenu, index, depth + 1);
            }
            entries.push_back(std::move(entry));
        }
        return entries;
    }
}
