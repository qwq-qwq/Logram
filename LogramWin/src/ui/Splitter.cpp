#include "ui/Splitter.h"
#include "ui/ThemeColors.h"
#include "resource.h"

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
        case WM_LBUTTONDOWN: {
            SetCapture(hwnd_);
            dragging_ = true;
            // Grip offset within the splitter at click time. Following
            // MOUSEMOVE handlers evaluate the cursor in PARENT coords (a
            // stable reference frame) and place the splitter at
            // (cursorParent - dragStart_). Reading the cursor in
            // splitter-local coords after the splitter has moved would
            // create self-referential oscillation between two positions.
            dragStart_ = (orient_ == Orientation::Vertical) ?
                GET_X_LPARAM(lParam) : GET_Y_LPARAM(lParam);
            // Tell the parent a drag is starting so it can hide owner-draw
            // toolbar buttons that would otherwise flicker each layout pass.
            HWND parent = GetParent(hwnd_);
            if (parent) SendMessageW(parent, WM_APP_SPLITTER_DRAG, 1, 0);
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
                HWND parent = GetParent(hwnd_);
                if (parent) SendMessageW(parent, WM_APP_SPLITTER_DRAG, 0, 0);
            }
            return 0;

        case WM_CAPTURECHANGED:
            // If capture was stolen mid-drag (alt-tab, etc.) make sure the
            // parent re-shows the toolbar buttons.
            if (dragging_) {
                dragging_ = false;
                HWND parent = GetParent(hwnd_);
                if (parent) SendMessageW(parent, WM_APP_SPLITTER_DRAG, 0, 0);
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
            ColorRGBA c;
            c.r = (theme.background.r + theme.secondary.r) * 0.5f;
            c.g = (theme.background.g + theme.secondary.g) * 0.5f;
            c.b = (theme.background.b + theme.secondary.b) * 0.5f;
            c.a = 1.0f;
            HBRUSH brush = CreateSolidBrush(ToCOLORREF(c));
            FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
            DeleteObject(brush);
            return 1;
        }
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}
