#include "ui/LogTableView.h"
#include "ui/App.h"
#include "ui/ThemeColors.h"
#include "infra/Utf.h"
#include "infra/Clipboard.h"
#include "infra/Dpi.h"
#include "infra/Settings.h"

LogTableView::LogTableView() {}
LogTableView::~LogTableView() {
    DiscardRenderTarget();
    if (textFormat_) textFormat_->Release();
    if (doc_) doc_->listeners.Remove(this);
}

void LogTableView::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

HWND LogTableView::Create(HWND parent, HINSTANCE hInstance, LogDocument* doc) {
    doc_ = doc;
    if (doc_) doc_->listeners.Add(this);

    hwnd_ = CreateWindowExW(0, kClassName, nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
        0, 0, 100, 100, parent, nullptr, hInstance, this);

    // Create DirectWrite text format
    auto* dwrite = LogramApp::Get()->DWriteFactory();
    if (dwrite) {
        dwrite->CreateTextFormat(L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &textFormat_);
        if (textFormat_) {
            textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            // Calculate row height from font metrics
            IDWriteFontCollection* fc = nullptr;
            dwrite->GetSystemFontCollection(&fc);
            if (fc) {
                UINT32 idx = 0;
                BOOL exists = FALSE;
                fc->FindFamilyName(L"Consolas", &idx, &exists);
                if (exists) {
                    IDWriteFontFamily* ff = nullptr;
                    fc->GetFontFamily(idx, &ff);
                    if (ff) {
                        IDWriteFont* font = nullptr;
                        ff->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_REGULAR,
                            DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
                        if (font) {
                            DWRITE_FONT_METRICS fm;
                            font->GetMetrics(&fm);
                            float designToPixels = 12.0f / fm.designUnitsPerEm;
                            float lineHeight = (fm.ascent + fm.descent + fm.lineGap) * designToPixels;
                            rowHeight_ = static_cast<int>(lineHeight * 1.33f + 0.5f);
                            if (rowHeight_ < 16) rowHeight_ = 16;
                            font->Release();
                        }
                        ff->Release();
                    }
                }
                fc->Release();
            }
        }
    }

    return hwnd_;
}

void LogTableView::SetDocument(LogDocument* doc) {
    if (doc_) doc_->listeners.Remove(this);
    doc_ = doc;
    if (doc_) doc_->listeners.Add(this);
    topRow_ = 0;
    selectedRows_.clear();
    UpdateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK LogTableView::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LogTableView* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<LogTableView*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<LogTableView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT LogTableView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_PAINT:
            OnPaint();
            return 0;
        case WM_ERASEBKGND:
            return 1; // D2D handles background
        case WM_VSCROLL:
            OnVScroll(LOWORD(wParam), HIWORD(wParam));
            return 0;
        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            SetFocus(hwnd_);
            return 0;
        case WM_KEYDOWN:
            OnKeyDown(wParam, lParam);
            return 0;
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void LogTableView::CreateRenderTarget() {
    if (rt_) return;
    auto* factory = LogramApp::Get()->D2DFactory();
    if (!factory) return;

    RECT rc;
    GetClientRect(hwnd_, &rc);
    D2D1_SIZE_U size = {static_cast<UINT32>(rc.right), static_cast<UINT32>(rc.bottom)};

    factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        &rt_);

    if (rt_) {
        // Align D2D coordinate system with the monitor DPI so 1 DIP ≈ 1/96".
        // Without this, "12pt" text renders as 12 physical pixels on hi-DPI
        // displays while pixel-sized column offsets stay unscaled.
        UINT dpi = GetDpiForWindow(hwnd_);
        if (dpi == 0) dpi = 96;
        rt_->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
        rt_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &brush_);
    }
}

void LogTableView::DiscardRenderTarget() {
    if (brush_) { brush_->Release(); brush_ = nullptr; }
    if (rt_) { rt_->Release(); rt_ = nullptr; }
}

