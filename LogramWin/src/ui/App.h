#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <string>

class MainWindow;

class LogramApp {
public:
    explicit LogramApp(HINSTANCE hInstance);
    ~LogramApp();

    bool Init(int nCmdShow);
    int Run();
    void OpenFile(const wchar_t* path);

    HINSTANCE GetInstance() const { return hInstance_; }
    ID2D1Factory1* D2DFactory() const { return d2dFactory_; }
    IDWriteFactory* DWriteFactory() const { return dwriteFactory_; }

    static LogramApp* Get();

private:
    bool InitD2D();
    void RegisterWindowClasses();

    HINSTANCE hInstance_;
    HACCEL hAccel_ = nullptr;
    MainWindow* mainWindow_ = nullptr;

    ID2D1Factory1* d2dFactory_ = nullptr;
    IDWriteFactory* dwriteFactory_ = nullptr;

    static LogramApp* s_instance;
};
