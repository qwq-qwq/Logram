#include "ui/App.h"
#include "ui/MainWindow.h"
#include "ui/LogTableView.h"
#include "ui/Splitter.h"
#include "ui/FilterSidebar.h"
#include "ui/DetailPanel.h"
#include "resource.h"
#include <d2d1_1.h>
#include <dwrite.h>

LogramApp* LogramApp::s_instance = nullptr;

LogramApp::LogramApp(HINSTANCE hInstance) : hInstance_(hInstance) {
    s_instance = this;
}

LogramApp::~LogramApp() {
    if (d2dFactory_) d2dFactory_->Release();
    if (dwriteFactory_) dwriteFactory_->Release();
    s_instance = nullptr;
}

LogramApp* LogramApp::Get() { return s_instance; }

bool LogramApp::InitD2D() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                   __uuidof(ID2D1Factory1),
                                   reinterpret_cast<void**>(&d2dFactory_));
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(&dwriteFactory_));
    if (FAILED(hr)) return false;

    return true;
}

void LogramApp::RegisterWindowClasses() {
    MainWindow::RegisterClass(hInstance_);
    LogTableView::RegisterClass(hInstance_);
    Splitter::RegisterClass(hInstance_);
    FilterSidebar::RegisterClass(hInstance_);
    DetailPanel::RegisterClass(hInstance_);
}

bool LogramApp::Init(int nCmdShow) {
    if (!InitD2D()) return false;
    RegisterWindowClasses();

    hAccel_ = LoadAcceleratorsW(hInstance_, MAKEINTRESOURCEW(IDR_ACCEL));

    mainWindow_ = new MainWindow();
    if (!mainWindow_->Create(hInstance_, nCmdShow)) {
        delete mainWindow_;
        mainWindow_ = nullptr;
        return false;
    }

    // Enable drag & drop
    DragAcceptFiles(mainWindow_->GetHwnd(), TRUE);
    ChangeWindowMessageFilterEx(mainWindow_->GetHwnd(), WM_DROPFILES, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(mainWindow_->GetHwnd(), WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(mainWindow_->GetHwnd(), 0x0049 /*WM_COPYGLOBALDATA*/, MSGFLT_ALLOW, nullptr);

    return true;
}

int LogramApp::Run() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (hAccel_ && mainWindow_ &&
            TranslateAcceleratorW(mainWindow_->GetHwnd(), hAccel_, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    delete mainWindow_;
    mainWindow_ = nullptr;
    return static_cast<int>(msg.wParam);
}

void LogramApp::OpenFile(const wchar_t* path) {
    if (mainWindow_) mainWindow_->LoadFile(path);
}
