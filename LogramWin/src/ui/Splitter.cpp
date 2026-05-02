#include "ui/Splitter.h"
#include "ui/ThemeColors.h"

void Splitter::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_SIZEWE);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

HWND Splitter::Create(HWND parent, HINSTANCE hInstance, int x, int y, int w, int h) {
    hwnd_ = CreateWindowExW(0, kClassName, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
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
        case WM_LBUTTONDOWN: {
            SetCapture(hwnd_);
            dragging_ = true;
            // Store the grip offset in splitter-local coords. Every following
            // MOUSEMOVE will be evaluated in PARENT coords (stable frame), so
            // the splitter follows the cursor without oscillation. Reading
            // lParam directly here would be in splitter coords — same thing
            // at click time, since the splitter hasn't moved yet.
            dragStart_ = (orient_ == Orientation::Vertical) ?
                GET_X_LPARAM(lParam) : GET_Y_LPARAM(lParam);
            return 0;
        }

        case WM_MOUSEMOVE:
            if (dragging_) {
                HWND parent = GetParent(hwnd_);
                if (!parent) return 0;
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(parent, &pt);
                int cursorParent = (orient_ == Orientation::Vertical) ? pt.x : pt.y;
                int newPos = cursorParent - dragStart_;
                if (newPos < 50) newPos = 50;
                RECT rc;
                GetClientRect(parent, &rc);
                int maxPos = ((orient_ == Orientation::Vertical) ? rc.right : rc.bottom) - 50;
                if (newPos > maxPos) newPos = maxPos;
                if (newPos != pos_) {
                    pos_ = newPos;
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
            ColorRGBA c;
            c.r = (theme.background.r + theme.secondary.r) * 0.5f;
            c.g = (theme.background.g + theme.secondary.g) * 0.5f;
            c.b = (theme.background.b + theme.secondary.b) * 0.5f;
            c.a = 1.0f;
            COLORREF cref = ToCOLORREF(c);
            if (!hEraseBrush_ || hEraseBrushColor_ != cref) {
                if (hEraseBrush_) DeleteObject(hEraseBrush_);
                hEraseBrush_ = CreateSolidBrush(cref);
                hEraseBrushColor_ = cref;
            }
            RECT rc;
            GetClientRect(hwnd_, &rc);
            FillRect(reinterpret_cast<HDC>(wParam), &rc, hEraseBrush_);
            return 1;
        }
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
