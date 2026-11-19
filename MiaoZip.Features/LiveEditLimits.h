// MiaoZip live document limits shared by the file manager and toolbar.
#pragma once

namespace MiaoZip
{
    inline constexpr unsigned DocumentLimitMiB = 128;
    inline constexpr unsigned ArchiveLimitMiB = 256;
    inline constexpr unsigned long long DocumentLimit = DocumentLimitMiB * 1024ull * 1024;
    inline constexpr unsigned long long ArchiveLimit = ArchiveLimitMiB * 1024ull * 1024;
}
