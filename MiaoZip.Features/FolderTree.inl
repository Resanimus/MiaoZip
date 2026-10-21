// MiaoZip archive navigation. Native folder tree, no additional archive engine.
void CPanel::ClearMiaoZipTree()
{
    _miaoTreeUpdating = true;
    if (_miaoTree) TreeView_DeleteAllItems(_miaoTree);
    _miaoTreeNodes.clear(); _miaoTreeArchive.Empty();
    _miaoTreeUpdating = false;
}

CMyComPtr<IFolderFolder> CPanel::MiaoZipTreeFolder(const UString& relative)
{
    CMyComPtr<IFolderFolder> folder = _folder;
    if (!folder || _parentFolders.IsEmpty()) return {};
    // BindToParentFolder stops at the root of the archive agent.
    for (unsigned depth = 0; depth < 256; ++depth)
    {
        CMyComPtr<IFolderFolder> parent;
        if (folder->BindToParentFolder(&parent) != S_OK || !parent) break;
        folder = parent;
    }
    for (const auto& component : std::filesystem::path(relative.Ptr()))
    {
        if (component.empty()) continue;
        CMyComPtr<IFolderFolder> child;
        if (folder->BindToFolder(component.c_str(), &child) != S_OK || !child) return {};
        folder = child;
    }
    return folder;
}

void CPanel::PopulateMiaoZipTree(MiaoTreeNode* node)
{
    if (!node || node->Loaded || !node->Absolute.IsEmpty()) return;
    auto folder = MiaoZipTreeFolder(node->Relative);
    if (!folder) return;
    node->Loaded = true;
    UInt32 count = 0; folder->GetNumberOfItems(&count);
    struct Child { UString Name; bool HasFolders; };
    std::vector<Child> children;
    for (UInt32 i = 0; i < count; ++i)
    {
        NWindows::NCOM::CPropVariant directory, name;
        folder->GetProperty(i, kpidIsDir, &directory);
        if (directory.vt != VT_BOOL || directory.boolVal == VARIANT_FALSE) continue;
        folder->GetProperty(i, kpidName, &name);
        if (name.vt != VT_BSTR) continue;
        // Look ahead one level: files alone must not advertise an expander.
        bool hasFolders = false;
        CMyComPtr<IFolderFolder> childFolder;
        if (folder->BindToFolder(name.bstrVal, &childFolder) == S_OK && childFolder)
        {
            UInt32 childCount = 0;
            childFolder->GetNumberOfItems(&childCount);
            for (UInt32 childIndex = 0; childIndex < childCount; ++childIndex)
            {
                NWindows::NCOM::CPropVariant isFolder;
                childFolder->GetProperty(childIndex, kpidIsDir, &isFolder);
                if (isFolder.vt == VT_BOOL && isFolder.boolVal != VARIANT_FALSE)
                { hasFolders = true; break; }
            }
        }
        children.push_back({ UString(name.bstrVal), hasFolders });
    }
    std::sort(children.begin(), children.end(), [](const Child& a, const Child& b)
        { return CompareFileNames_ForFolderList(a.Name, b.Name) < 0; });
    for (const auto& child : children)
    {
        auto value = std::make_unique<MiaoTreeNode>();
        value->Relative = node->Relative;
        value->Relative += child.Name; value->Relative.Add_PathSepar();
        TVINSERTSTRUCTW insert{}; insert.hParent = node->Item; insert.hInsertAfter = TVI_LAST;
        insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
        insert.item.pszText = const_cast<wchar_t*>(child.Name.Ptr());
        insert.item.lParam = reinterpret_cast<LPARAM>(value.get());
        insert.item.cChildren = child.HasFolders ? 1 : 0;
        value->Loaded = !child.HasFolders;
        SHFILEINFOW icon{};
        SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &icon, sizeof(icon), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
        insert.item.iImage = icon.iIcon; insert.item.iSelectedImage = icon.iIcon;
        value->Item = TreeView_InsertItem(_miaoTree, &insert);
        _miaoTreeNodes.push_back(std::move(value));
    }
    if (children.empty())
    {
        TVITEMW item{}; item.hItem = node->Item; item.mask = TVIF_CHILDREN; item.cChildren = 0;
        TreeView_SetItem(_miaoTree, &item);
    }
}

