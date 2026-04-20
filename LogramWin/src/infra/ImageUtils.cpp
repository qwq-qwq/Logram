#include "infra/ImageUtils.h"
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

HBITMAP LoadPngResourceAsHBITMAP(HINSTANCE hInstance, int resourceId, int size) {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        return nullptr;

    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!hRes) return nullptr;
    HGLOBAL hMem = LoadResource(hInstance, hRes);
    DWORD rSize = SizeofResource(hInstance, hRes);
    void* data = LockResource(hMem);
    if (!data || !rSize) return nullptr;

    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream))) return nullptr;
    if (FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(data), rSize))) return nullptr;

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr,
            WICDecodeMetadataCacheOnLoad, &decoder))) return nullptr;

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return nullptr;

    ComPtr<IWICFormatConverter> conv;
    if (FAILED(factory->CreateFormatConverter(&conv))) return nullptr;
    if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom))) return nullptr;

    ComPtr<IWICBitmapScaler> scaler;
    if (FAILED(factory->CreateBitmapScaler(&scaler))) return nullptr;
    if (FAILED(scaler->Initialize(conv.Get(), size, size,
            WICBitmapInterpolationModeHighQualityCubic))) return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!hbmp || !bits) { if (hbmp) DeleteObject(hbmp); return nullptr; }

    const UINT stride = size * 4;
    if (FAILED(scaler->CopyPixels(nullptr, stride, stride * size,
                                  static_cast<BYTE*>(bits)))) {
        DeleteObject(hbmp);
        return nullptr;
    }

    // Pre-multiply alpha for AlphaBlend (32bppBGRA is straight; AlphaBlend wants PBGRA).
    BYTE* p = static_cast<BYTE*>(bits);
    for (UINT i = 0; i < size * size; ++i) {
        BYTE a = p[3];
        p[0] = static_cast<BYTE>((p[0] * a + 127) / 255);
        p[1] = static_cast<BYTE>((p[1] * a + 127) / 255);
        p[2] = static_cast<BYTE>((p[2] * a + 127) / 255);
        p += 4;
    }
    return hbmp;
}