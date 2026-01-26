#include "Resource.h"
#include <windows.h>
#include <gdiplus.h>
#include "MainWindow.h"
#include "Preview.h"
#include "AudioPreview.h"
#include "Resource.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    WNDCLASSW wcPreview = { 0 };
    wcPreview.lpfnWndProc = PreviewWndProc;
    wcPreview.hInstance = hInstance;
    wcPreview.lpszClassName = L"ImagePreviewClass";
    wcPreview.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (!RegisterClassW(&wcPreview)) {
        MessageBoxW(nullptr, L"Failed to register preview window class.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSW wcAudioPreview = { 0 };
    wcAudioPreview.lpfnWndProc = AudioPreviewWndProc;
    wcAudioPreview.hInstance = hInstance;
    wcAudioPreview.lpszClassName = L"AudioPreviewClass";
    wcAudioPreview.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wcAudioPreview)) {
        MessageBoxW(nullptr, L"Failed to register audio preview window class.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BinEditorClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register main window class.", L"Error", MB_OK | MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 600;
    int windowHeight = 530;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    HWND hwnd = CreateWindowW(
        L"BinEditorClass",
        L"Binary File Editor - V.1.8 - By E.M - Softy",
        style,
        x, y,
        windowWidth, windowHeight,
        nullptr, nullptr,
        hInstance, nullptr
    );

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);

    return (int)msg.wParam;
}
