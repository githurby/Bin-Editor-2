#include "Resource.h"     // MUST be first so UNICODE/_UNICODE and ID_* macros are defined

#include <windows.h>
#include <windowsx.h>     // GetWindowStyle, GetWindowExStyle, GET_X_LPARAM/GET_Y_LPARAM
#include <gdiplus.h>

#include <algorithm>
#include <string>
#include "BinEditorUtils.h"
#include "Preview.h"
#include "Type.h"

struct PreviewDataLocal {
    std::wstring* imagePath;
    Gdiplus::Image* image;
    double zoom = 1.0;
    int offsetX = 0;
    int offsetY = 0;
    bool dragging = false;
    POINT lastMouse;
};

LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PreviewDataLocal* data = (PreviewDataLocal*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        std::wstring* imagePath = (std::wstring*)((CREATESTRUCT*)lParam)->lpCreateParams;
        PreviewDataLocal* newData = new PreviewDataLocal;
        newData->imagePath = imagePath;
        newData->image = Gdiplus::Image::FromFile(imagePath->c_str());
        if (!newData->image || newData->image->GetLastStatus() != Gdiplus::Ok) {
            MessageBoxW(hwnd, L"Failed to load image.", L"Error", MB_OK | MB_ICONERROR);
            delete newData->image;
            delete newData;
            return -1;
        }
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)newData);
        int imgWidth = newData->image->GetWidth();
        int imgHeight = newData->image->GetHeight();
        if (imgWidth == 0 || imgHeight == 0) {
            MessageBoxW(hwnd, L"Invalid image dimensions.", L"Error", MB_OK | MB_ICONERROR);
            delete newData->image;
            delete newData;
            return -1;
        }
        double aspect = static_cast<double>(imgHeight) / imgWidth;
        int screenW = GetSystemMetrics(SM_CXSCREEN) - 100;
        int screenH = GetSystemMetrics(SM_CYSCREEN) - 100;
        int clientW = std::min(std::max(imgWidth, 300), screenW);
        int clientH = static_cast<int>(clientW * aspect);
        if (clientH > screenH) {
            clientH = screenH;
            clientW = static_cast<int>(clientH / aspect);
        }
        clientW = std::max(300, clientW);
        clientH = std::max(200, clientH);
        RECT rect = {0, 0, clientW, clientH};
        AdjustWindowRectEx(&rect, GetWindowStyle(hwnd), FALSE, GetWindowExStyle(hwnd));
        int winW = rect.right - rect.left;
        int winH = rect.bottom - rect.top;
        int x = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
        int y = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
        SetWindowPos(hwnd, NULL, x, y, winW, winH, SWP_NOZORDER);
        newData->zoom = 1.0;
        if (imgWidth > clientW || imgHeight > clientH) {
            newData->zoom = std::min(static_cast<double>(clientW) / imgWidth, static_cast<double>(clientH) / imgHeight);
        }
        return 0;
    }
    case WM_PAINT: {
        if (!data || !data->image) return DefWindowProcW(hwnd, msg, wParam, lParam);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Gdiplus::Graphics graphics(hdc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int winWidth = rc.right - rc.left;
        int winHeight = rc.bottom - rc.top;
        int imgWidth = data->image->GetWidth();
        int imgHeight = data->image->GetHeight();
        int drawWidth = static_cast<int>(imgWidth * data->zoom);
        int drawHeight = static_cast<int>(imgHeight * data->zoom);
        int drawX = (winWidth - drawWidth) / 2 + data->offsetX;
        int drawY = (winHeight - drawHeight) / 2 + data->offsetY;
        graphics.DrawImage(data->image, drawX, drawY, drawWidth, drawHeight);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (!data) return 0;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        double zoomFactor = (delta > 0) ? 1.1 : 0.9;
        data->zoom *= zoomFactor;
        data->zoom = std::max(0.1, std::min(10.0, data->zoom));
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!data) return 0;
        data->dragging = true;
        data->lastMouse.x = GET_X_LPARAM(lParam);
        data->lastMouse.y = GET_Y_LPARAM(lParam);
        SetCapture(hwnd);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!data) return 0;
        data->dragging = false;
        ReleaseCapture();
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!data || !data->dragging) return 0;
        int dx = GET_X_LPARAM(lParam) - data->lastMouse.x;
        int dy = GET_Y_LPARAM(lParam) - data->lastMouse.y;
        data->offsetX += dx;
        data->offsetY += dy;
        data->lastMouse.x = GET_X_LPARAM(lParam);
        data->lastMouse.y = GET_Y_LPARAM(lParam);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_SIZE: {
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_DESTROY: {
        if (data) {
            delete data->image;
            if (data->imagePath) {
                delete data->imagePath;
            }
            delete data;
        }
        return 0;
    }
    case WM_CLOSE: {
        DestroyWindow(hwnd);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
