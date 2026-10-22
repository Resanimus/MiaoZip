// Geometry in device-independent pixels; independent of Win32 for layout tests.
#pragma once
#include <algorithm>
#include <vector>

namespace MiaoZip::Dialogs
{
    struct FormItem { int Id, X, Y, Width, Height; bool Footer = false; };
    struct ExtractForm
    {
        std::vector<FormItem> Items;
        int ContentHeight = 0;
        int ViewportHeight = 0;
        int ScrollMaximum = 0;
    };

    inline ExtractForm ArrangeExtraction(int width, int height)
    {
        ExtractForm form;
        const int pad = 24, gap = 24;
        const bool twoColumns = width >= 680;
        const int column = twoColumns ? (width - 2 * pad - gap) / 2 : width - 2 * pad;
        const int right = twoColumns ? pad + column + gap : pad;
        const int rightTop = twoColumns ? 164 : 484;
        const auto put = [&](int id, int x, int y, int w, int h, bool footer = false)
        { form.Items.push_back({id, x, y, w, h, footer}); };
        put(3401, pad, 22, width - 2 * pad, 22);
        put(100, pad, 52, width - 2 * pad - 92, 36);
        put(101, width - pad - 80, 52, 80, 36);
        put(131, pad, 104, 184, 28);
        put(130, pad + 196, 100, width - 2 * pad - 196, 36);
        put(3438, pad, 164, column, 296);
        put(3410, pad + 20, 196, column - 40, 22);
        put(102, pad + 20, 224, column - 40, 36);
        put(3420, pad + 20, 276, column - 40, 22);
        put(103, pad + 20, 304, column - 40, 36);
        put(3430, pad + 20, 360, column - 40, 28);
        put(3435, pad + 20, 400, column - 40, 28);
        put(3807, right, rightTop, column, 132);
        put(120, right + 20, rightTop + 36, column - 40, 36);
        put(3803, right + 20, rightTop + 88, column - 40, 28);
        put(3439, right, rightTop + 152, column, 196);
        put(104, right + 20, rightTop + 184, column - 40, 28);
        put(3431, right + 20, rightTop + 220, column - 40, 28);
        put(3433, right + 20, rightTop + 256, column - 40, 28);
        put(3436, right + 20, rightTop + 292, column - 40, 28);
        form.ContentHeight = rightTop + 372;
        form.ViewportHeight = (std::max)(1, height - 76);
        form.ScrollMaximum = (std::max)(0, form.ContentHeight - form.ViewportHeight);
        put(2, width - pad - 212, height - 56, 100, 36, true);
        put(1, width - pad - 100, height - 56, 100, 36, true);
        return form;
    }
}
