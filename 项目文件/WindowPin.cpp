#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cwchar>
#include <string>
#include <vector>
#include <dwmapi.h>

constexpr wchar_t ClassName[] = L"FelixWindowPinWindow";
constexpr wchar_t PinProperty[] = L"FelixWindowPin.Owned";
constexpr UINT TrayMessage = WM_APP + 1;
UINT taskbarCreated;
NOTIFYICONDATAW tray{};
bool menuOpen = false;
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RunName[] = L"FelixWindowPin";
#include "Borders.h"

std::wstring StartupLink() {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_CREATE, nullptr, &folder))) return {};
    std::wstring path = std::wstring(folder) + L"\\WindowPin.lnk";
    CoTaskMemFree(folder);
    return path;
}

bool StartupEnabled() {
    auto path = StartupLink();
    return !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool SetStartup(bool enabled) {
    auto path = StartupLink();
    if (path.empty()) return false;
    bool success = false;
    if (enabled) {
        wchar_t exe[32768];
        DWORD length = GetModuleFileNameW(nullptr, exe, ARRAYSIZE(exe));
        if (!length || length >= ARRAYSIZE(exe)) return false;
        IShellLinkW* link = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
        if (SUCCEEDED(hr)) {
            hr = link->SetPath(exe);
            std::wstring dir(exe, length);
            dir.resize(dir.find_last_of(L"\\/"));
            if (SUCCEEDED(hr)) hr = link->SetWorkingDirectory(dir.c_str());
            if (SUCCEEDED(hr)) hr = link->SetDescription(L"快捷键置顶 — Win + Ctrl + T");
            if (SUCCEEDED(hr)) hr = link->SetShowCmd(SW_SHOWNORMAL);
            IPersistFile* file = nullptr;
            if (SUCCEEDED(hr)) hr = link->QueryInterface(IID_PPV_ARGS(&file));
            if (SUCCEEDED(hr)) { hr = file->Save(path.c_str(), TRUE); file->Release(); }
            link->Release();
        }
        success = SUCCEEDED(hr);
    } else {
        success = DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    // Remove the previous Run registration after the replacement succeeds.
    if (success) {
        HKEY key;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RunKey, 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
            RegDeleteValueW(key, RunName);
            RegCloseKey(key);
        }
    }
    return success;
}
bool Toggle(HWND target) {
    if (!target || target == GetDesktopWindow() || target == GetShellWindow()) return false;
    bool pinned = (GetWindowLongPtrW(target, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (!pinned && !SetPropW(target, PinProperty, reinterpret_cast<HANDLE>(1))) return false;
    if (!SetWindowPos(target, pinned ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
        if (!pinned) RemovePropW(target, PinProperty);
        return false;
    }
    if (pinned) RemovePropW(target, PinProperty);
    if (pinned) UpdateBorders(); else AddBorder(target);
    return true;
}

BOOL CALLBACK Unpin(HWND window, LPARAM) {
    if (GetPropW(window, PinProperty)) {
        SetWindowPos(window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        RemovePropW(window, PinProperty);
        UpdateBorders();
    }
    return TRUE;
}

void AddTray() {
    if (Shell_NotifyIconW(NIM_ADD, &tray) || Shell_NotifyIconW(NIM_MODIFY, &tray))
        KillTimer(tray.hWnd, 2);
    else
        SetTimer(tray.hWnd, 2, 2000, nullptr);
}

LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == taskbarCreated) { AddTray(); return 0; }
    switch (msg) {
    case BorderEvent:
        borderUpdatePending = false; UpdateBorders(); return 0;
    case WM_ACTIVATEAPP:
        if (!wp && menuOpen) EndMenu();
        break;
    case WM_CANCELMODE:
        if (menuOpen) EndMenu();
        break;
    case WM_TIMER:
        if (wp == 3) { UpdateBorders(); return 0; }
        if (wp == 2) { AddTray(); return 0; }
        if (wp == 1 && menuOpen && GetForegroundWindow() != window) EndMenu();
        return 0;
    case WM_HOTKEY:
        if (wp == 1 && !Toggle(GetForegroundWindow())) MessageBeep(MB_ICONWARNING);
        return 0;
    case TrayMessage:
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            if (menuOpen) { EndMenu(); return 0; }
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"取消全部置顶");
            AppendMenuW(menu, MF_STRING | (StartupEnabled() ? MF_CHECKED : MF_UNCHECKED), 3, L"开机启动");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, 2, L"退出");
            POINT p; GetCursorPos(&p);
            // A visible, zero-sized tool window can own foreground activation
            // without appearing on the desktop or taskbar.
            SetWindowPos(window, nullptr, p.x, p.y, 0, 0,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SetForegroundWindow(window);
            menuOpen = true;
            SetTimer(window, 1, 100, nullptr);
            int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, window, nullptr);
            KillTimer(window, 1);
            menuOpen = false;
            ShowWindow(window, SW_HIDE);
            DestroyMenu(menu); PostMessageW(window, WM_NULL, 0, 0);
            if (command == 1) EnumWindows(Unpin, 0);
            if (command == 2) DestroyWindow(window);
            if (command == 3 && !SetStartup(!StartupEnabled()))
                MessageBoxW(window, L"无法更新开机启动设置，请重试。", L"快捷键置顶", MB_OK | MB_ICONWARNING);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(window, 3);
        if (borderHook) { UnhookWinEvent(borderHook); borderHook = nullptr; }
        for (const auto& border : borders) DestroyWindow(border.frame);
        borders.clear();
        UnregisterHotKey(window, 1); EnumWindows(Unpin, 0);
        Shell_NotifyIconW(NIM_DELETE, &tray); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR args, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    struct ComScope { HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        ~ComScope() { if (SUCCEEDED(result)) CoUninitialize(); } } com;
    if (wcscmp(args, L"--enable-startup") == 0) return SetStartup(true) ? 0 : 7;
    if (wcscmp(args, L"--disable-startup") == 0) return SetStartup(false) ? 0 : 7;
    if (wcscmp(args, L"--exit") == 0) {
        HWND existing = FindWindowW(ClassName, nullptr);
        if (existing) PostMessageW(existing, WM_CLOSE, 0, 0);
        return 0;
    }
    WNDCLASSW wc{}; wc.lpfnWndProc = WindowProc; wc.hInstance = instance; wc.lpszClassName = ClassName;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (!RegisterClassW(&wc)) return 1;
    WNDCLASSW bc{}; bc.lpfnWndProc = BorderProc; bc.hInstance = instance; bc.lpszClassName = BorderClass;
    if (!RegisterClassW(&bc)) return 1;
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, ClassName, L"快捷键置顶", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!window) return 2;
    mainWindow = window;
    borderHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, BorderWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (wcscmp(args, L"--self-test") == 0) {
        HWND test = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Window Pin test", WS_OVERLAPPEDWINDOW,
            100, 100, 400, 300, nullptr, nullptr, instance, nullptr);
        SetWindowPos(test, nullptr, 100, 100, 400, 300, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        bool ok = Toggle(test) && borders.size() == 1 && borders.front().render && IsWindowVisible(borders.front().frame);
        for (int step = 0; step < 60; ++step) {
            SetWindowPos(test, nullptr, 160 + step, 140 + step, 450 + step, 320 + step, SWP_NOZORDER | SWP_NOACTIVATE);
            UpdateBorders();
            RECT expected{}, actual{};
            DwmGetWindowAttribute(test, DWMWA_EXTENDED_FRAME_BOUNDS, &expected, sizeof(expected));
            if (!borders.empty()) {
                InflateRect(&expected, borders.front().thickness, borders.front().thickness);
                GetWindowRect(borders.front().frame, &actual);
                ok = EqualRect(&expected, &actual) && borders.front().render && ok;
            } else ok = false;
        }
        ShowWindow(test, SW_MINIMIZE); UpdateBorders();
        ok = !borders.empty() && !IsWindowVisible(borders.front().frame) && ok;
        ShowWindow(test, SW_SHOWNOACTIVATE); UpdateBorders();
        ok = !borders.empty() && IsWindowVisible(borders.front().frame) && ok;
        // Exercise the event hook, without calling UpdateBorders directly.
        SetWindowPos(test, nullptr, 310, 270, 510, 390, SWP_NOZORDER | SWP_NOACTIVATE);
        NotifyWinEvent(EVENT_OBJECT_LOCATIONCHANGE, test, OBJID_WINDOW, CHILDID_SELF);
        KillTimer(window, 3);
        DWORD deadline = GetTickCount() + 500;
        RECT expected{}, actual{};
        DwmGetWindowAttribute(test, DWMWA_EXTENDED_FRAME_BOUNDS, &expected, sizeof(expected));
        if (!borders.empty()) InflateRect(&expected, borders.front().thickness, borders.front().thickness);
        while (static_cast<LONG>(deadline - GetTickCount()) > 0) {
            MSG event;
            while (PeekMessageW(&event, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&event); DispatchMessageW(&event);
            }
            if (!borders.empty()) GetWindowRect(borders.front().frame, &actual);
            if (EqualRect(&expected, &actual)) break;
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
        }
        ok = EqualRect(&expected, &actual) && ok;
        ok = Toggle(test) && borders.empty() && ok;
        ok = Toggle(test) && ok;
        DestroyWindow(test); UpdateBorders();
        ok = borders.empty() && ok;
        DestroyWindow(window); return ok ? 0 : 3;
    }
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\FelixWindowPin");
    if (!mutex) return 4;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(mutex); return 0; }
    if (!RegisterHotKey(window, 1, MOD_WIN | MOD_CONTROL | MOD_NOREPEAT, 'T')) {
        MessageBoxW(nullptr, L"Win + Ctrl + T 已被其他程序占用。\n请先关闭 PowerToys 的置顶功能或其他使用此快捷键的程序，再启动。", L"快捷键置顶", MB_OK | MB_ICONWARNING);
        CloseHandle(mutex); return 5;
    }
    tray.cbSize = sizeof(tray); tray.hWnd = window; tray.uID = 1;
    tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; tray.uCallbackMessage = TrayMessage;
    tray.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    lstrcpyW(tray.szTip, L"快捷键置顶 — Win + Ctrl + T"); AddTray();
    MSG msg; BOOL result;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (result == -1) DestroyWindow(window);
    CloseHandle(mutex); return result == -1 ? 6 : 0;
}






