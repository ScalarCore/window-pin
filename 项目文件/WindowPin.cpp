#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <cwchar>
#include <string>

constexpr wchar_t ClassName[] = L"FelixWindowPinWindow";
constexpr wchar_t PinProperty[] = L"FelixWindowPin.Owned";
constexpr UINT TrayMessage = WM_APP + 1;
UINT taskbarCreated;
NOTIFYICONDATAW tray{};
bool menuOpen = false;
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RunName[] = L"FelixWindowPin";

std::wstring StartupCommand() {
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (!length || length >= ARRAYSIZE(path)) return {};
    return L"\"" + std::wstring(path, length) + L"\"";
}

bool StartupEnabled() {
    wchar_t value[32770]{};
    DWORD bytes = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, RunKey, RunName, RRF_RT_REG_SZ,
                        nullptr, value, &bytes) == ERROR_SUCCESS && StartupCommand() == value;
}

bool SetStartup(bool enabled) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RunKey, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS result;
    if (enabled) {
        auto command = StartupCommand();
        result = command.empty() ? ERROR_INVALID_DATA : RegSetValueExW(key, RunName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, RunName);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
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
    return true;
}

BOOL CALLBACK Unpin(HWND window, LPARAM) {
    if (GetPropW(window, PinProperty)) {
        SetWindowPos(window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        RemovePropW(window, PinProperty);
    }
    return TRUE;
}

void AddTray() { Shell_NotifyIconW(NIM_ADD, &tray); }

LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == taskbarCreated) { AddTray(); return 0; }
    switch (msg) {
    case WM_ACTIVATEAPP:
        if (!wp && menuOpen) EndMenu();
        break;
    case WM_CANCELMODE:
        if (menuOpen) EndMenu();
        break;
    case WM_TIMER:
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
        UnregisterHotKey(window, 1); EnumWindows(Unpin, 0);
        Shell_NotifyIconW(NIM_DELETE, &tray); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR args, int) {
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
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, ClassName, L"快捷键置顶", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!window) return 2;
    if (wcscmp(args, L"--self-test") == 0) {
        bool ok = Toggle(window) && (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST);
        ok = ok && Toggle(window) && !(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST);
        ok = ok && Toggle(window); EnumWindows(Unpin, 0);
        ok = ok && !(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) && !GetPropW(window, PinProperty);
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


