// Ensure Resource.h is the very first include for this translation unit.
#include "Resource.h"

#include "AudioPreview.h"
#include <mmsystem.h>
#include <vector>
#include <windows.h>
#include <iostream>
#include "BinEditorUtils.h"
#include "Type.h" // If you keep AudioPreviewData definition in Type.h; otherwise remove.

DWORD WINAPI MP3PlaybackThread(LPVOID lpParam) {
    AudioPreviewData* data = (AudioPreviewData*)lpParam;
    if (!data || !data->mpg123) return 1;

    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) return 1;

    long rate;
    int channels, encoding;
    if (mpg123_getformat(data->mpg123, &rate, &channels, &encoding) != MPG123_OK) {
        CoUninitialize();
        return 1;
    }

    // Set output format to match file or use safe defaults
    mpg123_format_none(data->mpg123);
    if (mpg123_format(data->mpg123, rate, channels, MPG123_ENC_SIGNED_16) != MPG123_OK) {
        CoUninitialize();
        return 1;
    }

    size_t buffer_size = mpg123_outblock(data->mpg123);
    std::vector<unsigned char> buffer(buffer_size);

    HWAVEOUT hWaveOut = nullptr;
    WAVEFORMATEX wfx = { 0 };
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = static_cast<WORD>(channels);
    wfx.nSamplesPerSec = static_cast<DWORD>(rate);
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = static_cast<WORD>(wfx.nChannels * wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, 0) != MMSYSERR_NOERROR) {
        CoUninitialize();
        return 1;
    }

    std::vector<WAVEHDR> waveHeaders(4); // Increased to 4 for smoother playback
    for (auto& wh : waveHeaders) {
        wh.lpData = (LPSTR)malloc(buffer_size);
        wh.dwBufferLength = (DWORD)buffer_size;
        wh.dwFlags = 0;
        waveOutPrepareHeader(hWaveOut, &wh, sizeof(WAVEHDR));
    }
    size_t currentHeader = 0;

    while (data->isPlaying) {
        size_t done = 0;
        int ret = mpg123_read(data->mpg123, buffer.data(), buffer_size, &done);
        if (ret != MPG123_OK && ret != MPG123_DONE) {
            // Log error for debugging
            const char* err = mpg123_strerror(data->mpg123);
            OutputDebugStringA(("mpg123_read error: " + std::string(err) + "\n").c_str());
            break;
        }

        WAVEHDR& wh = waveHeaders[currentHeader];
        if (wh.dwFlags & WHDR_INQUEUE) {
            while (data->isPlaying && (wh.dwFlags & WHDR_INQUEUE)) {
                Sleep(5); // Reduced sleep for responsiveness
            }
        }
        if (!data->isPlaying) break;

        memcpy(wh.lpData, buffer.data(), done);
        wh.dwBufferLength = (DWORD)done;
        if (waveOutWrite(hWaveOut, &wh, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            break;
        }

        currentHeader = (currentHeader + 1) % waveHeaders.size();
        if (ret == MPG123_DONE) break;

        // Small delay to prevent CPU overload on old PC
        Sleep(1);
    }

    for (auto& wh : waveHeaders) {
        if (wh.dwFlags & WHDR_INQUEUE) {
            while (wh.dwFlags & WHDR_INQUEUE) {
                Sleep(5);
            }
        }
        waveOutUnprepareHeader(hWaveOut, &wh, sizeof(WAVEHDR));
        free(wh.lpData);
    }
    waveOutClose(hWaveOut);
    CoUninitialize();
    return 0;
}

