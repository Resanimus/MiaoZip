// Rendering for the standard Windows controls. Input and accessibility stay native.
#pragma once
#include "Theme.h"
#include "AppIcon.h"
#include <CommCtrl.h>
#include <windowsx.h>
#include <vector>
#pragma comment(lib, "comctl32.lib")

namespace MiaoZip::Theme
{
    inline void PaintHeader(HWND window, HDC dc)
    {
        RECT client{}; GetClientRect(window, &client);
        const auto colors = Colors();
        Fill(dc, client, colors.Content);
        const int saved = SaveDC(dc);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, colors.Muted);
        SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)));
        POINT pointer{}; GetCursorPos(&pointer); ScreenToClient(window, &pointer);
        const int count = Header_GetItemCount(window);
        for (int i = 0; i < count; ++i)
        {
            RECT item{}; Header_GetItemRect(window, i, &item);
            if (item.right <= client.left || item.left >= client.right) continue;
            if (PtInRect(&item, pointer))
            {
                RECT hover = item; InflateRect(&hover, -2, -3);
                Rounded(dc, hover, colors.Hover, Scale(window, 8));
            }
            wchar_t text[512]{};
            HDITEMW value{}; value.mask = HDI_TEXT | HDI_FORMAT;
            value.pszText = text; value.cchTextMax = ARRAYSIZE(text);
            Header_GetItem(window, i, &value);
            RECT label = item;
            label.left += Scale(window, i == 0 ? 36 : 12); label.right -= Scale(window, 12);
            UINT format = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
            if (value.fmt & HDF_RIGHT) format |= DT_RIGHT;
            else if (value.fmt & HDF_CENTER) format |= DT_CENTER;
            if (value.fmt & (HDF_SORTUP | HDF_SORTDOWN))
            {
                const int x = item.right - Scale(window, 10), y = (item.top + item.bottom) / 2;
                const int direction = (value.fmt & HDF_SORTUP) ? -1 : 1;
                HPEN pen = CreatePen(PS_SOLID, Scale(window, 1), colors.Muted);
                const auto old = SelectObject(dc, pen);
                MoveToEx(dc, x - Scale(window, 3), y - direction, nullptr);
                LineTo(dc, x, y + direction * 2);
                LineTo(dc, x + Scale(window, 3), y - direction);
                SelectObject(dc, old); DeleteObject(pen);
                label.right -= Scale(window, 10);
            }
            DrawTextW(dc, text, -1, &label, format);
        }
        RECT line = client; line.top = line.bottom - 1;
        Fill(dc, line, colors.Line);
        RestoreDC(dc, saved);
    }

    inline LRESULT CALLBACK HeaderProcedure(HWND window, UINT message,
        WPARAM wParam, LPARAM lParam, UINT_PTR subclass, DWORD_PTR)
    {
        switch (message)
        {
        case WM_PAINT:
        {
            PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint);
            PaintHeader(window, dc); EndPaint(window, &paint); return 0;
        }
        case WM_PRINTCLIENT:
            PaintHeader(window, reinterpret_cast<HDC>(wParam)); return 0;
        case WM_ERASEBKGND:
            return 1;
        case HDM_LAYOUT:
        {
            const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
            const auto layout = reinterpret_cast<HDLAYOUT*>(lParam);
            if (layout && layout->pwpos)
            {
                const int height = Scale(window, 36);
                const int difference = height - layout->pwpos->cy;
                layout->pwpos->cy = height;
                if (layout->prc) layout->prc->top += difference;
            }
            return result;
        }
        case WM_MOUSEMOVE:
        {
            TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, window, 0 };
            TrackMouseEvent(&tracking); InvalidateRect(window, nullptr, FALSE); break;
        }
        case WM_MOUSELEAVE:
            InvalidateRect(window, nullptr, FALSE); break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(window, HeaderProcedure, subclass); break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    inline void TrackRow(HWND list, LPARAM point)
    {
        LVHITTESTINFO hit{}; hit.pt = { GET_X_LPARAM(point), GET_Y_LPARAM(point) };
        const int item = ListView_HitTest(list, &hit);
        const int old = static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(list, L"MiaoZip.HotRow"))) - 1;
        if (old != item)
        {
            SetPropW(list, L"MiaoZip.HotRow", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(item + 1)));
            if (old >= 0) ListView_RedrawItems(list, old, old);
            if (item >= 0) ListView_RedrawItems(list, item, item);
        }
        TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, list, 0 };
        TrackMouseEvent(&tracking);
    }

    inline void ClearHotRow(HWND list)
    {
        const int old = static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(list, L"MiaoZip.HotRow"))) - 1;
        RemovePropW(list, L"MiaoZip.HotRow");
        if (old >= 0) ListView_RedrawItems(list, old, old);
    }

    inline void DrawRow(const DRAWITEMSTRUCT& item, bool selected, bool deleted, bool grid, bool archiveFile)
    {
        const HWND list = item.hwndItem;
        const HDC dc = item.hDC;
        const auto colors = Colors();
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom);
        Fill(dc, item.rcItem, colors.Content);
        const bool hot = reinterpret_cast<INT_PTR>(GetPropW(list, L"MiaoZip.HotRow")) == static_cast<INT_PTR>(item.itemID + 1);
        RECT highlight = item.rcItem;
        highlight.left += Scale(list, 4); highlight.right -= Scale(list, 4);
        highlight.top += Scale(list, 2); highlight.bottom -= Scale(list, 2);
        if (selected || hot)
            Rounded(dc, highlight, selected ? colors.Selected : colors.Hover, Scale(list, 8));
        if (selected && !Current().HighContrast)
        {
            RECT marker = highlight;
            marker.left += Scale(list, 1); marker.right = marker.left + Scale(list, 3);
            marker.top += Scale(list, 6); marker.bottom -= Scale(list, 6);
            Rounded(dc, marker, colors.Accent, Scale(list, 3));
        }
        SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(list, WM_GETFONT, 0, 0)));
        SetBkMode(dc, TRANSPARENT);
        const HWND header = ListView_GetHeader(list);
        const int count = Header_GetItemCount(header);
        for (int order = 0; order < count; ++order)
        {
            const int column = Header_OrderToIndex(header, order);
            RECT cell{}; Header_GetItemRect(header, column, &cell);
            MapWindowPoints(header, list, reinterpret_cast<POINT*>(&cell), 2);
            cell.top = item.rcItem.top; cell.bottom = item.rcItem.bottom;
            if (cell.right <= item.rcItem.left || cell.left >= item.rcItem.right) continue;
            const int cellSave = SaveDC(dc);
            IntersectClipRect(dc, cell.left, cell.top, cell.right, cell.bottom);
            cell.left += Scale(list, 12); cell.right -= Scale(list, 12);
            if (column == 0)
            {
                LVITEMW value{}; value.iItem = static_cast<int>(item.itemID);
                value.mask = LVIF_IMAGE | LVIF_STATE; value.stateMask = LVIS_OVERLAYMASK | LVIS_CUT;
                ListView_GetItem(list, &value);
                const int size = Scale(list, 16);
                if (archiveFile && MiaoZip::Icons::Application(list))
                    DrawIconEx(dc, cell.left, cell.top + (cell.bottom - cell.top - size) / 2,
                        MiaoZip::Icons::Application(list), size, size, 0, nullptr, DI_NORMAL);
                else ImageList_DrawEx(ListView_GetImageList(list, LVSIL_SMALL), value.iImage, dc,
                    cell.left, cell.top + (cell.bottom - cell.top - size) / 2,
                    size, size, CLR_NONE, CLR_NONE, ILD_TRANSPARENT |
                    (value.state & LVIS_OVERLAYMASK) | ((value.state & LVIS_CUT) ? ILD_BLEND50 : 0));
                cell.left += size + Scale(list, 8);
            }
            wchar_t text[2048]{};
            ListView_GetItemText(list, static_cast<int>(item.itemID), column, text, ARRAYSIZE(text));
            LVCOLUMNW format{}; format.mask = LVCF_FMT;
            ListView_GetColumn(list, column, &format);
            UINT flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
            if (format.fmt & LVCFMT_RIGHT) flags |= DT_RIGHT;
            else if (format.fmt & LVCFMT_CENTER) flags |= DT_CENTER;
            SetTextColor(dc, deleted ? RGB(220,83,94) :
                (selected && Current().HighContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) :
                (column == 0 || selected ? colors.Text : colors.Muted)));
            DrawTextW(dc, text, -1, &cell, flags);
            RestoreDC(dc, cellSave);
        }
        if (grid && !selected)
        {
            RECT line = item.rcItem; line.top = line.bottom - 1;
            line.left += Scale(list, 12); line.right -= Scale(list, 12);
            Fill(dc, line, colors.Line);
        }
        if ((item.itemState & ODS_FOCUS) && GetFocus() == list &&
            !(SendMessageW(list, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS))
        {
            RECT focus = highlight;
            InflateRect(&focus, -Scale(list, 1), -Scale(list, 1));
            auto pen = CreatePen(PS_SOLID, (std::max)(1, Scale(list, 1)), colors.Accent);
            auto oldPen = SelectObject(dc, pen);
            auto oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(dc, focus.left, focus.top, focus.right, focus.bottom,
                Scale(list, 8), Scale(list, 8));
            SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(pen);
        }
        RestoreDC(dc, saved);
    }
}
