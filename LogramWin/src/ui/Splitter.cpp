#include "ui/Splitter.h"
#include "ui/ThemeColors.h"

void Splitter::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_SIZEWE);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

HWND Splitter::Create(HWND parent, HINSTANCE hInstance, int x, int y, int w, int h) {
    hwnd_ = CreateWindowExW(0, kClassName, nullptr,
        WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, nullptr, hInstance, this);
    return hwnd_;
}

void Splitter::SetPosition(int pos) {
    // Pure setter. The parent drives layout; we only notify it when the
    // user drags (WM_MOUSEMOVE handler). Sending WM_SIZE from here would
    // re-enter LayoutChildren and recurse indefinitely.
    pos_ = pos;
}

LRESULT CALLBACK Splitter::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Splitter* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Splitter*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Splitter*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Splitter::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            SetCapture(hwnd_);
            dragging_ = true;
            dragStart_ = (orient_ == Orientation::Vertical) ?
                GET_X_LPARAM(lParam) : GET_Y_LPARAM(lParam);
            posStart_ = pos_;
            return 0;

        case WM_MOUSEMOVE:
            if (dragging_) {
                int current = (orient_ == Orientation::Vertical) ?
                    GET_X_LPARAM(lParam) : GET_Y_LPARAM(lParam);
                pos_ = posStart_ + (current - dragStart_);
                if (pos_ < 50) pos_ = 50;
                HWND parent = GetParent(hwnd_);
                if (parent) {
                    RECT rc;
                    GetClientRect(parent, &rc);
                    int maxPos = ((orient_ == Orientation::Vertical) ? rc.right : rc.bottom) - 50;
                    if (pos_ > maxPos) pos_ = maxPos;
                    // Trigger parent layout
                    SendMessageW(parent, WM_SIZE, 0,
                        MAKELPARAM(rc.right, rc.bottom));
                }
            }
            return 0;

        case WM_LBUTTONUP:
            if (dragging_) {
                dragging_ = false;
                ReleaseCapture();
            }
            return 0;

        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr,
                orient_ == Orientation::Vertical ? IDC_SIZEWE : IDC_SIZENS));
            return TRUE;

        case WM_ERASEBKGND: {
            auto& theme = CurrentTheme();
            RECT rc;
            GetClientRect(hwnd_, &rc);
            HBRUSH brush = CreateSolidBrush(ToCOLORREF(theme.background));
            FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
            DeleteObject(brush);
            return 1;
        }
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
