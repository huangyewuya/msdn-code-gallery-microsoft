// Copyright (c) Microsoft Corporation. All rights reserved

#include "pch.h"
#include "Renderer.h"

using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Microsoft::WRL;

namespace PdfViewModel
{
    // Initialize all DirectX resources
    Renderer::Renderer(_In_ Rect windowBounds, _In_ float dpi)
        : windowBounds(windowBounds), dpi(dpi)
    {
        CreateDeviceIndependentResources();
        CreateDeviceResources();
    }

    // Recreate all device resources and set them back to the current state.
    void Renderer::HandleDeviceLost()
    {
        CreateDeviceResources();
    }

    // Create device independent resources
    void Renderer::CreateDeviceIndependentResources()
    {
        D2D1_FACTORY_OPTIONS options;
        ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
        // If the project is in a debug build, enable Direct2D debugging via Direct2D SDK layer.
        // Enabling SDK debug layer can help catch coding mistakes such as invalid calls and
        // resource leaking that needs to be fixed during the development cycle.
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

        DX::ThrowIfFailed(
            D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory1),
            &options,
            &d2dFactory
            )
        );
    }

    // These are the resources that depend on hardware.
    void Renderer::CreateDeviceResources()
    {
        // This flag adds support for surfaces with a different color channel ordering than the API default.
        // It is recommended usage, and is required for compatibility with Direct2D.
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

        // This array defines the set of DirectX hardware feature levels this app will support.
        // Note the ordering should be preserved.
        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };

        // Create the D3D11 API device object, and get a corresponding context.
        ComPtr<ID3D11Device> d3dDevice;
        ComPtr<ID3D11DeviceContext> d3dContext;
        DX::ThrowIfFailed(
            D3D11CreateDevice(
            nullptr,                    // specify null to use the default adapter
            D3D_DRIVER_TYPE_HARDWARE,
            0,                          // leave as 0 unless software device
            creationFlags,              // optionally set debug and Direct2D compatibility flags
            featureLevels,              // list of feature levels this app can support
            ARRAYSIZE(featureLevels),   // number of entries in above list
            D3D11_SDK_VERSION,          // always set this to D3D11_SDK_VERSION for Modern style apps
            &d3dDevice,                 // returns the Direct3D device created
            &featureLevel,              // returns feature level of device created
            &d3dContext                 // returns the device immediate context
            )
            );

        // Obtain the underlying DXGI device of the Direct3D11.1 device.
        DX::ThrowIfFailed(
            d3dDevice.As(&dxgiDevice)
        );

        // Obtain the Direct2D device for 2-D rendering.
        DX::ThrowIfFailed(
            d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice)
        );

        // And get its corresponding device context object.
        DX::ThrowIfFailed(
            d2dDevice->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            &d2dDeviceContext
            )
        );

        // Since this device context will be used to draw content onto XAML surface image source,
        // it needs to operate as pixels. Setting pixel unit mode is a way to tell Direct2D to treat
        // the incoming coordinates and vectors, typically as DIPs, as in pixels.
        d2dDeviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);

        // Despite treating incoming values as pixels, it is still very important to tell Direct2D
        // the logical DPI the application operates on. Direct2D uses the DPI value as a hint to
        // optimize internal rendering policy such as to determine when is appropriate to enable
        // symmetric text rendering modes. Not specifying the appropriate DPI in this case will hurt
        // application performance.
        d2dDeviceContext->SetDpi(dpi, dpi);

        // When an application performs animation or image composition of graphics content, it is important
        // to use Direct2D grayscale text rendering mode rather than ClearType. The ClearType technique
        // operates on the color channels and not the alpha channel, and therefore unable to correctly perform
        // image composition or sub-pixel animation of text. ClearType is still a method of choice when it
        // comes to direct rendering of text to the destination surface with no subsequent composition required.
        d2dDeviceContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

        DX::ThrowIfFailed(
            PdfCreateRenderer(dxgiDevice.Get(), &pdfRenderer)
        );
    }

    // Helps track the DPI in the helper class.
    void Renderer::SetDpi(_In_ float dpiLocal)
    {
        if (dpi != dpiLocal)
        {
            dpi = dpiLocal;
            d2dDeviceContext->SetDpi(dpi, dpi);
        }
    }

    // Call this method when the app suspends to hint to the driver that the app is entering an idle state
    // and that its memory can be used temporarily for other apps.
    void Renderer::Trim()
    {
        ComPtr<IDXGIDevice3> dxgiDeviceLocal;
        DX::ThrowIfFailed(
            dxgiDevice.As(&dxgiDeviceLocal)
        );

        dxgiDeviceLocal->Trim();
    }

    void Renderer::GetDXGIDevice(_Outptr_ IDXGIDevice** ldxgiDevice)
    {
        DX::ThrowIfFailed(
            dxgiDevice.Get()->QueryInterface(IID_PPV_ARGS(ldxgiDevice))
        );
    }

    void Renderer::GetPdfNativeRenderer(_Outptr_ IPdfRendererNative** nativePdfRenderer)
    {
        DX::ThrowIfFailed(
            pdfRenderer.Get()->QueryInterface(IID_PPV_ARGS(nativePdfRenderer))
        );
    }
}