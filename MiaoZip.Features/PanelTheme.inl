// **************** NanaZip Modification Start ****************
// MiaoZip visual layer. The existing panel remains responsible for file operations.
void CPanel::ApplyMiaoZipTheme()
{
    MiaoZip::Theme::Reload();
    if (!(HWND)*this || !(HWND)_listView) return;
    const auto colors = MiaoZip::Theme::Colors();
    const UINT dpi = GetDpiForWindow(*this);
    if (!_miaoFont || _miaoFontDpi != dpi)
    {
        const HFONT oldFont = _miaoFont, oldHeader = _miaoHeaderFont;
        _miaoFont = CreateFontW(-MulDiv(14, dpi, 96), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        _miaoHeaderFont = CreateFontW(-MulDiv(12, dpi, 96), 0, 0, 0, FW_MEDIUM,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        _miaoFontDpi = dpi;
        SendMessageW(_listView, WM_SETFONT, reinterpret_cast<WPARAM>(_miaoFont), TRUE);
        if (_miaoTree) SendMessageW(_miaoTree, WM_SETFONT, reinterpret_cast<WPARAM>(_miaoFont), TRUE);
        if (oldFont) DeleteObject(oldFont);
        if (oldHeader) DeleteObject(oldHeader);
    }
    ListView_SetBkColor(_listView, colors.Content);
    ListView_SetTextBkColor(_listView, colors.Content);
    ListView_SetTextColor(_listView, colors.Text);
    const HWND header = ListView_GetHeader(_listView);
    if (header)
    {
        SendMessageW(header, WM_SETFONT, reinterpret_cast<WPARAM>(_miaoHeaderFont), TRUE);
        SetWindowSubclass(header, MiaoZip::Theme::HeaderProcedure, 0x4D5A, 0);
        ::InvalidateRect(header, nullptr, TRUE);
    }
    if (_miaoTree)
    {
        TreeView_SetBkColor(_miaoTree, colors.Sidebar);
        TreeView_SetTextColor(_miaoTree, colors.Text);
        TreeView_SetLineColor(_miaoTree, colors.Line);
        TreeView_SetItemHeight(_miaoTree, MulDiv(34, dpi, 96));
        TreeView_SetIndent(_miaoTree, MulDiv(18, dpi, 96));
        SendMessageW(_miaoTree, TVM_SETEXTENDEDSTYLE, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
    }
    RECT rectangle{}; ::GetClientRect(*this, &rectangle);
    ChangeWindowSize(rectangle.right, rectangle.bottom);
    RedrawWindow(*this, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
}

bool CPanel::DrawMiaoZipRow(const DRAWITEMSTRUCT* item)
{
    if (!item || item->hwndItem != (HWND)_listView || item->itemID == static_cast<UINT>(-1))
        return false;
    bool selected = (item->itemState & ODS_SELECTED) != 0;
    const int index = GetRealItemIndex(static_cast<int>(item->itemID));
    if (_mySelectMode && index != kParentIndex && index >= 0 &&
        static_cast<unsigned>(index) < _selectedStatusVector.Size())
        selected = selected || _selectedStatusVector[index];
    const bool deleted = _markDeletedItems && _thereAreDeletedItems &&
        index != kParentIndex && index >= 0 && IsItem_Deleted(index);
    DRAWITEMSTRUCT row = *item;
    RECT client{}; ::GetClientRect(_listView, &client);
    row.rcItem.left = 0; row.rcItem.right = client.right;
    const bool archiveFile = index >= 0 && !IsItem_Folder(index) &&
        MiaoZip::Icons::IsArchive(GetItemName(index).Ptr());
    MiaoZip::Theme::DrawRow(row, selected, deleted, (_exStyle & LVS_EX_GRIDLINES) != 0, archiveFile);
    return true;
}

void CPanel::PaintMiaoZipPanel()
{
    PAINTSTRUCT paint{}; HDC dc = BeginPaint(*this, &paint);
    RECT client{}; ::GetClientRect(*this, &client);
    const auto colors = MiaoZip::Theme::Colors();
    const bool glass = GetPropW(GetAncestor(*this, GA_ROOT), L"MiaoZip.GlassActive") != nullptr;
    MiaoZip::Theme::Fill(dc, client, colors.Chrome);
    if (glass)
    {
        RECT navigation = client; navigation.bottom = MiaoZip::Theme::Scale(*this, AddressBarHeight);
        MiaoZip::Theme::Fill(dc, navigation, RGB(0,0,0));
        RECT footer = client; footer.top = client.bottom - MiaoZip::Theme::Scale(*this, StatusBarHeight);
        MiaoZip::Theme::Fill(dc, footer, RGB(0,0,0));
    }
    const int gap = MiaoZip::Theme::Scale(*this, 12);
    const int top = MiaoZip::Theme::Scale(*this, AddressBarHeight) + gap;
    const int bottom = client.bottom - MiaoZip::Theme::Scale(*this, StatusBarHeight) - gap;
    const bool treeVisible = _miaoTree && IsWindowVisible(_miaoTree);
    if (treeVisible)
    {
        RECT tree{}; ::GetWindowRect(_miaoTree, &tree);
        MapWindowPoints(HWND_DESKTOP, *this, reinterpret_cast<POINT*>(&tree), 2);
        RECT card{ gap, top, tree.right + gap, bottom };
        MiaoZip::Theme::Rounded(dc, card, colors.Sidebar, MiaoZip::Theme::Scale(*this, 18));
        RECT heading{ gap * 2, top, card.right - gap,
            top + MiaoZip::Theme::Scale(*this, 37) };
        const auto oldFont = SelectObject(dc, _miaoHeaderFont);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, colors.Muted);
        DrawTextW(dc, _parentFolders.IsEmpty() ? L"位置" : L"压缩包目录", -1,
            &heading, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, oldFont);
    }
    if ((HWND)_listView)
    {
        RECT list{}; ::GetWindowRect(_listView, &list);
        MapWindowPoints(HWND_DESKTOP, *this, reinterpret_cast<POINT*>(&list), 2);
        InflateRect(&list, 1, 1);
        MiaoZip::Theme::Rounded(dc, list, colors.Line, MiaoZip::Theme::Scale(*this, 18));
    }
    EndPaint(*this, &paint);
}
// **************** NanaZip Modification End ****************
