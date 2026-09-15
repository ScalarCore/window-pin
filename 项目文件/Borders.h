// Adapted from Microsoft PowerToys WindowBorder.cpp and FrameDrawer.cpp.
// Copyright (c) Microsoft Corporation. MIT license: ../文档/PowerToys-LICENSE.txt
#include <d2d1.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
constexpr wchar_t BorderClass[] = L"FelixWindowPinBorder";
constexpr UINT BorderEvent = WM_APP + 2;
HWND mainWindow = nullptr;
HWINEVENTHOOK borderHook = nullptr;
bool borderUpdatePending = false;
struct Border {
    HWND target = nullptr, frame = nullptr;
    RECT bounds{};
    int thickness = 0;
    float radius = 0;
    ComPtr<ID2D1HwndRenderTarget> render;
    ComPtr<ID2D1SolidColorBrush> brush;
};
std::vector<Border> borders;
ComPtr<ID2D1Factory> borderFactory;

float CornerRadius(HWND target) {
    if (IsZoomed(target)) return 0;
    DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_DEFAULT;
    if (FAILED(DwmGetWindowAttribute(target, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference)))) return 0;
    int radius = 0;
    switch (preference) {
    case DWMWCP_DEFAULT:
    case DWMWCP_ROUND: radius = 8; break;
    case DWMWCP_ROUNDSMALL: radius = 4; break;
    default: return 0;
    }
    return radius * GetDpiForWindow(target) / 96.f;
}

LRESULT CALLBACK BorderProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_ERASEBKGND) return TRUE;
    if (msg == WM_PAINT) { ValidateRect(window, nullptr); return 0; }
    return DefWindowProcW(window, msg, wp, lp);
}

bool RenderBorder(Border& border, int width, int height) {
    if (!borderFactory && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, borderFactory.GetAddressOf()))) return false;
    if (border.render) {
        auto size = border.render->GetPixelSize();
        if ((size.width != static_cast<UINT>(width) || size.height != static_cast<UINT>(height)) &&
            FAILED(border.render->Resize(D2D1::SizeU(width, height)))) {
            border.brush.Reset(); border.render.Reset();
        }
    }
    if (!border.render) {
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
        if (FAILED(borderFactory->CreateHwndRenderTarget(properties,
            D2D1::HwndRenderTargetProperties(border.frame, D2D1::SizeU(width, height)),
            border.render.GetAddressOf()))) return false;
    }
    if (!border.brush && FAILED(border.render->CreateSolidColorBrush(D2D1::ColorF(0x2563EB), border.brush.GetAddressOf()))) return false;
    border.render->BeginDraw();
    border.render->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
    float inset = border.thickness / 2.f + 1.f;
    auto rect = D2D1::RectF(inset, inset, width - inset, height - inset);
    if (border.radius > 0)
        border.render->DrawRoundedRectangle(D2D1::RoundedRect(rect, border.radius, border.radius),
            border.brush.Get(), static_cast<float>(border.thickness));
    else
        border.render->DrawRectangle(rect, border.brush.Get(), static_cast<float>(border.thickness));
    HRESULT result = border.render->EndDraw();
    if (FAILED(result)) { border.brush.Reset(); border.render.Reset(); }
    return SUCCEEDED(result);
}

void UpdateBorders() {
    for (auto it = borders.begin(); it != borders.end();) {
        if (!IsWindow(it->target) || !GetPropW(it->target, PinProperty) ||
            !(GetWindowLongPtrW(it->target, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
            DestroyWindow(it->frame); it = borders.erase(it); continue;
        }
        DWORD cloaked = 0;
        DwmGetWindowAttribute(it->target, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        if (!IsWindowVisible(it->target) || IsIconic(it->target) || cloaked) {
            ShowWindow(it->frame, SW_HIDE); ++it; continue;
        }
        RECT rect{};
        if (FAILED(DwmGetWindowAttribute(it->target, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect))) &&
            !GetWindowRect(it->target, &rect)) { ++it; continue; }
        int thickness = MulDiv(3, GetDpiForWindow(it->target), 96);
        InflateRect(&rect, thickness, thickness);
        int width = rect.right - rect.left, height = rect.bottom - rect.top;
        if (width <= thickness * 2 || height <= thickness * 2) { ++it; continue; }
        bool resized = width != it->bounds.right - it->bounds.left || height != it->bounds.bottom - it->bounds.top;
        float radius = CornerRadius(it->target);
        bool redraw = resized || radius != it->radius || thickness != it->thickness || !IsWindowVisible(it->frame) || !it->render;
        // Keep the border immediately behind its target, rather than repeatedly
        // bringing an overlay above every topmost window. Moving needs no repaint.
        if (!EqualRect(&rect, &it->bounds) || !IsWindowVisible(it->frame) || GetWindow(it->target, GW_HWNDNEXT) != it->frame)
            SetWindowPos(it->frame, it->target, rect.left, rect.top, width, height,
                SWP_NOACTIVATE | SWP_NOREDRAW | SWP_SHOWWINDOW);
        it->bounds = rect; it->thickness = thickness; it->radius = radius;
        if (redraw) RenderBorder(*it, width, height);
        ++it;
    }
    if (borders.empty() && mainWindow) KillTimer(mainWindow, 3);
}

void CALLBACK BorderWinEvent(HWINEVENTHOOK, DWORD event, HWND target, LONG object, LONG, DWORD, DWORD) {
    if (!mainWindow || borderUpdatePending || !target) return;
    if (event >= EVENT_OBJECT_CREATE && object != OBJID_WINDOW) return;
    for (const auto& border : borders) {
        if (target == border.target) {
            borderUpdatePending = PostMessageW(mainWindow, BorderEvent, 0, 0) != FALSE;
            return;
        }
    }
}

void AddBorder(HWND target) {
    for (const auto& border : borders) if (border.target == target) return;
    HWND frame = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOPMOST,
        BorderClass, L"", WS_POPUP | WS_DISABLED, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!frame) return;
    // PowerToys uses a tiny off-screen blur region to enable transparent DWM composition.
    int position = -GetSystemMetrics(SM_CXVIRTUALSCREEN) - 8;
    HRGN region = CreateRectRgn(position, 0, position + 1, 1);
    DWM_BLURBEHIND blur{DWM_BB_ENABLE | DWM_BB_BLURREGION, TRUE, region, FALSE};
    DwmEnableBlurBehindWindow(frame, &blur); DeleteObject(region);
    SetLayeredWindowAttributes(frame, RGB(0, 0, 0), 0, LWA_COLORKEY);
    SetLayeredWindowAttributes(frame, 0, 255, LWA_ALPHA);
    BOOL excluded = TRUE;
    DwmSetWindowAttribute(frame, DWMWA_EXCLUDED_FROM_PEEK, &excluded, sizeof(excluded));
    Border border; border.target = target; border.frame = frame;
    borders.push_back(std::move(border));
    if (mainWindow) SetTimer(mainWindow, 3, 100, nullptr);
    UpdateBorders();
}
