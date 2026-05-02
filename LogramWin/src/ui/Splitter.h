#pragma once
#include <windows.h>

class Splitter {
public:
    enum class Orientation { Horizontal, Vertical };

    Splitter(Orientation orient) : orient_(orient) {}

    static void RegisterClass(HINSTANCE hInstance);
    HWND Create(HWND parent, HINSTANCE hInstance, int x, int y, int w, int h);
    HWND GetHwnd() const { return hwnd_; }

    int GetPosition() const { return pos_; }
    void SetPosition(int pos);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    Orientation orient_;
    int pos_ = 200;
    bool dragging_ = false;
    int dragStart_ = 0;
    int posStart_ = 0;

    static constexpr const wchar_t* kClassName = L"LogramSplitter";
};