LRESULT CALLBACK AudioPreviewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AudioPreviewData* data = (AudioPreviewData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        std::wstring* audioPath = (std::wstring*)((CREATESTRUCT*)lParam)->lpCreateParams;
        AudioPreviewData* newData = new AudioPreviewData;
        newData->audioPath = audioPath;

        std::filesystem::path path(*audioPath);
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        bool loadSuccess = false;

        if (ext == ".wav" || ext == ".ogg") {
            newData->playType = AudioPreviewData::PlayType::SFML_MUSIC;
            newData->music = new sf::Music;
            std::string sPath = BinEditorUtils::ToString(*audioPath);
            loadSuccess = newData->music->openFromFile(sPath.c_str());
            if (!loadSuccess) {
                delete newData->music;
                newData->music = nullptr;
            }
        } else if (ext == ".mp3") {
            newData->playType = AudioPreviewData::PlayType::MPG123;
            mpg123_init();
            newData->mpg123 = mpg123_new(nullptr, nullptr);
            if (!newData->mpg123) {
                MessageBoxW(hwnd, L"Failed to initialize mpg123 handle.", L"Error", MB_OK | MB_ICONERROR);
                mpg123_exit();
                delete newData;
                return -1;
            }
            std::string sPath = BinEditorUtils::ToString(*audioPath);
            if (mpg123_open(newData->mpg123, sPath.c_str()) != MPG123_OK) {
                MessageBoxW(hwnd, L"Failed to open MP3 file with mpg123.", L"Error", MB_OK | MB_ICONERROR);
                mpg123_delete(newData->mpg123);
                mpg123_exit();
                delete newData;
                return -1;
            }
            // Set buffer size for streaming
            mpg123_param(newData->mpg123, MPG123_RVA, MPG123_RVA_OFF, 0.0); // Disable ReplayGain
            loadSuccess = true;
        } else if (ext == ".wma") {
            newData->playType = AudioPreviewData::PlayType::MCI;
            std::wstring openCmd = L"open \"" + *audioPath + L"\" alias " + newData->mciAlias;
            if (mciSendStringW(openCmd.c_str(), NULL, 0, NULL) != 0) {
                MessageBoxW(hwnd, L"Failed to open WMA with MCI.", L"Error", MB_OK | MB_ICONERROR);
                delete newData;
                return -1;
            }
            loadSuccess = true;
        }

        if (!loadSuccess) {
            MessageBoxW(hwnd, L"Failed to load audio file.", L"Error", MB_OK | MB_ICONERROR);
            if (newData->mpg123) {
                mpg123_delete(newData->mpg123);
                mpg123_exit();
            }
            delete newData;
            return -1;
        }

        newData->hPlayStop = CreateWindowW(L"BUTTON", L"Play", WS_CHILD | WS_VISIBLE,
            100, 30, 100, 30, hwnd, (HMENU)ID_BUTTON_PLAY_STOP, nullptr, nullptr);
        SetTimer(hwnd, ID_TIMER_AUDIO_CHECK, 500, NULL);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)newData);

        // Set window size and position
        int winWidth = 300;
        int winHeight = 100;
        int x = (GetSystemMetrics(SM_CXSCREEN) - winWidth) / 2;
        int y = (GetSystemMetrics(SM_CYSCREEN) - winHeight) / 2;
        SetWindowPos(hwnd, NULL, x, y, winWidth, winHeight, SWP_NOZORDER);
        return 0;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_BUTTON_PLAY_STOP && data) {
            if (data->isPlaying) {
                // Stop playback
                if (data->playType == AudioPreviewData::PlayType::SFML_MUSIC && data->music) {
                    data->music->stop();
                } else if (data->playType == AudioPreviewData::PlayType::SFML_SOUND && data->sound) {
                    data->sound->stop();
                } else if (data->playType == AudioPreviewData::PlayType::MCI) {
                    std::wstring cmd = L"stop " + data->mciAlias;
                    mciSendStringW(cmd.c_str(), NULL, 0, NULL);
                } else if (data->playType == AudioPreviewData::PlayType::MPG123 && data->mpg123) {
                    data->isPlaying = false;
                    if (data->hThread) {
                        WaitForSingleObject(data->hThread, INFINITE);
                        CloseHandle(data->hThread);
                        data->hThread = nullptr;
                    }
                }
                SetWindowTextW(data->hPlayStop, L"Play");
                data->isPlaying = false;
            } else {
                // Start playback
                if (data->playType == AudioPreviewData::PlayType::SFML_MUSIC && data->music) {
                    data->music->play();
                    data->isPlaying = true;
                } else if (data->playType == AudioPreviewData::PlayType::SFML_SOUND && data->sound) {
                    data->sound->play();
                    data->isPlaying = true;
                } else if (data->playType == AudioPreviewData::PlayType::MCI) {
                    std::wstring cmd = L"play " + data->mciAlias + L" from 0";
                    mciSendStringW(cmd.c_str(), NULL, 0, NULL);
                    data->isPlaying = true;
                } else if (data->playType == AudioPreviewData::PlayType::MPG123 && data->mpg123) {
                    data->isPlaying = true;
                    mpg123_seek(data->mpg123, 0, SEEK_SET);
                    data->hThread = CreateThread(NULL, 0, MP3PlaybackThread, data, 0, NULL);
                    if (!data->hThread) {
                        MessageBoxW(hwnd, L"Failed to create playback thread.", L"Error", MB_OK | MB_ICONERROR);
                        data->isPlaying = false;
                    }
                }
                if (data->isPlaying) {
                    SetWindowTextW(data->hPlayStop, L"Stop");
                }
            }
            return 0;
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == ID_TIMER_AUDIO_CHECK && data) {
            bool finished = false;
            if (data->playType == AudioPreviewData::PlayType::SFML_MUSIC && data->music) {
                finished = data->music->getStatus() == sf::SoundSource::Stopped;
            } else if (data->playType == AudioPreviewData::PlayType::SFML_SOUND && data->sound) {
                finished = data->sound->getStatus() == sf::SoundSource::Stopped;
            } else if (data->playType == AudioPreviewData::PlayType::MCI) {
                wchar_t status[128];
                std::wstring cmd = L"status " + data->mciAlias + L" mode";
                mciSendStringW(cmd.c_str(), status, 128, NULL);
                finished = wcscmp(status, L"stopped") == 0;
            } else if (data->playType == AudioPreviewData::PlayType::MPG123 && data->hThread) {
                DWORD exitCode;
                if (GetExitCodeThread(data->hThread, &exitCode) && exitCode != STILL_ACTIVE) {
                    finished = true;
                    CloseHandle(data->hThread);
                    data->hThread = nullptr;
                }
            }
            if (finished && data->isPlaying) {
                data->isPlaying = false;
                SetWindowTextW(data->hPlayStop, L"Play");
            }
        }
        return 0;
    }
    case WM_DESTROY: {
        if (data) {
            KillTimer(hwnd, ID_TIMER_AUDIO_CHECK);
            if (data->playType == AudioPreviewData::PlayType::MCI) {
                std::wstring cmd = L"close " + data->mciAlias;
                mciSendStringW(cmd.c_str(), NULL, 0, NULL);
            }
            if (data->playType == AudioPreviewData::PlayType::MPG123 && data->mpg123) {
                data->isPlaying = false;
                if (data->hThread) {
                    WaitForSingleObject(data->hThread, INFINITE);
                    CloseHandle(data->hThread);
                }
                mpg123_close(data->mpg123);
                mpg123_delete(data->mpg123);
                mpg123_exit();
            }
            if (data->music) {
                data->music->stop();
                delete data->music;
            }
            if (data->sound) {
                data->sound->stop();
                delete data->sound;
            }
            if (data->buffer) {
                delete data->buffer;
            }
            if (data->audioPath) {
                delete data->audioPath;
            }
            delete data;
        }
        break;
    }
    case WM_CLOSE: {
        DestroyWindow(hwnd);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}