void CPanel::SyncMiaoZipTree()
{
    if (!_miaoTree) return;
    if (_parentFolders.IsEmpty()) RemovePropW(_miaoTree, L"MiaoZip.ArchiveTree");
    else SetPropW(_miaoTree, L"MiaoZip.ArchiveTree", reinterpret_cast<HANDLE>(1));
    SetWindowTextW(_miaoTree, _parentFolders.IsEmpty() ? L"位置" : L"压缩包目录");
    if (_parentFolders.IsEmpty())
    {
        if (_miaoTreeArchive != L":locations:")
        {
            ClearMiaoZipTree(); _miaoTreeUpdating = true;
            _miaoTreeArchive = L":locations:";
            const auto addLocation = [this](const wchar_t* label, const wchar_t* path)
            {
                auto value = std::make_unique<MiaoTreeNode>();
                value->Absolute = path; value->Loaded = true;
                TVINSERTSTRUCTW insert{}; insert.hParent = TVI_ROOT; insert.hInsertAfter = TVI_LAST;
                insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
                insert.item.pszText = const_cast<wchar_t*>(label);
                insert.item.lParam = reinterpret_cast<LPARAM>(value.get());
                SHFILEINFOW icon{};
                SHGetFileInfoW(path, FILE_ATTRIBUTE_DIRECTORY, &icon, sizeof(icon),
                    SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
                insert.item.iImage = insert.item.iSelectedImage = icon.iIcon;
                value->Item = TreeView_InsertItem(_miaoTree, &insert);
                _miaoTreeNodes.push_back(std::move(value));
            };
            const struct { const KNOWNFOLDERID* Id; const wchar_t* Label; } locations[] =
            { { &FOLDERID_Desktop, L"桌面" }, { &FOLDERID_Downloads, L"下载" }, { &FOLDERID_Documents, L"文档" } };
            for (const auto& location : locations)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(*location.Id, KF_FLAG_DONT_VERIFY, nullptr, &path)))
                { addLocation(location.Label, path); CoTaskMemFree(path); }
            }
            wchar_t drives[512]{};
            if (GetLogicalDriveStringsW(ARRAYSIZE(drives), drives))
                for (const wchar_t* drive = drives; *drive; drive += wcslen(drive) + 1)
                    if (GetDriveTypeW(drive) == DRIVE_FIXED || GetDriveTypeW(drive) == DRIVE_REMOVABLE)
                        addLocation(drive, drive);
            _miaoTreeUpdating = false;
        }
        ShowWindow(_miaoTree, SW_SHOW);
    }
    else
    {
        const UString archive = _parentFolders.Back().VirtualPath;
        _miaoTreeUpdating = true;
        WIN32_FILE_ATTRIBUTE_DATA info{};
        const auto diskPath = _parentFolders.Back().FilePath;
        const bool found = GetFileAttributesExW(diskPath.Ptr(), GetFileExInfoStandard, &info) != FALSE;
        const UInt64 stamp = (static_cast<UInt64>(info.ftLastWriteTime.dwHighDateTime) << 32) | info.ftLastWriteTime.dwLowDateTime;
        const UInt64 size = (static_cast<UInt64>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        if (_miaoTreeArchive != archive || (found && (stamp != _miaoTreeStamp || size != _miaoTreeSize)))
        {
            ClearMiaoZipTree(); _miaoTreeUpdating = true; _miaoTreeArchive = archive;
            _miaoTreeStamp = stamp; _miaoTreeSize = size;
            auto value = std::make_unique<MiaoTreeNode>();
            const auto filename = std::filesystem::path(archive.Ptr()).filename().wstring();
            TVINSERTSTRUCTW insert{}; insert.hParent = TVI_ROOT; insert.hInsertAfter = TVI_LAST;
            insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
            SHFILEINFOW icon{};
            SHGetFileInfoW(archive.Ptr(), FILE_ATTRIBUTE_NORMAL, &icon, sizeof(icon), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
            insert.item.iImage = icon.iIcon; insert.item.iSelectedImage = icon.iIcon;
            insert.item.pszText = const_cast<wchar_t*>(filename.c_str());
            insert.item.lParam = reinterpret_cast<LPARAM>(value.get()); insert.item.cChildren = 1;
            value->Item = TreeView_InsertItem(_miaoTree, &insert);
            _miaoTreeNodes.push_back(std::move(value));
            PopulateMiaoZipTree(_miaoTreeNodes.front().get());
            TreeView_Expand(_miaoTree, _miaoTreeNodes.front()->Item, TVE_EXPAND);
        }
        UString relative;
        if (_currentFolderPrefix.Len() > archive.Len()) relative = _currentFolderPrefix.Ptr(archive.Len() + 1);
        UString accumulated;
        for (const auto& component : std::filesystem::path(relative.Ptr()))
        {
            if (component.empty()) continue;
            accumulated += component.c_str(); accumulated.Add_PathSepar();
            for (size_t i = 0; i < _miaoTreeNodes.size(); ++i)
                if (_miaoTreeNodes[i]->Relative == accumulated)
                {
                    auto* current = _miaoTreeNodes[i].get();
                    PopulateMiaoZipTree(current); TreeView_Expand(_miaoTree, current->Item, TVE_EXPAND); break;
                }
        }
        for (const auto& node : _miaoTreeNodes)
            if (node->Relative == relative) { TreeView_SelectItem(_miaoTree, node->Item); break; }
        _miaoTreeUpdating = false;
        ShowWindow(_miaoTree, SW_SHOW);
    }
    RECT rectangle{}; ::GetClientRect(*this, &rectangle);
    ChangeWindowSize(rectangle.right, rectangle.bottom);
}

bool CPanel::NotifyMiaoZipTree(LPNMHDR header, LRESULT& result)
{
    if (!_miaoTree || header->hwndFrom != _miaoTree) return false;
    result = 0;
    if (header->code == NM_CUSTOMDRAW)
    { result = MiaoZip::Theme::DrawTree(reinterpret_cast<NMTVCUSTOMDRAW*>(header)); return true; }
    if (_miaoTreeUpdating) return true;
    const auto* change = reinterpret_cast<NMTREEVIEWW*>(header);
    if (header->code == TVN_ITEMEXPANDINGW && (change->action & TVE_EXPAND))
        PopulateMiaoZipTree(reinterpret_cast<MiaoTreeNode*>(change->itemNew.lParam));
    else if (header->code == TVN_ITEMEXPANDEDW)
    {
        // Native tree scrolling can reuse only part of an owner-painted row.
        // Repaint every visible row after expansion finishes, including siblings.
        ::InvalidateRect(_miaoTree, nullptr, TRUE);
    }
    else if (header->code == TVN_SELCHANGEDW)
    {
        const auto* node = reinterpret_cast<MiaoTreeNode*>(change->itemNew.lParam);
        if (node && _processNotify)
        {
            UString path = node->Absolute;
            if (path.IsEmpty()) { path = _miaoTreeArchive; path.Add_PathSepar(); path += node->Relative; }
            BindToPathAndRefresh(path);
            ::InvalidateRect(_miaoTree, nullptr, TRUE);
        }
    }
    return true;
}
