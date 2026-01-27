#include "Resource.h"
#include "MainWindow.h"
#include "Dialogs.h"
#include "BinEditor.h"
#include "BinEditorUtils.h"
#include "Preview.h"
#include "AudioPreview.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <string>
#include <set>
#include <regex>
#include <algorithm>
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hList;
    static BinEditor* editor = nullptr;
    static int contextIndex = -1;
    static HFONT hFontList;
    static std::filesystem::path tempBinsPath;

    switch (msg) {
    case WM_CREATE: {
        wchar_t tempPath[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempPath) == 0) {
            MessageBoxW(nullptr, L"Failed to get temporary path.", L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
        tempBinsPath = std::filesystem::path(tempPath) / L"Temp Bins";
        try {
            std::filesystem::create_directories(tempBinsPath);
        } catch (const std::exception& e) {
            MessageBoxW(nullptr, (std::wstring(L"Failed to create Temp Bins folder: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
        if (!std::filesystem::exists(tempBinsPath)) {
            MessageBoxW(nullptr, L"Temp Bins folder does not exist after creation.", L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
        editor = new BinEditor(tempBinsPath);

        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS };
        InitCommonControlsEx(&icc);

        hList = CreateWindowW(L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL | WS_HSCROLL | LBS_MULTIPLESEL | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            20, 20, 400, 450, hwnd, (HMENU)ID_LISTBOX, nullptr, nullptr);
        DragAcceptFiles(hList, TRUE);

        hFontList = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
        SendMessage(hList, WM_SETFONT, (WPARAM)hFontList, TRUE);

        int buttonX = 440;
        int buttonWidth = 140;
        int buttonHeight = 30;
        int startY = 20;
        int interval = 58;

        CreateWindowW(L"BUTTON", L"Open BIN", WS_CHILD | WS_VISIBLE,
            buttonX, startY, buttonWidth, buttonHeight, hwnd, (HMENU)ID_BUTTON_OPEN, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE,
            buttonX, startY + interval, buttonWidth, buttonHeight, hwnd, (HMENU)ID_BUTTON_ADD, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE,
            buttonX, startY + 2 * interval, buttonWidth, buttonHeight, hwnd, (HMENU)ID_BUTTON_REMOVE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Replace", WS_CHILD | WS_VISIBLE,
            buttonX, startY + 3 * interval, buttonWidth, buttonHeight, hwnd, (HMENU)ID_BUTTON_REPLACE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Export", WS_CHILD | WS_VISIBLE,
            buttonX, startY + 4 * interval, buttonWidth, buttonHeight, hwnd, (HMENU)ID_BUTTON_EXPORT, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Save BIN", WS_CHILD | WS_VISIBLE,
            buttonX, startY + 5 * interval, buttonWidth, buttonHeight, hwnd, (HMENU)ID_BUTTON_SAVE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Create New Bin", WS_CHILD | WS_VISIBLE,
            buttonX, startY + 6 * interval, buttonWidth, buttonHeight, hwnd, (HMENU)ID_BUTTON_CREATE_BIN, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Exit", WS_CHILD | WS_VISIBLE,
            buttonX, startY + 7 * interval, buttonWidth, buttonHeight, hwnd, (HMENU)ID_BUTTON_EXIT, nullptr, nullptr);
        break;
    }
    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
        if (lpmis->CtlID == ID_LISTBOX) {
            lpmis->itemHeight = 24;
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlID == ID_LISTBOX && lpdis->itemID != (UINT)-1) {
            HDC hdc = lpdis->hDC;
            RECT rc = lpdis->rcItem;
            COLORREF bgColor, fgColor;
            if (lpdis->itemState & ODS_SELECTED) {
                bgColor = GetSysColor(COLOR_HIGHLIGHT);
                fgColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
            } else {
                bgColor = (lpdis->itemID % 2 == 0) ? RGB(255, 255, 255) : RGB(240, 240, 240);
                fgColor = GetSysColor(COLOR_WINDOWTEXT);
            }
            HBRUSH hbr = CreateSolidBrush(bgColor);
            FillRect(hdc, &rc, hbr);
            DeleteObject(hbr);
            char text[256];
            SendMessageA(lpdis->hwndItem, LB_GETTEXT, lpdis->itemID, (LPARAM)text);
            SetBkColor(hdc, bgColor);
            SetTextColor(hdc, fgColor);
            DrawTextA(hdc, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            if (lpdis->itemState & ODS_FOCUS) {
                DrawFocusRect(hdc, &rc);
            }
            return TRUE;
        }
        break;
    }
    case WM_CONTEXTMENU: {
        if ((HWND)wParam == hList) {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            POINT pt = { xPos, yPos };
            contextIndex = GetListBoxItemUnderMouse(hList, pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE_SELECT, L"Toggle Selection");
            AppendMenuW(hMenu, MF_STRING, IDM_SELECT_ALL, L"Select All");
            AppendMenuW(hMenu, MF_STRING, IDM_DESELECT_ALL, L"Deselect All");
            if (contextIndex != -1) {
                char buffer[260];
                if (SendMessageA(hList, LB_GETTEXT, contextIndex, (LPARAM)buffer) > 0) {
                    std::string itemText(buffer);
                    size_t parenPos = itemText.find_last_of('(');
                    if (parenPos != std::string::npos) {
                        std::string itemPath = itemText.substr(0, parenPos - 1);
                        std::filesystem::path filePath = editor->GetTempDir() / itemPath;
                        std::string ext = filePath.extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" || ext == ".bmp") {
                            AppendMenuW(hMenu, MF_STRING, ID_PREVIEW_IMAGE, L"Preview Image");
                        } else if (ext == ".mp3" || ext == ".wav" || ext == ".ogg" || ext == ".wma") {
                            AppendMenuW(hMenu, MF_STRING, ID_PREVIEW_AUDIO, L"Preview Audio");
                        }
                    }
                }
            }
            HMENU hSortMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSortMenu, L"Sort by");
            AppendMenuW(hSortMenu, MF_STRING, IDM_SORT_NAME, L"Name");
            AppendMenuW(hSortMenu, MF_STRING, IDM_SORT_SIZE, L"Size");
            AppendMenuW(hSortMenu, MF_STRING, IDM_SORT_TYPE, L"Type");
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, xPos, yPos, 0, hwnd, nullptr);
            DestroyMenu(hMenu);
            return 0;
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        if (GetFocus() == hList) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hList, &pt);
            int index = GetListBoxItemUnderMouse(hList, pt);
            if (index != -1) {
                if (GetKeyState(VK_CONTROL) & 0x8000) {
                    BOOL selected = SendMessage(hList, LB_GETSEL, index, 0);
                    SendMessage(hList, LB_SETSEL, !selected, index);
                } else {
                    SendMessage(hList, LB_SETSEL, FALSE, -1);
                    SendMessage(hList, LB_SETSEL, TRUE, index);
                }
                return 0;
            }
        }
        break;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        POINT pt;
        DragQueryPoint(hDrop, &pt);
        ClientToScreen(hList, &pt);

        if (!editor->IsBinLoaded()) {
            MessageBoxW(hwnd, L"Please create or open a .bin file first.", L"Error", MB_OK | MB_ICONERROR);
            DragFinish(hDrop);
            return 0;
        }

        std::string folder = "default";
        int itemIndex = GetListBoxItemUnderMouse(hList, pt);
        if (itemIndex != -1) {
            char buffer[260];
            int len = SendMessageA(hList, LB_GETTEXT, itemIndex, (LPARAM)buffer);
            if (len > 0 && len < 260) {
                std::string itemText(buffer);
                if (itemText.empty()) {
                    folder = "default";
                } else {
                    size_t parenPos = itemText.find_last_of('(');
                    if (parenPos != std::string::npos && parenPos > 0) {
                        std::string itemPath = itemText.substr(0, parenPos - 1);
                        std::filesystem::path relPath(itemPath);
                        folder = relPath.parent_path().string();
                        std::replace(folder.begin(), folder.end(), '\\', '/');
                        if (folder.empty()) folder = "default";
                    } else {
                        folder = "default";
                    }
                }
            } else {
                folder = "default";
            }
        } else {
        }

        ProgressDialogData progData = { false };
        HWND hProgress = CreateProgressDialog(hwnd, L"Processing dropped files...", &progData);
        if (!hProgress) {
            MessageBoxW(hwnd, L"Failed to create progress dialog", L"Error", MB_OK | MB_ICONERROR);
            DragFinish(hDrop);
            return 0;
        }

        uint64_t totalBytes = 0;
        std::vector<std::tuple<std::filesystem::path, std::filesystem::path, std::string>> paths;
        wchar_t szPath[MAX_PATH];
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        constexpr int MAX_DEPTH = 100;

        for (UINT i = 0; i < fileCount; ++i) {
            if (!DragQueryFileW(hDrop, i, szPath, MAX_PATH)) {
                continue;
            }

            std::wstring wpath = szPath;
            if (wpath.empty()) {
                continue;
            }

            std::filesystem::path inputPath(wpath);
            if (!std::filesystem::exists(inputPath)) {
                continue;
            }

            if (BinEditorUtils::IsRestrictedFile(inputPath)) {
                continue;
            }

            try {
                if (std::filesystem::is_directory(inputPath)) {
                    std::string droppedName = inputPath.filename().string();
                    bool hasFiles = false;
                    int depth = 0;
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(inputPath, std::filesystem::directory_options::skip_permission_denied)) {
                        if (depth > MAX_DEPTH) {
                            break;
                        }
                        if (entry.is_directory()) {
                            ++depth;
                            continue;
                        }
                        if (!entry.is_regular_file()) {
                            continue;
                        }
                        if (BinEditorUtils::IsRestrictedFile(entry.path())) {
                            continue;
                        }
                        auto size = std::filesystem::file_size(entry.path());
                        if (size > BinEditor::MAX_FILE_SIZE) {
                            continue;
                        }
                        totalBytes += size;
                        paths.emplace_back(entry.path(), inputPath, droppedName);
                        hasFiles = true;
                    }
                    if (!hasFiles) {
                    }
                } else {
                    if (!std::filesystem::is_regular_file(inputPath)) {
                        continue;
                    }
                    auto size = std::filesystem::file_size(inputPath);
                    if (size > BinEditor::MAX_FILE_SIZE) {
                        continue;
                    }
                    totalBytes += size;
                    paths.emplace_back(inputPath, inputPath.parent_path(), "");
                }
            } catch (const std::exception&) {
                continue;
            }
        }

        int added = 0, skipped = 0, failed = 0;
        uint64_t processedBytes = 0;

        for (const auto& pathTuple : paths) {
            std::filesystem::path filePath = std::get<0>(pathTuple);
            std::filesystem::path baseFolder = std::get<1>(pathTuple);
            std::string droppedName = std::get<2>(pathTuple);
            if (progData.cancelled) {
                break;
            }

            try {
                std::wstring wpath = filePath.wstring();
                std::string relativePath = std::filesystem::relative(filePath, baseFolder).string();
                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                std::string effectiveFolder = folder == "default" ? "" : folder;
                if (!droppedName.empty()) {
                    if (!effectiveFolder.empty()) effectiveFolder += "/";
                    effectiveFolder += droppedName;
                }
                std::string targetPath = effectiveFolder;
                if (!targetPath.empty() && !relativePath.empty()) targetPath += "/";
                targetPath += relativePath;
                targetPath = BinEditorUtils::NormalizeName(targetPath);
                std::filesystem::path checkPath = editor->GetTempDir() / targetPath;

                bool isDuplicate = false;
                for (const auto& entry : editor->GetEntries()) {
                    if (entry.name == targetPath) {
                        isDuplicate = true;
                        ++skipped;
                        break;
                    }
                }
                if (isDuplicate) continue;

                std::ifstream in(filePath, std::ios::binary | std::ios::ate);
                if (!in) {
                    ++failed;
                    continue;
                }

                auto size = in.tellg();
                if (static_cast<uint64_t>(size) > BinEditor::MAX_FILE_SIZE) {
                    ++skipped;
                    continue;
                }

                in.seekg(0);
                std::vector<char> buf(static_cast<size_t>(size));
                in.read(buf.data(), size);
                if (!in) {
                    in.close();
                    ++failed;
                    continue;
                }
                in.close();

                BinEntry binEntry;
                binEntry.name = targetPath;
                binEntry.size = static_cast<uint64_t>(size);
                binEntry.dataOffset = 0;
                binEntry.data = std::move(buf);
                auto last_write = std::filesystem::last_write_time(filePath);
                binEntry.last_modified = static_cast<uint64_t>(BinEditorUtils::to_time_t(last_write));

                std::filesystem::path outPath = editor->GetTempDir() / binEntry.name;
                std::filesystem::create_directories(outPath.parent_path());
                std::ofstream fout(outPath, std::ios::binary);
                if (!fout) {
                    ++failed;
                    continue;
                }
                fout.write(binEntry.data.data(), binEntry.data.size());
                fout.close();
                if (!std::filesystem::exists(outPath)) {
                    ++failed;
                    continue;
                }

                editor->GetEntriesMutable().push_back(binEntry);
                ++added;
                processedBytes += size;
                UpdateProgressDialog(hProgress, static_cast<int>((processedBytes * 100) / totalBytes));
            } catch (const std::exception&) {
                ++failed;
            }
        }

        UpdateListBox(hList, *editor);
        CloseProgressDialog(hProgress);
        DragFinish(hDrop);

        std::wstringstream ss;
        ss << L"Added: " << added << L" files\nSkipped: " << skipped << L" files\nFailed: " << failed << L" files";
        MessageBoxW(hwnd, ss.str().c_str(), L"Drop Result", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_BUTTON_OPEN: {
            auto paths = OpenFileDialog(hwnd, L"BIN Files\0*.bin\0", false);
            if (!paths.empty()) {
                // LoadBinStructure is inlined below (unchanged from original main.cpp)
                editor->CleanupCurrentTemp();
                editor->GetEntriesMutable().clear();
                editor->SetBinPath(BinEditorUtils::ToString(paths[0]));
                std::string path = editor->GetBinPath();
                std::ifstream in(path, std::ios::binary);
                if (!in) {
                    MessageBoxW(hwnd, L"Error opening .bin file", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                std::filesystem::path binPath(path);
                std::filesystem::path metadataPath = binPath;
                metadataPath.replace_extension(L".h");
                if (!std::filesystem::exists(metadataPath)) {
                    std::filesystem::path extraPath = binPath.parent_path() / (binPath.stem().wstring() + L"-extra.h");
                    if (std::filesystem::exists(extraPath)) {
                        metadataPath = extraPath;
                    }
                }
                std::vector<ParsedMetadata> metadata = ParseMetadataFile(metadataPath.string(), hwnd);
                if (metadata.empty()) {
                    in.close();
                    MessageBoxW(hwnd, (std::wstring(L"Metadata file not found or invalid: ") + metadataPath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                try {
                    // Defensive sanity-check: ensure GetTempDir() looks valid before creating directories
                    std::wstring tempDirW = editor->GetTempDir().wstring();
                    if (tempDirW.empty() || tempDirW.find(L'\0') != std::wstring::npos) {
                        MessageBoxW(hwnd, (std::wstring(L"Invalid temp directory path: ") + tempDirW).c_str(), L"Error", MB_OK | MB_ICONERROR);
                        in.close();
                        break;
                    }

                    if (std::filesystem::exists(editor->GetTempDir())) {
                        std::filesystem::remove_all(editor->GetTempDir());
                    }
                    std::filesystem::create_directories(editor->GetTempDir());
                } catch (const std::exception& e) {
                    MessageBoxW(hwnd, (std::wstring(L"Error creating temp directory: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    in.close();
                    break;
                }

                ProgressDialogData progData = { false };
                HWND hProg = CreateProgressDialog(hwnd, L"Reading and extracting...", &progData);
                if (!hProg) {
                    in.close();
                    break;
                }

                uint64_t totalSize = std::filesystem::file_size(path);
                uint64_t totalBytes = 0;
                for (const auto& meta : metadata) {
                    totalBytes += meta.size;
                }

                uint64_t processedBytes = 0;
                for (size_t i = 0; i < metadata.size() && !progData.cancelled; ++i) {
                    const auto& meta = metadata[i];
                    if (BinEditorUtils::IsRestrictedFile(std::filesystem::path(meta.name))) {
                        continue;
                    }
                    BinEntry entry;
                    entry.name = BinEditorUtils::NormalizeName(meta.name);
                    entry.size = meta.size;
                    entry.dataOffset = meta.offset;
                    entry.last_modified = 0; // No original timestamp available when loading from bin
                    if (meta.offset + meta.size > totalSize) {
                        MessageBoxW(hwnd, (std::wstring(L"Asset ") + BinEditorUtils::ToWString(meta.name) + L" exceeds file size").c_str(), L"Error", MB_OK | MB_ICONERROR);
                        CloseProgressDialog(hProg);
                        in.close();
                        break;
                    }
                    std::vector<char> buffer(meta.size);
                    in.seekg(meta.offset);
                    in.read(buffer.data(), meta.size);
                    if (!in) {
                        MessageBoxW(hwnd, (std::wstring(L"Error reading data for ") + BinEditorUtils::ToWString(meta.name)).c_str(), L"Error", MB_OK | MB_ICONERROR);
                        CloseProgressDialog(hProg);
                        in.close();
                        break;
                    }
                    std::filesystem::path outPath = editor->GetTempDir() / entry.name;
                    try {
                        std::filesystem::create_directories(outPath.parent_path());
                        std::ofstream fout(outPath.string(), std::ios::binary);
                        if (!fout) {
                            MessageBoxW(hwnd, (std::wstring(L"Error writing to ") + outPath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                            CloseProgressDialog(hProg);
                            in.close();
                            break;
                        }
                        fout.write(buffer.data(), meta.size);
                        fout.close();
                    } catch (const std::exception& e) {
                        MessageBoxW(hwnd, (std::wstring(L"Error writing to temp: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
                        CloseProgressDialog(hProg);
                        in.close();
                        break;
                    }
                    editor->GetEntriesMutable().push_back(entry);
                    processedBytes += meta.size;
                    UpdateProgressDialog(hProg, static_cast<int>((processedBytes * 100) / totalBytes));
                }
                std::sort(editor->GetEntriesMutable().begin(), editor->GetEntriesMutable().end(),
                    [](const BinEntry& a, const BinEntry& b){ return a.name < b.name; });
                CloseProgressDialog(hProg);
                UpdateListBox(hList, *editor);
                in.close();
            }
            return 0;
        }
        case ID_BUTTON_ADD: {
            if (!editor->IsBinLoaded()) {
                MessageBoxW(hwnd, L"Please create or open a .bin file first.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }
            std::string folder = "";
            int selCount = (int)SendMessage(hList, LB_GETSELCOUNT, 0, 0);
            if (selCount == 1) {
                int selIndex;
                SendMessage(hList, LB_GETSELITEMS, 1, (LPARAM)&selIndex);
                char buffer[260];
                if (SendMessageA(hList, LB_GETTEXT, selIndex, (LPARAM)buffer) > 0) {
                    std::string itemText(buffer);
                    size_t parenPos = itemText.find_last_of('(');
                    if (parenPos != std::string::npos) {
                        std::string itemPath = itemText.substr(0, parenPos - 1);
                        std::filesystem::path relPath(itemPath);
                        folder = relPath.parent_path().string();
                        std::replace(folder.begin(), folder.end(), '\\', '/');
                    }
                }
            }
            auto selectedPaths = OpenFileDialog(hwnd, L"All Files\0*.*\0", true);
            if (!selectedPaths.empty()) {
                AddResult result;
                for (const auto& wpath : selectedPaths) {
                    result += AddFileToBin(hwnd, hList, wpath, *editor, folder);
                }
                UpdateListBox(hList, *editor);
                std::wstringstream ss;
                ss << L"Added: " << result.added << L" files\nSkipped: " << result.skipped << L" files\nFailed: " << result.failed << L" files";
                MessageBoxW(hwnd, ss.str().c_str(), L"Add Result", MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }
        case ID_BUTTON_REMOVE: {
            if (!editor->IsBinLoaded()) {
                MessageBoxW(hwnd, L"Please create or open a .bin file first.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }
            std::vector<int> selectedIndices;
            int selCount = (int)SendMessage(hList, LB_GETSELCOUNT, 0, 0);
            if (selCount > 0) {
                selectedIndices.resize(selCount);
                SendMessage(hList, LB_GETSELITEMS, selCount, (LPARAM)selectedIndices.data());
                if (MessageBoxW(hwnd, L"Are you sure you want to remove the selected files?", L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    std::vector<BinEntry>& entries = editor->GetEntriesMutable();
                    for (int i = selCount - 1; i >= 0; --i) {
                        int index = selectedIndices[i];
                        std::filesystem::path filePath = editor->GetTempDir() / entries[index].name;
                        try {
                            if (std::filesystem::exists(filePath)) {
                                std::filesystem::remove(filePath);
                            }
                            entries.erase(entries.begin() + index);
                        } catch (const std::exception&) {
                        }
                    }
                    UpdateListBox(hList, *editor);
                }
            } else {
                MessageBoxW(hwnd, L"No files selected to remove.", L"Error", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        case ID_BUTTON_REPLACE: {
            if (!editor->IsBinLoaded()) {
                MessageBoxW(hwnd, L"Please create or open a .bin file first.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }
            int selCount = (int)SendMessage(hList, LB_GETSELCOUNT, 0, 0);
            if (selCount != 1) {
                MessageBoxW(hwnd, L"Please select exactly one file to replace.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }
            int selectedIndex;
            SendMessage(hList, LB_GETSELITEMS, 1, (LPARAM)&selectedIndex);
            auto paths = OpenFileDialog(hwnd, L"All Files\0*.*\0", false);
            if (!paths.empty()) {
                std::wstring wpath = paths[0];
                std::filesystem::path newFilePath(wpath);
                if (BinEditorUtils::IsRestrictedFile(newFilePath)) {
                    MessageBoxW(hwnd, L"Cannot replace with restricted file (no .bin, .h, or metadata.h allowed).", L"Error", MB_OK | MB_ICONERROR);
                    return 0;
                }
                auto size = std::filesystem::file_size(newFilePath);
                if (size > BinEditor::MAX_FILE_SIZE) {
                    MessageBoxW(hwnd, L"Replacement file is too large.", L"Error", MB_OK | MB_ICONERROR);
                    return 0;
                }
                std::string spath = BinEditorUtils::ToString(wpath);
                std::ifstream in(spath, std::ios::binary | std::ios::ate);
                if (!in) {
                    MessageBoxW(hwnd, (std::wstring(L"Failed to open replacement file: ") + wpath).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    return 0;
                }
                in.seekg(0);
                std::vector<char> buf(static_cast<size_t>(size));
                in.read(buf.data(), size);
                in.close();
                BinEntry& entry = editor->GetEntriesMutable()[selectedIndex];
                std::filesystem::path oldFilePath = editor->GetTempDir() / entry.name;

                std::string newFileName = newFilePath.filename().string();
                std::filesystem::path oldNamePath(entry.name);
                std::string parent = oldNamePath.parent_path().string();
                std::replace(parent.begin(), parent.end(), '\\', '/');
                std::string newName = parent.empty() ? newFileName : parent + "/" + newFileName;
                newName = BinEditorUtils::NormalizeName(newName);

                bool isDuplicate = false;
                for (const auto& e : editor->GetEntries()) {
                    if (&e != &entry && e.name == newName) {
                        isDuplicate = true;
                        break;
                    }
                }
                if (isDuplicate) {
                    MessageBoxW(hwnd, (std::wstring(L"File with name ") + BinEditorUtils::ToWString(newFileName) + L" already exists in the same folder.").c_str(), L"Error", MB_OK | MB_ICONERROR);
                    return 0;
                }

                std::filesystem::path newOutPath = editor->GetTempDir() / newName;

                try {
                    std::filesystem::create_directories(newOutPath.parent_path());
                    std::filesystem::remove(oldFilePath);
                    std::ofstream fout(newOutPath, std::ios::binary);
                    if (!fout) {
                        MessageBoxW(hwnd, (std::wstring(L"Failed to write to: ") + newOutPath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                        return 0;
                    }
                    fout.write(buf.data(), size);
                    fout.close();
                    if (!std::filesystem::exists(newOutPath)) {
                        MessageBoxW(hwnd, (std::wstring(L"File not found after writing: ") + newOutPath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                        return 0;
                    }
                    entry.name = newName;
                    entry.data = std::move(buf);
                    entry.size = size;
                    auto last_write = std::filesystem::last_write_time(newFilePath);
                    entry.last_modified = static_cast<uint64_t>(BinEditorUtils::to_time_t(last_write));
                    UpdateListBox(hList, *editor);
                    MessageBoxW(hwnd, L"File replaced successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
                } catch (const std::exception& e) {
                    MessageBoxW(hwnd, (std::wstring(L"Filesystem error: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
                }
            }
            return 0;
        }
        case ID_BUTTON_EXPORT: {
            int selCount = (int)SendMessage(hList, LB_GETSELCOUNT, 0, 0);
            if (selCount == 0) {
                MessageBoxW(hwnd, L"No items selected to export", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }

            std::wstring destFolder = BrowseForFolder(hwnd, L"Select destination folder for export", editor->GetTempDir());
            if (destFolder.empty()) {
                MessageBoxW(hwnd, L"No destination folder selected", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }

            std::vector<int> selIndices(selCount);
            SendMessage(hList, LB_GETSELITEMS, selCount, (LPARAM)selIndices.data());

            ProgressDialogData progData = { false };
            HWND hProgress = CreateProgressDialog(hwnd, L"Exporting files...", &progData);
            if (!hProgress) {
                return 0;
            }

            uint64_t totalBytes = 0;
            for (int index : selIndices) {
                char buffer[260];
                SendMessageA(hList, LB_GETTEXT, index, (LPARAM)buffer);
                std::string itemText(buffer);
                size_t parenPos = itemText.find_last_of('(');
                if (parenPos == std::string::npos) continue;
                std::string itemPath = itemText.substr(0, parenPos - 1);
                auto it = std::find_if(editor->GetEntries().begin(), editor->GetEntries().end(),
                    [&](const BinEntry& e) { return e.name == itemPath; });
                if (it != editor->GetEntries().end()) {
                    totalBytes += it->size;
                }
            }

            uint64_t processedBytes = 0;
            int exported = 0, skipped = 0, failed = 0;
            for (int index : selIndices) {
                if (progData.cancelled) {
                    break;
                }
                char buffer[260];
                SendMessageA(hList, LB_GETTEXT, index, (LPARAM)buffer);
                std::string itemText(buffer);
                size_t parenPos = itemText.find_last_of('(');
                if (parenPos == std::string::npos) {
                    ++skipped;
                    continue;
                }
                std::string itemPath = itemText.substr(0, parenPos - 1);
                std::filesystem::path srcPath = editor->GetTempDir() / itemPath;
                std::filesystem::path destPath = std::filesystem::path(destFolder) / std::filesystem::path(itemPath).filename();

                try {
                    if (std::filesystem::exists(srcPath)) {
                        if (std::filesystem::exists(destPath)) {
                            std::wstring msg = L"File already exists: " + destPath.wstring() + L"\nOverwrite?";
                            if (MessageBoxW(hwnd, msg.c_str(), L"Confirm Overwrite", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                                ++skipped;
                                continue;
                            }
                        }
                        std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing);
                        ++exported;

                        auto it = std::find_if(editor->GetEntries().begin(), editor->GetEntries().end(),
                            [&](const BinEntry& e) { return e.name == itemPath; });
                        if (it != editor->GetEntries().end()) {
                            processedBytes += it->size;
                        }
                    } else {
                        ++failed;
                    }
                } catch (const std::exception&) {
                    ++failed;
                }
                UpdateProgressDialog(hProgress, static_cast<int>((processedBytes * 100) / totalBytes));
            }
            CloseProgressDialog(hProgress);
            std::wstring msg = L"Exported: " + std::to_wstring(exported) +
                              L", Skipped: " + std::to_wstring(skipped) +
                              L", Failed: " + std::to_wstring(failed);
            MessageBoxW(hwnd, msg.c_str(), L"Export Result", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case ID_BUTTON_SAVE: {
            if (!editor->IsBinLoaded()) {
                MessageBoxW(hwnd, L"Please create or open a .bin file first.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }
            // SaveBinToFile inlined here (unchanged from original main.cpp)
            {
                wchar_t szFile[MAX_PATH] = {};
                OPENFILENAMEW ofn = { sizeof(ofn) };
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = L"BIN Files\0*.bin\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrDefExt = L"bin";
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                ofn.lpstrTitle = L"Save BIN File As";
                if (!GetSaveFileNameW(&ofn)) {
                    break;
                }

                std::wstring wFilePath = szFile;
                if (wFilePath.length() < 4 || wFilePath.substr(wFilePath.length() - 4) != L".bin") {
                    wFilePath += L".bin";
                }

                ProgressDialogData progData = { false };
                HWND hProgress = CreateProgressDialog(hwnd, L"Saving...", &progData);
                if (!hProgress) {
                    break;
                }

                std::ifstream inOriginal;
                if (!editor->GetBinPath().empty()) {
                    inOriginal.open(editor->GetBinPath(), std::ios::binary);
                }
                std::ofstream out(BinEditorUtils::ToString(wFilePath), std::ios::binary);
                if (!out) {
                    MessageBoxW(hwnd, (std::wstring(L"Error creating .bin file: ") + wFilePath).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    CloseProgressDialog(hProgress);
                    break;
                }

                uint64_t currentOffset = 0;
                uint64_t totalBytes = 0;
                for (const auto& entry : editor->GetEntries()) {
                    if (!BinEditorUtils::IsRestrictedFile(std::filesystem::path(entry.name))) {
                        totalBytes += entry.size;
                    }
                }
                uint64_t processedBytes = 0;

                for (size_t i = 0; i < editor->GetEntries().size() && !progData.cancelled; ++i) {
                    BinEntry& entry = editor->GetEntriesMutable()[i];
                    entry.name = BinEditorUtils::NormalizeName(entry.name);
                    if (BinEditorUtils::IsRestrictedFile(std::filesystem::path(entry.name))) {
                        continue;
                    }
                    if (!entry.data.empty()) {
                        out.write(entry.data.data(), static_cast<std::streamsize>(entry.data.size()));
                    } else if (inOriginal) {
                        inOriginal.seekg(entry.dataOffset);
                        std::vector<char> buffer(entry.size);
                        inOriginal.read(buffer.data(), static_cast<std::streamsize>(entry.size));
                        out.write(buffer.data(), static_cast<std::streamsize>(entry.size));
                    }
                    entry.dataOffset = currentOffset;
                    currentOffset += entry.size;
                    processedBytes += entry.size;
                    UpdateProgressDialog(hProgress, static_cast<int>((processedBytes * 100) / totalBytes));
                }
                out.close();
                CloseProgressDialog(hProgress);

                if (progData.cancelled) {
                    std::filesystem::remove(wFilePath);
                    break;
                }

                std::filesystem::path headerPath = std::filesystem::path(wFilePath);
                headerPath.replace_extension(L".h");
                if (std::filesystem::exists(headerPath)) {
                    if (MessageBoxW(hwnd, L"Metadata file already exists. Overwrite?", L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                        break;
                    }
                }
                std::ofstream header(headerPath.string());
                if (header) {
                    header << "#pragma once\n#include <cstdint>\n";
                    header << "struct AssetMetadata {\n"
                           << "    const char* name;\n"
                           << "    uint64_t offset;\n"
                           << "    uint64_t size;\n"
                           << "};\n\n";
                    header << "static const AssetMetadata assets[] = {\n";
                    for (const auto& entry : editor->GetEntries()) {
                        if (BinEditorUtils::IsRestrictedFile(std::filesystem::path(entry.name))) {
                            continue;
                        }
                        std::string escapedName = entry.name;
                        escapedName = std::regex_replace(escapedName, std::regex("\\\\"), "\\\\");
                        escapedName = std::regex_replace(escapedName, std::regex("\""), "\\\"");
                        header << "    { \"" << escapedName << "\", " << entry.dataOffset << ", " << entry.size << " },\n";
                    }
                    header << "};\n";
                    header.close();
                } else {
                    MessageBoxW(hwnd, (std::wstring(L"Error creating metadata file: ") + headerPath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    std::filesystem::remove(wFilePath);
                    break;
                }

                // Create extra metadata file
                std::filesystem::path extraHeaderPath = headerPath.parent_path() / (headerPath.stem().string() + "-extra.h");
                std::ofstream extraHeader(extraHeaderPath.string());
                if (extraHeader) {
                    extraHeader << "#pragma once\n#include <cstdint>\n";
                    extraHeader << "struct AssetMetadataExtra {\n"
                                << "    const char* name;\n"
                                << "    uint64_t offset;\n"
                                << "    uint64_t size;\n"
                                << "    const char* ext;\n"
                                << "    const char* mime_type;\n"
                                << "    uint64_t last_modified;\n"
                                << "};\n\n";
                    extraHeader << "static const AssetMetadataExtra assets_extra[] = {\n";
                    for (const auto& entry : editor->GetEntries()) {
                        if (BinEditorUtils::IsRestrictedFile(std::filesystem::path(entry.name))) {
                            continue;
                        }
                        std::string escapedName = entry.name;
                        escapedName = std::regex_replace(escapedName, std::regex("\\\\"), "\\\\");
                        escapedName = std::regex_replace(escapedName, std::regex("\""), "\\\"");
                        std::string ext = BinEditorUtils::getExt(entry.name);
                        std::string mime = BinEditorUtils::getMime(ext);
                        extraHeader << "    { \"" << escapedName << "\", " << entry.dataOffset << ", " << entry.size << ", \"" << ext << "\", \"" << mime << "\", " << entry.last_modified << " },\n";
                    }
                    extraHeader << "};\n";
                    extraHeader.close();
                } else {
                }

                auto oldTempDir = editor->GetTempDir();
                editor->SetBinPath(BinEditorUtils::ToString(wFilePath));
                auto newTempDir = editor->GetTempDir();
                if (oldTempDir != newTempDir && !oldTempDir.empty()) {
                    try {
                        // Defensive sanity-check for newTempDir
                        std::wstring newTempW = newTempDir.wstring();
                        if (newTempW.empty() || newTempW.find(L'\0') != std::wstring::npos) {
                            MessageBoxW(hwnd, (std::wstring(L"Invalid temp directory path for migration: ") + newTempW).c_str(), L"Error", MB_OK | MB_ICONERROR);
                            // Skip migration to avoid filesystem errors
                        } else {
                            if (std::filesystem::exists(newTempDir)) {
                                std::filesystem::remove_all(newTempDir);
                            }
                            std::filesystem::create_directories(newTempDir);
                            for (const auto& entry : std::filesystem::recursive_directory_iterator(oldTempDir, std::filesystem::directory_options::skip_permission_denied)) {
                                std::filesystem::path relative = std::filesystem::relative(entry.path(), oldTempDir);
                                std::filesystem::path dest = newTempDir / relative;
                                if (entry.is_directory()) {
                                    std::filesystem::create_directories(dest);
                                } else {
                                    std::filesystem::copy_file(entry.path(), dest, std::filesystem::copy_options::overwrite_existing);
                                }
                            }
                            std::filesystem::remove_all(oldTempDir);
                            editor->RemoveTempDir(oldTempDir);
                        }
                    } catch (const std::exception& e) {
                        MessageBoxW(hwnd, (std::wstring(L"Error migrating temp files: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    }
                }

                MessageBoxW(hwnd, (std::wstring(L"BIN file and metadata saved successfully to: ") + wFilePath).c_str(), L"Success", MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }
        case ID_BUTTON_CREATE_BIN: {
            // CreateNewBin inlined here (unchanged from original main.cpp)
            {
                std::wstring selectedFolder = BrowseForFolder(hwnd, L"Select folder containing files and subfolders for new BIN file", L"");
                if (selectedFolder.empty()) {
                    MessageBoxW(hwnd, L"No folder selected. Creation cancelled.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                wchar_t szFile[MAX_PATH] = L"newfile.bin";
                OPENFILENAMEW ofn = { sizeof(ofn) };
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = L"BIN Files\0*.bin\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrDefExt = L"bin";
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                ofn.lpstrTitle = L"Enter name for new BIN file";
                if (!GetSaveFileNameW(&ofn)) {
                    MessageBoxW(hwnd, L"No file name selected. Creation cancelled.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                std::wstring wFilePath = szFile;
                if (wFilePath.length() < 4 || wFilePath.substr(wFilePath.length() - 4) != L".bin") {
                    wFilePath += L".bin";
                }

                std::filesystem::path folderPath(selectedFolder);
                std::filesystem::path binPath(wFilePath);
                std::string binPathStr = BinEditorUtils::ToString(wFilePath);

                bool hasFiles = false;
                try {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath, std::filesystem::directory_options::skip_permission_denied)) {
                        if (entry.is_regular_file() && !BinEditorUtils::IsRestrictedFile(entry.path())) {
                            hasFiles = true;
                            break;
                        }
                    }
                } catch (const std::exception& e) {
                    MessageBoxW(hwnd, (std::wstring(L"Error scanning folder: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    break;
                }
                if (!hasFiles) {
                    MessageBoxW(hwnd, L"Selected folder contains no eligible files (no .bin, .h, or metadata.h allowed). Creation cancelled.", L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                editor->CleanupCurrentTemp();
                editor->GetEntriesMutable().clear();
                editor->SetBinPath(binPathStr);

                try {
                    // Defensive sanity-check: ensure GetTempDir() looks valid before creating directories
                    std::wstring tempDirW = editor->GetTempDir().wstring();
                    if (tempDirW.empty() || tempDirW.find(L'\0') != std::wstring::npos) {
                        MessageBoxW(hwnd, (std::wstring(L"Invalid temp directory path: ") + tempDirW).c_str(), L"Error", MB_OK | MB_ICONERROR);
                        break;
                    }

                    if (std::filesystem::exists(editor->GetTempDir())) {
                        std::filesystem::remove_all(editor->GetTempDir());
                    }
                    std::filesystem::create_directories(editor->GetTempDir());
                    if (!std::filesystem::exists(editor->GetTempDir())) {
                        MessageBoxW(hwnd, (std::wstring(L"Temp directory not found after creation: ") + editor->GetTempDir().wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                } catch (const std::exception& e) {
                    MessageBoxW(hwnd, (std::wstring(L"Error creating temp directory: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                std::ofstream out(binPathStr, std::ios::binary);
                if (!out) {
                    MessageBoxW(hwnd, (std::wstring(L"Error creating .bin file: ") + binPath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    break;
                }

                ProgressDialogData progData = { false };
                HWND hProgress = CreateProgressDialog(hwnd, L"Creating BIN file...", &progData);
                if (!hProgress) {
                    out.close();
                    break;
                }

                uint64_t totalBytes = 0;
                std::vector<std::filesystem::path> files;
                try {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath, std::filesystem::directory_options::skip_permission_denied)) {
                        if (entry.is_regular_file() && !BinEditorUtils::IsRestrictedFile(entry.path())) {
                            auto size = std::filesystem::file_size(entry.path());
                            if (size <= BinEditor::MAX_FILE_SIZE) {
                                totalBytes += size;
                                files.push_back(entry.path());
                            }
                        } else if (entry.is_regular_file()) {
                        }
                    }
                } catch (const std::exception& e) {
                    MessageBoxW(hwnd, (std::wstring(L"Error scanning folder: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    CloseProgressDialog(hProgress);
                    out.close();
                    break;
                }

                uint64_t currentOffset = 0;
                uint64_t processedBytes = 0;
                for (const auto& filePath : files) {
                    if (progData.cancelled) {
                        CloseProgressDialog(hProgress);
                        out.close();
                        std::filesystem::remove(binPath);
                        editor->GetEntriesMutable().clear();
                        UpdateListBox(hList, *editor);
                        break;
                    }

                    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
                    if (!in) {
                        continue;
                    }

                    auto size = in.tellg();
                    if (static_cast<uint64_t>(size) > BinEditor::MAX_FILE_SIZE) {
                        in.close();
                        continue;
                    }

                    in.seekg(0);
                    std::vector<char> buf(static_cast<size_t>(size));
                    in.read(buf.data(), size);
                    in.close();

                    std::string relativeName = std::filesystem::relative(filePath, folderPath).string();
                    std::replace(relativeName.begin(), relativeName.end(), '\\', '/');
                    relativeName = BinEditorUtils::NormalizeName(relativeName);

                    BinEntry binEntry;
                    binEntry.name = relativeName;
                    binEntry.size = static_cast<uint64_t>(size);
                    binEntry.dataOffset = currentOffset;
                    binEntry.data = std::move(buf);
                    auto last_write = std::filesystem::last_write_time(filePath);
                    binEntry.last_modified = static_cast<uint64_t>(BinEditorUtils::to_time_t(last_write));

                    std::filesystem::path outPath = editor->GetTempDir() / binEntry.name;
                    std::filesystem::create_directories(outPath.parent_path());
                    std::ofstream fout(outPath, std::ios::binary);
                    if (!fout) {
                        continue;
                    }
                    fout.write(binEntry.data.data(), binEntry.data.size());
                    fout.close();
                    if (!std::filesystem::exists(outPath)) {
                        continue;
                    }

                    out.write(binEntry.data.data(), binEntry.data.size());
                    currentOffset += binEntry.size;
                    processedBytes += binEntry.size;
                    UpdateProgressDialog(hProgress, static_cast<int>((processedBytes * 100) / totalBytes));

                    editor->GetEntriesMutable().push_back(binEntry);
                }
                out.close();
                CloseProgressDialog(hProgress);

                if (progData.cancelled) {
                    std::filesystem::remove(binPath);
                    editor->GetEntriesMutable().clear();
                    UpdateListBox(hList, *editor);
                    break;
                }

                std::filesystem::path headerPath = binPath;
                headerPath.replace_extension(L".h");
                std::ofstream header(headerPath.string());
                if (header) {
                    header << "#pragma once\n#include <cstdint>\n";
                    header << "struct AssetMetadata {\n"
                           << "    const char* name;\n"
                           << "    uint64_t offset;\n"
                           << "    uint64_t size;\n"
                           << "};\n\n";
                    header << "static const AssetMetadata assets[] = {\n";
                    for (const auto& entry : editor->GetEntries()) {
                        std::string escapedName = entry.name;
                        escapedName = std::regex_replace(escapedName, std::regex("\\\\"), "\\\\");
                        escapedName = std::regex_replace(escapedName, std::regex("\""), "\\\"");
                        header << "    { \"" << escapedName << "\", " << entry.dataOffset << ", " << entry.size << " },\n";
                    }
                    header << "};\n";
                    header.close();
                } else {
                    MessageBoxW(hwnd, (std::wstring(L"Error creating metadata file: ") + headerPath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    std::filesystem::remove(binPath);
                    break;
                }

                std::filesystem::path extraHeaderPath = headerPath.parent_path() / (headerPath.stem().string() + "-extra.h");
                std::ofstream extraHeader(extraHeaderPath.string());
                if (extraHeader) {
                    extraHeader << "#pragma once\n#include <cstdint>\n";
                    extraHeader << "struct AssetMetadataExtra {\n"
                                << "    const char* name;\n"
                                << "    uint64_t offset;\n"
                                << "    uint64_t size;\n"
                                << "    const char* ext;\n"
                                << "    const char* mime_type;\n"
                                << "    uint64_t last_modified;\n"
                                << "};\n\n";
                    extraHeader << "static const AssetMetadataExtra assets_extra[] = {\n";
                    for (const auto& entry : editor->GetEntries()) {
                        std::string escapedName = entry.name;
                        escapedName = std::regex_replace(escapedName, std::regex("\\\\"), "\\\\");
                        escapedName = std::regex_replace(escapedName, std::regex("\""), "\\\"");
                        std::string ext = BinEditorUtils::getExt(entry.name);
                        std::string mime = BinEditorUtils::getMime(ext);
                        extraHeader << "    { \"" << escapedName << "\", " << entry.dataOffset << ", " << entry.size << ", \"" << ext << "\", \"" << mime << "\", " << entry.last_modified << " },\n";
                    }
                    extraHeader << "};\n";
                    extraHeader.close();
                }

                std::sort(editor->GetEntriesMutable().begin(), editor->GetEntriesMutable().end(),
                          [](const BinEntry& a, const BinEntry& b) { return a.name < b.name; });
                UpdateListBox(hList, *editor);
                MessageBoxW(hwnd, (std::wstring(L"BIN file created successfully at: ") + wFilePath).c_str(), L"Success", MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }
        case ID_BUTTON_EXIT: {
            SendMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        case ID_BUTTON_PROG_CANCEL: {
            ProgressDialogData* dialogData = (ProgressDialogData*)GetWindowLongPtrW(GetParent((HWND)lParam), GWLP_USERDATA);
            if (dialogData) {
                dialogData->cancelled = true;
            }
            return 0;
        }
        case IDM_TOGGLE_SELECT: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int index = GetListBoxItemUnderMouse(hList, pt);
            if (index != -1) {
                BOOL selected = SendMessage(hList, LB_GETSEL, index, 0);
                SendMessage(hList, LB_SETSEL, !selected, index);
            }
            return 0;
        }
        case IDM_SELECT_ALL: {
            SendMessage(hList, LB_SETSEL, TRUE, -1);
            return 0;
        }
        case IDM_DESELECT_ALL: {
            SendMessage(hList, LB_SETSEL, FALSE, -1);
            return 0;
        }
        case ID_PREVIEW_IMAGE: {
            int index = contextIndex;
            if (index == -1) {
                MessageBoxW(hwnd, L"No item selected for preview.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }
            char buffer[260];
            int textLen = SendMessageA(hList, LB_GETTEXT, index, (LPARAM)buffer);
            if (textLen <= 0) {
                MessageBoxW(hwnd, L"Failed to retrieve item text.", L"Error", MB_OK | MB_ICONERROR);
                contextIndex = -1;
                return 0;
            }
            std::string itemText(buffer);
            size_t parenPos = itemText.find_last_of('(');
            if (parenPos == std::string::npos) {
                MessageBoxW(hwnd, L"Invalid item format.", L"Error", MB_OK | MB_ICONERROR);
                contextIndex = -1;
                return 0;
            }
            std::string itemPath = itemText.substr(0, parenPos - 1);
            std::filesystem::path filePath = editor->GetTempDir() / itemPath;
            if (!std::filesystem::exists(filePath)) {
                MessageBoxW(hwnd, (std::wstring(L"File does not exist: ") + filePath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                contextIndex = -1;
                return 0;
            }
            std::wstring* wImagePath = new std::wstring(filePath.wstring());
            HWND hPreview = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, L"ImagePreviewClass", L"Image Preview",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hwnd, nullptr, GetModuleHandle(nullptr), wImagePath);
            if (!hPreview) {
                delete wImagePath;
                MessageBoxW(hwnd, L"Failed to create preview window.", L"Error", MB_OK | MB_ICONERROR);
                contextIndex = -1;
                return 0;
            }
            ShowWindow(hPreview, SW_SHOW);
            UpdateWindow(hPreview);
            contextIndex = -1;
            return 0;
        }
        case ID_PREVIEW_AUDIO: {
            int index = contextIndex;
            if (index == -1) {
                MessageBoxW(hwnd, L"No item selected for preview.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }
            char buffer[260];
            int textLen = SendMessageA(hList, LB_GETTEXT, index, (LPARAM)buffer);
            if (textLen <= 0) {
                MessageBoxW(hwnd, L"Failed to retrieve item text.", L"Error", MB_OK | MB_ICONERROR);
                contextIndex = -1;
                return 0;
            }
            std::string itemText(buffer);
            size_t parenPos = itemText.find_last_of('(');
            if (parenPos == std::string::npos) {
                MessageBoxW(hwnd, L"Invalid item format.", L"Error", MB_OK | MB_ICONERROR);
                contextIndex = -1;
                return 0;
            }
            std::string itemPath = itemText.substr(0, parenPos - 1);
            std::filesystem::path filePath = editor->GetTempDir() / itemPath;
            if (!std::filesystem::exists(filePath)) {
                MessageBoxW(hwnd, (std::wstring(L"File does not exist: ") + filePath.wstring()).c_str(), L"Error", MB_OK | MB_ICONERROR);
                contextIndex = -1;
                return 0;
            }
            std::wstring* wAudioPath = new std::wstring(filePath.wstring());
            int x = (GetSystemMetrics(SM_CXSCREEN) - 300) / 2;
            int y = (GetSystemMetrics(SM_CYSCREEN) - 100) / 2;
            HWND hAudioPreview = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, L"AudioPreviewClass", L"Audio Preview",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, 300, 100, hwnd, nullptr, GetModuleHandle(nullptr), wAudioPath);
            if (!hAudioPreview) {
                delete wAudioPath;
                MessageBoxW(hwnd, L"Failed to create audio preview window.", L"Error", MB_OK | MB_ICONERROR);
                contextIndex = -1;
                return 0;
            }
            ShowWindow(hAudioPreview, SW_SHOW);
            UpdateWindow(hAudioPreview);
            contextIndex = -1;
            return 0;
        }
        case IDM_SORT_NAME: {
            std::sort(editor->GetEntriesMutable().begin(), editor->GetEntriesMutable().end(),
                [](const BinEntry& a, const BinEntry& b) { return a.name < b.name; });
            UpdateListBox(hList, *editor);
            return 0;
        }
        case IDM_SORT_SIZE: {
            std::sort(editor->GetEntriesMutable().begin(), editor->GetEntriesMutable().end(),
                [](const BinEntry& a, const BinEntry& b) { return a.size < b.size; });
            UpdateListBox(hList, *editor);
            return 0;
        }
        case IDM_SORT_TYPE: {
            std::sort(editor->GetEntriesMutable().begin(), editor->GetEntriesMutable().end(),
                [](const BinEntry& a, const BinEntry& b) {
                    std::string extA = BinEditorUtils::getExt(a.name);
                    std::string extB = BinEditorUtils::getExt(b.name);
                    if (extA == extB) return a.name < b.name;
                    return extA < extB;
                });
            UpdateListBox(hList, *editor);
            return 0;
        }
        }
        break;
    }

    case WM_DESTROY: {
        if (hFontList) DeleteObject(hFontList);
        if (editor) {
            delete editor;
        }
        for (const auto& entry : std::filesystem::directory_iterator(tempBinsPath)) {
            if (entry.is_directory()) {
                try {
                    std::filesystem::remove_all(entry.path());
                } catch (...) {
                }
            }
        }
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}