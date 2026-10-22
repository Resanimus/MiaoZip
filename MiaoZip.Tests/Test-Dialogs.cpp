#include "../MiaoZip.Features/NativeDialogSurface.h"
#include "../MiaoZip.Features/NativeMenuSurface.h"
#include "../MiaoZip.Features/MenuModel.h"
#include "../MiaoZip.Features/ExtractDialogLayout.h"
#include "../MiaoZip.Features/ExtractionPreferences.h"
#include "../MiaoZip.Features/ExtractionCompletion.h"
#include <iostream>
#include <set>
#include <stdexcept>

static void Require(bool value, const char* message)
{ if (!value) throw std::runtime_error(message); }

int wmain()
{
    try
    {
        using namespace MiaoZip;
        int layouts = 0;
        const std::set<int> required{3401,100,101,131,130,3438,3410,102,3420,103,
            3430,3435,3807,120,3803,3439,104,3431,3433,3436,1,2};
        for (int dpi : {96,120,144,168,192,240,288})
        for (int width : {500,640,680,780,960})
        for (int height : {400,480,612,900})
        {
            auto form = Dialogs::ArrangeExtraction(width,height);
            std::set<int> found;
            for (const auto& item : form.Items)
            {
                Require(found.insert(item.Id).second,"Duplicate original control ID");
                Require(item.X >= 0 && item.Y >= 0 && item.Width > 0 && item.Height > 0,"Invalid geometry");
                Require(item.X + item.Width <= width,"Control exceeds right edge");
                Require(MulDiv(item.X+item.Width,dpi,96) <= MulDiv(width,dpi,96),"DPI rounding overflows");
                if (item.Footer) Require(item.Y + item.Height <= height,"Action is clipped");
                else Require(item.Y + item.Height <= form.ContentHeight,"Option is unreachable by scroll");
            }
            Require(found==required,"Existing extraction option missing");
            Require(form.ScrollMaximum + form.ViewportHeight >= form.ContentHeight,"Scroll range loses controls");
            const auto& cancel=form.Items[form.Items.size()-2];
            const auto& confirm=form.Items.back();
            Require(cancel.X+cancel.Width+12==confirm.X,"Footer actions overlap or lack spacing");
            ++layouts;
        }
        // Invisible native components validate the Windows combo behaviour
        // that pure geometry tests cannot catch. They never target user windows.
        HWND componentHost = CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,800,600,nullptr,nullptr,nullptr,nullptr);
        Require(componentHost != nullptr,"Cannot create component test host");
        for (DWORD type : {CBS_DROPDOWN,CBS_DROPDOWNLIST})
        for (int height : {28,36,54,72})
        {
            HWND combo = CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD | type,0,0,300,240,componentHost,nullptr,nullptr,nullptr);
            Require(combo != nullptr,"Cannot create native combo component");
            SetWindowPos(combo,nullptr,0,0,300,240,SWP_NOZORDER);
            Dialogs::SetComboHeight(combo,height);
            RECT rect{}; GetWindowRect(combo,&rect);
            Require(rect.bottom-rect.top==height,"Native combo resets its requested field height");
            if (type == CBS_DROPDOWN)
            {
                COMBOBOXINFO info{sizeof(info)}; GetComboBoxInfo(combo,&info);
                RECT edit{}; GetWindowRect(info.hwndItem,&edit);
                MapWindowPoints(nullptr,combo,reinterpret_cast<POINT*>(&edit),2);
                Require(std::abs(edit.top+edit.bottom-height)<=1,"Editable combo text is not vertically centred");
            }
            DestroyWindow(combo);
        }
        // Exercise real button state transitions and the full offscreen paint,
        // including the default-style change that previously drew native edges.
        const auto appearance = Theme::Current();
        int buttonPaints = 0;
        for (bool darkMode : {false,true})
        {
            Theme::Current().Dark = darkMode;
            Theme::Current().HighContrast = false;
            HWND button = CreateWindowExW(0,L"BUTTON",L"",WS_CHILD | BS_DEFPUSHBUTTON,
                0,0,120,36,componentHost,reinterpret_cast<HMENU>(IDOK),nullptr,nullptr);
            Require(button != nullptr,"Cannot create native button component");
            Dialogs::AttachControl(button);
            HDC dc = CreateCompatibleDC(nullptr);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = 120; info.bmiHeader.biHeight = -36;
            info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
            void* memory{};
            HBITMAP bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,&memory,nullptr,0);
            Require(dc && bitmap && memory,"Cannot create button paint target");
            auto previous = SelectObject(dc,bitmap);
            for (DWORD style : {BS_PUSHBUTTON,BS_DEFPUSHBUTTON})
            for (bool pressed : {false,true})
            {
                SendMessageW(button,BM_SETSTYLE,style,TRUE);
                SendMessageW(button,BM_SETSTATE,pressed,0);
                auto pixels = static_cast<DWORD*>(memory);
                std::fill(pixels,pixels + 120 * 36,0x00ff00ff);
                SendMessageW(button,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
                GdiFlush();
                const auto background = Dialogs::ControlBackground(button);
                const auto fill = pressed ? Design::Pressed(darkMode) : Design::AccentFill(darkMode);
                int blended = 0;
                for (int y = 0; y < 36; ++y)
                for (int x = 0; x < 120; ++x)
                {
                    const auto pixel = pixels[y * 120 + x];
                    const auto color = RGB((pixel >> 16) & 255,(pixel >> 8) & 255,pixel & 255);
                    for (int shift : {0,8,16})
                    {
                        const auto value = (color >> shift) & 255;
                        Require(value >= ((std::min)((background >> shift) & 255,(fill >> shift) & 255)) &&
                            value <= ((std::max)((background >> shift) & 255,(fill >> shift) & 255)),
                            "Native/dithered pixels remain on a themed button");
                    }
                    if (color != background && color != fill) ++blended;
                    if (x > 15 && x < 105 && y > 5 && y < 30)
                        Require(color == fill,"Solid button fill is not uniform");
                    if (x == 0 || y == 0 || x == 119 || y == 35)
                        Require(color == background,"Button spills outside its rounded frame");
                }
                Require(blended > 0,"Rounded edge is not antialiased");
                ++buttonPaints;
            }
            SelectObject(dc,previous); DeleteObject(bitmap); DeleteDC(dc);
            DestroyWindow(button);
        }
        Theme::Current() = appearance;
        DestroyWindow(componentHost);
        HMENU menu = CreatePopupMenu(), submenu = CreatePopupMenu();
        AppendMenuW(submenu,MF_STRING | MF_CHECKED,4101,L"名称(&N)\tCtrl+N");
        AppendMenuW(submenu,MF_STRING | MF_GRAYED,4102,L"不可用(&D)");
        AppendMenuW(submenu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(submenu),L"文件(&F)");
        const auto menuModel = Menus::Snapshot(nullptr,menu);
        Require(menuModel.size()==1 && menuModel[0].Caption.Text==L"文件" && menuModel[0].Caption.AccessKey==L"F","Menu caption lost its mnemonic");
        const auto& commands=menuModel[0].Children;
        Require(commands.size()==3 && commands[0].Command==4101 && commands[0].Checked,"Menu command/check state changed");
        Require(commands[0].Caption.Shortcut==L"Ctrl+N" && !commands[1].Enabled && commands[2].Separator,"Menu keyboard/disabled/separator state changed");
        Require(Menus::ParseLabel(L"A&&B").Text==L"A&B","Escaped menu ampersand changed");
        DestroyMenu(menu);
        Require(Extraction::ShouldExitAfterExtraction(true,false,true,true,false),"Successful extraction must exit when enabled");
        Require(!Extraction::ShouldExitAfterExtraction(false,false,true,true,false),"Default extraction must remain open");
        Require(!Extraction::ShouldExitAfterExtraction(true,true,true,true,false),"Archive testing must not exit");
        Require(!Extraction::ShouldExitAfterExtraction(true,false,false,true,false),"Cancelled/failed extraction must not exit");
        Require(!Extraction::ShouldExitAfterExtraction(true,false,true,false,false),"Callback errors must prevent exit");
        Require(!Extraction::ShouldExitAfterExtraction(true,false,true,true,true),"Warnings must prevent exit");
        Require(!Extraction::RequestOwnerClose({}),"An unrelated or missing window must not close");

        // Isolate preference tests from the user's actual registry and running app.
        const auto keyName = L"Software\\MiaoZip-Dialog-Tests-" + std::to_wstring(GetCurrentProcessId());
        HKEY isolated{};
        Require(RegCreateKeyExW(HKEY_CURRENT_USER,keyName.c_str(),0,nullptr,0,
            KEY_ALL_ACCESS,nullptr,&isolated,nullptr)==ERROR_SUCCESS,"Cannot create isolated preferences");
        Require(RegOverridePredefKey(HKEY_CURRENT_USER,isolated)==ERROR_SUCCESS,"Cannot isolate HKCU");
        Require(!Extraction::ExitAfterExtraction() && !Extraction::OrganizeOuterFolder(),"New options must default off");
        Extraction::SaveExitAfterExtraction(true);
        Require(Extraction::ExitAfterExtraction(),"Exit preference must persist");
        Require(!Extraction::OrganizeOuterFolder(),"Exit setting changed organization");
        Extraction::SaveOrganizeOuterFolder(true);
        Extraction::SaveExitAfterExtraction(false);
        Require(!Extraction::ExitAfterExtraction() && Extraction::OrganizeOuterFolder(),"Preferences must remain independent");
        RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);
        RegCloseKey(isolated);
        RegDeleteTreeW(HKEY_CURRENT_USER,keyName.c_str());
        const auto dark=Design::Colors(true,false), light=Design::Colors(false,false);
        Require(dark.Chrome==RGB(22,26,33) && dark.Content==RGB(26,31,39),"Main dark palette changed");
        Require(light.Chrome==RGB(243,247,253) && light.Content==RGB(255,255,255),"Main light palette changed");
        std::wcout << L"PASS: " << layouts << L" responsive/DPI layouts; " << buttonPaints << L" native button paint states; 8 native combo sizes; preserved controls and menu commands; extraction completion guards; isolated persistent preferences; main palette.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);
        std::cerr << error.what() << '\n'; return 1;
    }
}
