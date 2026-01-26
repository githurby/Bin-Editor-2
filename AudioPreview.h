#pragma once
// Resource.h must come first so UNICODE/_UNICODE and resource IDs are defined before windows headers.
#include "Resource.h"

#include <windows.h>
#include <SFML/Audio.hpp>
#include <string>
#include "mpg123.h"

// Forward declaration to avoid duplicate struct definitions if you already have AudioPreviewData in Type.h
struct AudioPreviewData;

LRESULT CALLBACK AudioPreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Playback thread declaration (must match the definition in the .cpp file)
DWORD WINAPI MP3PlaybackThread(LPVOID lpParam);