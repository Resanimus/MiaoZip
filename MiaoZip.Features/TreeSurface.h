// Render the navigation tree with the same readable states as the file list.
#pragma once
#include "Theme.h"
#include "AppIcon.h"
#include <CommCtrl.h>

namespace MiaoZip::Theme
{
    inline LRESULT DrawTree(NMTVCUSTOMDRAW* draw)
    {
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (draw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT) return CDRF_DODEFAULT;
        const HWND tree = draw->nmcd.hdr.hwndFrom;
        const HTREEITEM handle = reinterpret_cast<HTREEITEM>(draw->nmcd.dwItemSpec);
        const auto colors = Colors();
        wchar_t text[1024]{};
        TVITEMW item{}; item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_STATE | TVIF_CHILDREN;
        item.hItem = handle; item.pszText = text; item.cchTextMax = ARRAYSIZE(text);
        item.stateMask = TVIS_SELECTED | TVIS_EXPANDED;
        if (!TreeView_GetItem(tree, &item)) return CDRF_DODEFAULT;
        RECT client{}; GetClientRect(tree, &client);
        RECT label{};
        if (!TreeView_GetItemRect(tree, handle, &label, TRUE) || label.bottom <= label.top)
            return CDRF_SKIPDEFAULT;
        RECT row{ 0, label.top, client.right, label.bottom };
        const HDC dc = draw->nmcd.hdc;
        const int saved = SaveDC(dc);
        // Never draw outside this row during a partial invalidation.
        IntersectClipRect(dc, row.left, row.top, row.right, row.bottom);
        Fill(dc, row, colors.Sidebar);
        const bool selected = (item.state & TVIS_SELECTED) != 0;
        if (selected)
        {
            RECT selection = row; InflateRect(&selection, -3, -2);
            Rounded(dc, selection, colors.Selected, Scale(tree, 8));
            RECT marker = selection; marker.right = marker.left + Scale(tree, 3);
            marker.top += Scale(tree, 6); marker.bottom -= Scale(tree, 6);
            Rounded(dc, marker, colors.Accent, Scale(tree, 3));
        }
        const int icon = Scale(tree, 16);
        const int contentInset = Scale(tree, 8);
        const int iconLeft = label.left - Scale(tree, 20) + contentInset;
        if (item.cChildren && TreeView_GetParent(tree, handle))
        {
            const int x = iconLeft - contentInset - Scale(tree, 13), y = (row.top + row.bottom) / 2;
            HPEN pen = CreatePen(PS_SOLID, Scale(tree, 1), colors.Muted);
            const auto old = SelectObject(dc, pen);
            if (item.state & TVIS_EXPANDED)
            {
                MoveToEx(dc, x, y - Scale(tree, 2), nullptr);
                LineTo(dc, x + Scale(tree, 3), y + Scale(tree, 1));
                LineTo(dc, x + Scale(tree, 6), y - Scale(tree, 2));
            }
            else
            {
                MoveToEx(dc, x + Scale(tree, 1), y - Scale(tree, 3), nullptr);
                LineTo(dc, x + Scale(tree, 4), y);
                LineTo(dc, x + Scale(tree, 1), y + Scale(tree, 3));
            }
            SelectObject(dc, old); DeleteObject(pen);
        }
        const auto images = TreeView_GetImageList(tree, TVSIL_NORMAL);
        const bool archiveRoot = !TreeView_GetParent(tree, handle) &&
            GetPropW(tree, L"MiaoZip.ArchiveTree");
        if (archiveRoot && MiaoZip::Icons::Application(tree))
            DrawIconEx(dc, iconLeft, (row.top + row.bottom - icon) / 2,
                MiaoZip::Icons::Application(tree), icon, icon, 0, nullptr, DI_NORMAL);
        else if (images) ImageList_Draw(images, item.iImage, dc, iconLeft,
            (row.top + row.bottom - icon) / 2, ILD_TRANSPARENT);
        SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(tree, WM_GETFONT, 0, 0)));
        SetTextColor(dc, colors.Text); SetBkMode(dc, TRANSPARENT);
        label.left += contentInset;
        label.right = client.right - Scale(tree, 8);
        DrawTextW(dc, text, -1, &label, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        RestoreDC(dc, saved);
        return CDRF_SKIPDEFAULT;
    }
}