void LogTableView::OnPaint() {
    PAINTSTRUCT ps;
    BeginPaint(hwnd_, &ps);

    CreateRenderTarget();
    if (!rt_) { EndPaint(hwnd_, &ps); return; }

    auto& theme = CurrentTheme();
    rt_->BeginDraw();
    rt_->Clear(ToD2D(theme.background));

    if (doc_ && textFormat_) {
        const auto& indices = doc_->FilteredIndices();
        const auto& lines = doc_->AllLines();
        const uint8_t* base = doc_->MappedBase();
        int visibleRows = clientHeight_ / rowHeight_ + 1;
        int totalRows = static_cast<int>(indices.size());

        for (int row = 0; row < visibleRows && (topRow_ + row) < totalRows; ++row) {
            int idx = topRow_ + row;
            uint32_t lineId = indices[idx];
            const auto& line = lines[lineId];
            auto level = static_cast<LogLevel>(line.level);

            float y = static_cast<float>(row * rowHeight_);
            D2D1_RECT_F rowRect = {0, y, static_cast<float>(clientWidth_), y + rowHeight_};

            // Row background (level-based)
            auto& rowBg = theme.rowBackground[static_cast<int>(level)];
            if (rowBg.a > 0) {
                brush_->SetColor(ToD2D(rowBg));
                rt_->FillRectangle(rowRect, brush_);
            }

            // Selection highlight
            if (selectedRows_.count(idx)) {
                brush_->SetColor(ToD2D(theme.selection));
                rt_->FillRectangle(rowRect, brush_);
            }

            float x = 4.0f;

            // Time column
            auto timeStr = FormatTime(line.epochCS);
            if (!timeStr.empty()) {
                auto wtime = Utf8ToWide(timeStr);
                brush_->SetColor(ToD2D(theme.secondary));
                D2D1_RECT_F timeRect = {x, y, x + 100.0f, y + rowHeight_};
                rt_->DrawText(wtime.c_str(), static_cast<UINT32>(wtime.size()),
                              textFormat_, timeRect, brush_);
            }
            x += 105.0f;

            // Thread
            if (line.thread >= 0) {
                wchar_t thBuf[4];
                thBuf[0] = static_cast<wchar_t>(line.thread + '!');
                thBuf[1] = 0;
                brush_->SetColor(ToD2D(ThreadColor(line.thread)));
                D2D1_RECT_F thRect = {x, y, x + 14.0f, y + rowHeight_};
                rt_->DrawText(thBuf, 1, textFormat_, thRect, brush_);
            }
            x += 18.0f;

            // Level badge — fixed width so message column aligns
            auto& info = GetLogLevelInfo(level);
            auto wlabel = Utf8ToWide(info.label);
            float badgeWidth = 60.0f;
            D2D1_ROUNDED_RECT badge = {{x, y + 2, x + badgeWidth, y + rowHeight_ - 2}, 3.0f, 3.0f};
            auto badgeColor = theme.levelBadge[static_cast<int>(level)];
            badgeColor.a = 0.4f;
            brush_->SetColor(ToD2D(badgeColor));
            rt_->FillRoundedRectangle(badge, brush_);
            brush_->SetColor(ToD2D(theme.foreground));
            D2D1_RECT_F badgeTextRect = {x + 4, y, x + badgeWidth, y + rowHeight_};
            rt_->DrawText(wlabel.c_str(), static_cast<UINT32>(wlabel.size()),
                          textFormat_, badgeTextRect, brush_);
            x += badgeWidth + 8.0f;

            // Duration column (right-aligned, before message)
            bool showDur = Settings::Instance().GetShowDuration();
            float durColWidth = showDur ? 90.0f : 0.0f;

            // Message
            auto msg = GetMessage(base, line);
            if (!msg.empty()) {
                auto wmsg = Utf8ToWide(msg.substr(0, 500)); // truncate for performance
                brush_->SetColor(ToD2D(theme.messageColor[static_cast<int>(level)]));
                float msgRight = static_cast<float>(clientWidth_) - durColWidth - 4.0f;
                D2D1_RECT_F msgRect = {x, y, msgRight, y + rowHeight_};
                rt_->DrawText(wmsg.c_str(), static_cast<UINT32>(wmsg.size()),
                              textFormat_, msgRect, brush_);
            }

            // Duration value
            if (showDur) {
                int64_t durUS = doc_->GetDuration(lineId);
                if (durUS > 0) {
                    wchar_t durBuf[32];
                    double durMS = durUS / 1000.0;
                    if (durMS >= 1000.0)
                        swprintf(durBuf, 32, L"%.1f s", durMS / 1000.0);
                    else
                        swprintf(durBuf, 32, L"%.3f ms", durMS);

                    brush_->SetColor(ToD2D(DurationColor(durUS)));
                    float durX = static_cast<float>(clientWidth_) - durColWidth;
                    D2D1_RECT_F durRect = {durX, y, static_cast<float>(clientWidth_) - 4.0f, y + rowHeight_};

                    // Right-align: create layout for measuring
                    IDWriteTextLayout* layout = nullptr;
                    LogramApp::Get()->DWriteFactory()->CreateTextLayout(
                        durBuf, static_cast<UINT32>(wcslen(durBuf)),
                        textFormat_, durColWidth - 4.0f, static_cast<float>(rowHeight_), &layout);
                    if (layout) {
                        layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                        rt_->DrawTextLayout({durX, y}, layout, brush_);
                        layout->Release();
                    }
                }
            }
        }
    }

    HRESULT hr = rt_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardRenderTarget();
    }

    EndPaint(hwnd_, &ps);
}

