#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <windows.h>
#include <gdiplus.h>
#include <SFML/Audio.hpp>
#include "mpg123.h"

// Core data types used across modules

struct BinEntry {
    std::string name;
    uint64_t size;
    uint64_t dataOffset;
    std::vector<char> data;
    uint64_t last_modified = 0;
};

struct ParsedMetadata {
    std::string name;
    uint64_t offset;
    uint64_t size;
};

struct ProgressDialogData {
    bool cancelled;
};

struct AddResult {
    int added = 0;
    int skipped = 0;
    int failed = 0;

    AddResult& operator+=(const AddResult& other) {
        added += other.added;
        skipped += other.skipped;
        failed += other.failed;
        return *this;
    }
};

struct PreviewData {
    std::wstring* imagePath;
    Gdiplus::Image* image;
    double zoom = 1.0;
    int offsetX = 0;
    int offsetY = 0;
    bool dragging = false;
    POINT lastMouse;
};

struct AudioPreviewData {
    std::wstring* audioPath;
    enum class PlayType { NONE, SFML_MUSIC, SFML_SOUND, MCI, MPG123 } playType = PlayType::NONE;
    sf::Music* music = nullptr;
    sf::SoundBuffer* buffer = nullptr;
    sf::Sound* sound = nullptr;
    mpg123_handle* mpg123 = nullptr;
    HANDLE hThread = nullptr;
    bool isPlaying = false;
    HWND hPlayStop = nullptr;
    std::wstring mciAlias = L"audio_preview";
};