void LogTableView::OnSize(int w, int h) {
    // w/h are physical pixels; drawing coords are DIPs once SetDpi is set.
    float scale = GetDpiForWindow(hwnd_) / 96.0f;
    if (scale <= 0.0f) scale = 1.0f;
    dpiScale_ = scale;
    clientWidth_ = static_cast<int>(w / scale);
    clientHeight_ = static_cast<int>(h / scale);
    if (rt_) {
        rt_->Resize(D2D1::SizeU(w, h));
    }
    UpdateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void LogTableView::UpdateScrollInfo() {
    int totalRows = doc_ ? static_cast<int>(doc_->FilteredIndices().size()) : 0;
    int pageSize = (rowHeight_ > 0) ? (clientHeight_ / rowHeight_) : 1;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = totalRows - 1;
    si.nPage = pageSize;
    si.nPos = topRow_;
    SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);
}

void LogTableView::OnVScroll(int code, int pos) {
    int totalRows = doc_ ? static_cast<int>(doc_->FilteredIndices().size()) : 0;
    int pageSize = clientHeight_ / rowHeight_;
    int oldTop = topRow_;

    switch (code) {
        case SB_LINEUP:    topRow_ = std::max(0, topRow_ - 1); break;
        case SB_LINEDOWN:  topRow_ = std::min(totalRows - pageSize, topRow_ + 1); break;
        case SB_PAGEUP:    topRow_ = std::max(0, topRow_ - pageSize); break;
        case SB_PAGEDOWN:  topRow_ = std::min(totalRows - pageSize, topRow_ + pageSize); break;
        case SB_THUMBTRACK: topRow_ = pos; break;
        case SB_TOP:       topRow_ = 0; break;
        case SB_BOTTOM:    topRow_ = totalRows - pageSize; break;
    }
    topRow_ = std::max(0, topRow_);

    if (topRow_ != oldTop) {
        SetScrollPos(hwnd_, SB_VERT, topRow_, TRUE);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void LogTableView::OnMouseWheel(int delta) {
    int lines = -delta / WHEEL_DELTA * 3;
    int totalRows = doc_ ? static_cast<int>(doc_->FilteredIndices().size()) : 0;
    int pageSize = clientHeight_ / rowHeight_;
    topRow_ = std::clamp(topRow_ + lines, 0, std::max(0, totalRows - pageSize));
    SetScrollPos(hwnd_, SB_VERT, topRow_, TRUE);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

int LogTableView::HitTestRow(int y) const {
    if (rowHeight_ <= 0) return -1;
    // y arrives in physical pixels from WM_LBUTTONDOWN, but rowHeight_ is in
    // DIPs (matches the drawing coordinate system).
    int yDip = static_cast<int>(y / (dpiScale_ > 0.0f ? dpiScale_ : 1.0f));
    return topRow_ + yDip / rowHeight_;
}

void LogTableView::OnLButtonDown(int x, int y, WPARAM keys) {
    int row = HitTestRow(y);
    if (row < 0 || !doc_) return;
    int total = static_cast<int>(doc_->FilteredIndices().size());
    if (row >= total) return;

    if (keys & MK_SHIFT) {
        // Range selection
        size_t from = std::min(anchorRow_, static_cast<size_t>(row));
        size_t to = std::max(anchorRow_, static_cast<size_t>(row));
        if (!(keys & MK_CONTROL)) selectedRows_.clear();
        for (size_t i = from; i <= to; ++i) selectedRows_.insert(i);
    } else if (keys & MK_CONTROL) {
        // Toggle selection
        if (selectedRows_.count(row)) selectedRows_.erase(row);
        else selectedRows_.insert(row);
        anchorRow_ = row;
    } else {
        selectedRows_.clear();
        selectedRows_.insert(row);
        anchorRow_ = row;
    }

    // Notify selection
    doc_->SetSelectedLineId(static_cast<int>(doc_->FilteredIndices()[row]));
    DocumentChanges changes;
    changes.flags = DocumentChanges::SelectionChanged;
    doc_->listeners.Notify(changes);

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void LogTableView::OnKeyDown(WPARAM vk, LPARAM) {
    if (!doc_) return;
    int total = static_cast<int>(doc_->FilteredIndices().size());
    if (total == 0) return;
    int pageSize = clientHeight_ / rowHeight_;

    int current = selectedRows_.empty() ? 0 : static_cast<int>(*selectedRows_.rbegin());

    switch (vk) {
        case VK_UP:    current = std::max(0, current - 1); break;
        case VK_DOWN:  current = std::min(total - 1, current + 1); break;
        case VK_PRIOR: current = std::max(0, current - pageSize); break;
        case VK_NEXT:  current = std::min(total - 1, current + pageSize); break;
        case VK_HOME:  current = 0; break;
        case VK_END:   current = total - 1; break;
        case 'A':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                selectedRows_.clear();
                for (int i = 0; i < total; ++i) selectedRows_.insert(i);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
            return;
        case 'C':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                // Copy selected lines
                std::string text;
                const uint8_t* base = doc_->MappedBase();
                for (auto idx : selectedRows_) {
                    if (idx < doc_->FilteredIndices().size()) {
                        auto lineId = doc_->FilteredIndices()[idx];
                        auto raw = GetRawLine(base, doc_->AllLines()[lineId]);
                        if (!text.empty()) text += '\n';
                        text.append(raw.data(), raw.size());
                    }
                }
                CopyToClipboard(hwnd_, text);
                return;
            }
            return;
        default:
            return;
    }

    selectedRows_.clear();
    selectedRows_.insert(current);
    anchorRow_ = current;
    ScrollToLine(current);

    doc_->SetSelectedLineId(static_cast<int>(doc_->FilteredIndices()[current]));
    DocumentChanges changes;
    changes.flags = DocumentChanges::SelectionChanged;
    doc_->listeners.Notify(changes);

    InvalidateRect(hwnd_, nullptr, FALSE);
}

void LogTableView::ScrollToLine(int filteredIdx) {
    int pageSize = (rowHeight_ > 0) ? clientHeight_ / rowHeight_ : 1;
    if (filteredIdx < topRow_) {
        topRow_ = filteredIdx;
    } else if (filteredIdx >= topRow_ + pageSize) {
        topRow_ = filteredIdx - pageSize + 1;
    }
    topRow_ = std::max(0, topRow_);
    UpdateScrollInfo();
}

void LogTableView::SelectLine(int filteredIdx) {
    selectedRows_.clear();
    selectedRows_.insert(filteredIdx);
    anchorRow_ = filteredIdx;
    ScrollToLine(filteredIdx);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void LogTableView::OnDocumentChanged(DocumentChanges changes) {
    if (changes.Has(DocumentChanges::DataLoaded) ||
        changes.Has(DocumentChanges::FiltersChanged)) {
        topRow_ = 0;
        selectedRows_.clear();
        UpdateScrollInfo();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}
