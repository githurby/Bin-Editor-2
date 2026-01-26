#include "Resource.h"
#include "Dialogs.h"
#include "BinEditorUtils.h"
#include "Resource.h"
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

std::vector<std::wstring> OpenFileDialog(HWND hwnd, const wchar_t* filter, bool multiSelect) {
    wchar_t szFile[MAX_PATH * 100] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH * 100;
    ofn.Flags = OFN_FILEMUSTEXIST | (multiSelect ? OFN_ALLOWMULTISELECT | OFN_EXPLORER : 0);
    if (!GetOpenFileNameW(&ofn)) {
        return {};
    }
    std::vector<std::wstring> paths;
    wchar_t* p = szFile;
    std::wstring current = p;
    if (current.empty()) return {};
    p += current.length() + 1;
    if (*p == 0) {
        paths.push_back(current);
    } else {
        std::wstring dir = current;
        if (!dir.empty() && dir.back() != L'\\') dir += L'\\';
        while (*p) {
            paths.push_back(dir + p);
            p += wcslen(p) + 1;
        }
    }
    return paths;
}

std::wstring BrowseForFolder(HWND hwnd, const std::wstring& title, const std::filesystem::path& rootDir) {
    std::wstring selectedPath;
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hwnd;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX | BIF_VALIDATE;
    bi.lpfn = [](HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) -> int CALLBACK {
        if (uMsg == BFFM_INITIALIZED && lpData != 0) {
            SendMessage(hwnd, BFFM_SETEXPANDED, TRUE, lpData);
            SendMessage(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
        }
        return 0;
    };
    bi.lParam = reinterpret_cast<LPARAM>(rootDir.wstring().c_str());

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != nullptr) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            selectedPath = path;
        }
        CoTaskMemFree(pidl);
    }
    return selectedPath;
}

std::string SelectFolderForAdd(HWND hwnd, const BinEditor& editor) {
    std::filesystem::path rootDir = editor.GetBinPath().empty() ? editor.GetTempDir() : std::filesystem::path(BinEditorUtils::ToWString(editor.GetBinPath())).parent_path();
    std::wstring folder = BrowseForFolder(hwnd, L"Select folder for adding files", rootDir);
    if (folder.empty()) {
        return "";
    }
    std::string folderStr = BinEditorUtils::ToString(folder);
    std::replace(folderStr.begin(), folderStr.end(), '\\', '/');
    std::filesystem::path selectedPath(folder);
    std::filesystem::path binParent = rootDir;

    try {
        if (!std::filesystem::equivalent(selectedPath, binParent) &&
            selectedPath.string().find(binParent.string()) != 0 &&
            selectedPath.string().find(editor.GetTempDir().string()) != 0) {
            MessageBoxW(hwnd, L"Selected folder must be within the bin file's folder or temp folder.", L"Error", MB_OK | MB_ICONERROR);
            return "";
        }
        folderStr = std::filesystem::relative(selectedPath, editor.GetTempDir()).string();
        std::replace(folderStr.begin(), folderStr.end(), '\\', '/');
        if (folderStr == ".") folderStr = "";
        std::filesystem::path fullPath = editor.GetTempDir() / folderStr;
        folderStr = std::filesystem::relative(std::filesystem::canonical(fullPath), editor.GetTempDir()).string();
        std::replace(folderStr.begin(), folderStr.end(), '\\', '/');
    } catch (const std::exception&) {
        MessageBoxW(hwnd, L"Error processing folder path.", L"Error", MB_OK | MB_ICONERROR);
        return "";
    }
    return folderStr;
}

std::string CombinePath(const std::string& folder, const std::wstring& filename, const std::filesystem::path& tempDir, const std::filesystem::path& selectedFolder) {
    std::filesystem::path filePath(filename);
    std::string relativePath;
    if (filePath.is_relative()) {
        relativePath = filePath.string();
    } else {
        relativePath = filePath.filename().string();
    }
    if (!folder.empty() && folder != "default") {
        std::string mutableFolder = folder;
        std::replace(mutableFolder.begin(), mutableFolder.end(), '\\', '/');
        relativePath = mutableFolder + "/" + relativePath;
    }
    std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

    std::filesystem::path fullPath = tempDir / relativePath;
    try {
        std::filesystem::path canonicalPath = std::filesystem::canonical(fullPath);
        relativePath = std::filesystem::relative(canonicalPath, tempDir).string();
        std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
    } catch (const std::exception&) {
    }

    relativePath = BinEditorUtils::NormalizeName(relativePath);

    return relativePath;
}

AddResult AddFileToBin(HWND hwnd, HWND hList, const std::wstring& wpath, BinEditor& editor, const std::string& folder) {
    AddResult result;
    if (!editor.IsBinLoaded()) {
        MessageBoxW(hwnd, L"Please create or open a .bin file first.", L"Error", MB_OK | MB_ICONERROR);
        return result;
    }

    if (wpath.empty()) {
        MessageBoxW(hwnd, L"No file or folder selected.", L"Error", MB_OK | MB_ICONERROR);
        return result;
    }

    try {
        std::string path = BinEditorUtils::ToString(wpath);
        std::filesystem::path inputPath(path);

        if (BinEditorUtils::IsRestrictedFile(inputPath)) {
            ++result.skipped;
            return result;
        }

        if (!std::filesystem::exists(inputPath)) {
            ++result.failed;
            return result;
        }

        std::string targetFolder = folder;
        if (targetFolder.empty()) {
            targetFolder = "default";
        }

        std::filesystem::path binParent = std::filesystem::path(BinEditorUtils::ToWString(editor.GetBinPath())).parent_path();
        std::filesystem::path selectedFolder = editor.GetTempDir() / targetFolder;
        if (std::filesystem::is_directory(inputPath)) {
            std::string droppedFolderName = inputPath.filename().string();
            std::string effectiveFolder = targetFolder == "default" ? droppedFolderName : targetFolder + "/" + droppedFolderName;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(inputPath, std::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_regular_file() && !BinEditorUtils::IsRestrictedFile(entry.path())) {
                    std::filesystem::path filePath = entry.path();
                    std::string relativePath = std::filesystem::relative(filePath, inputPath).string();
                    std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                    std::string targetPath = effectiveFolder;
                    if (!relativePath.empty()) {
                        targetPath += "/" + relativePath;
                    }
                    targetPath = BinEditorUtils::NormalizeName(targetPath);

                    bool isDuplicate = false;
                    for (const auto& binEntry : editor.GetEntries()) {
                        if (binEntry.name == targetPath) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    if (isDuplicate) {
                        ++result.skipped;
                        continue;
                    }

                    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
                    if (!in) {
                        ++result.failed;
                        continue;
                    }

                    auto size = in.tellg();
                    if (static_cast<uint64_t>(size) > BinEditor::MAX_FILE_SIZE) {
                        ++result.skipped;
                        continue;
                    }

                    in.seekg(0);
                    std::vector<char> buf(static_cast<size_t>(size));
                    in.read(buf.data(), size);
                    if (!in) {
                        in.close();
                        ++result.failed;
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

                    std::filesystem::path outPath = editor.GetTempDir() / binEntry.name;
                    std::filesystem::create_directories(outPath.parent_path());
                    std::ofstream fout(outPath, std::ios::binary);
                    if (!fout) {
                        ++result.failed;
                        continue;
                    }
                    fout.write(binEntry.data.data(), binEntry.data.size());
                    fout.close();
                    if (!std::filesystem::exists(outPath)) {
                        ++result.failed;
                        continue;
                    }

                    editor.GetEntriesMutable().push_back(binEntry);
                    ++result.added;
                } else if (entry.is_regular_file()) {
                    ++result.skipped;
                }
            }
        } else {
            std::string newFileName = CombinePath(targetFolder, wpath, editor.GetTempDir(), selectedFolder);
            newFileName = BinEditorUtils::NormalizeName(newFileName);
            for (const auto& entry : editor.GetEntries()) {
                if (entry.name == newFileName) {
                    ++result.skipped;
                    return result;
                }
            }

            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in) {
                ++result.failed;
                return result;
            }

            auto size = in.tellg();
            if (static_cast<uint64_t>(size) > BinEditor::MAX_FILE_SIZE) {
                ++result.skipped;
                return result;
            }

            in.seekg(0);
            std::vector<char> buf(static_cast<size_t>(size));
            in.read(buf.data(), size);
            if (!in) {
                in.close();
                ++result.failed;
                return result;
            }
            in.close();

            BinEntry entry;
            entry.name = newFileName;
            entry.size = static_cast<uint64_t>(size);
            entry.dataOffset = 0;
            entry.data = std::move(buf);
            auto last_write = std::filesystem::last_write_time(inputPath);
            entry.last_modified = static_cast<uint64_t>(BinEditorUtils::to_time_t(last_write));

            std::filesystem::path outPath = editor.GetTempDir() / entry.name;
            if (targetFolder != "default") {
                std::filesystem::create_directories(outPath.parent_path());
            }
            std::ofstream fout(outPath, std::ios::binary);
            if (!fout) {
                ++result.failed;
                return result;
            }
            fout.write(entry.data.data(), entry.data.size());
            fout.close();
            if (!std::filesystem::exists(outPath)) {
                ++result.failed;
                return result;
            }

            editor.GetEntriesMutable().push_back(entry);
            ++result.added;
        }
    } catch (const std::exception&) {
        MessageBoxW(hwnd, L"An error occurred while adding the file.", L"Error", MB_OK | MB_ICONERROR);
        ++result.failed;
    }
    return result;
}

std::vector<ParsedMetadata> ParseMetadataFile(const std::string& metadataPath, HWND hwnd) {
    std::vector<ParsedMetadata> metadata;
    if (!std::filesystem::exists(metadataPath)) {
        MessageBoxW(hwnd, L"Metadata file not found. Please select metadata.h", L"Error", MB_OK | MB_ICONERROR);
        auto paths = OpenFileDialog(hwnd, L"Header Files\0*.h\0", false);
        if (paths.empty()) return metadata;
        return ParseMetadataFile(BinEditorUtils::ToString(paths[0]), hwnd);
    }

    std::ifstream in(metadataPath);
    if (!in) {
        MessageBoxW(hwnd, (std::wstring(L"Error opening metadata.h: ") + BinEditorUtils::ToWString(metadataPath)).c_str(), L"Error", MB_OK | MB_ICONERROR);
        return metadata;
    }

    std::string line;
    bool inArray = false;
    std::regex entryRegex(R"(\{\s*\"([^\"]+)\",\s*(\d+),\s*(\d+)(?:,\s*\"[^\"]*\",\s*\"[^\"]*\",\s*\d+)?\s*\},?)");
    while (std::getline(in, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.find("assets[] = {") != std::string::npos || line.find("assets_extra[] = {") != std::string::npos) {
            inArray = true;
            continue;
        }
        if (inArray && line.find("};") != std::string::npos) {
            break;
        }
        if (inArray && !line.empty()) {
            std::smatch match;
            if (std::regex_match(line, match, entryRegex)) {
                ParsedMetadata entry;
                entry.name = BinEditorUtils::NormalizeName(match[1].str());
                try {
                    entry.offset = std::stoull(match[2].str());
                    entry.size = std::stoull(match[3].str());
                    metadata.push_back(entry);
                } catch (const std::exception& e) {
                    MessageBoxW(hwnd, (std::wstring(L"Error parsing metadata.h: ") + BinEditorUtils::ToWString(e.what())).c_str(), L"Error", MB_OK | MB_ICONERROR);
                    return {};
                }
            }
        }
    }
    if (metadata.empty()) {
        MessageBoxW(hwnd, L"No valid data found in metadata.h", L"Error", MB_OK | MB_ICONERROR);
    }
    return metadata;
}

HWND CreateProgressDialog(HWND parent, const std::wstring& title, ProgressDialogData* dialogData) {
    int x = (GetSystemMetrics(SM_CXSCREEN) - 300) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - 150) / 2;
    HWND hwndProgress = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", title.c_str(),
        WS_POPUP | WS_CAPTION, x, y, 300, 150,
        parent, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwndProgress) {
        MessageBoxW(parent, L"Failed to create progress dialog", L"Error", MB_OK | MB_ICONERROR);
        return nullptr;
    }
    CreateWindowW(L"STATIC", L"0%", WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 20, 260, 20, hwndProgress, nullptr, nullptr, nullptr);
    HWND hBar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE, 20, 50, 260, 20,
        hwndProgress, nullptr, nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
        110, 80, 80, 25, hwndProgress, (HMENU)ID_BUTTON_PROG_CANCEL, nullptr, nullptr);
    SendMessage(hBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SetWindowLongPtr(hwndProgress, GWLP_USERDATA, (LONG_PTR)dialogData);
    ShowWindow(hwndProgress, SW_SHOW);
    UpdateWindow(hwndProgress);
    return hBar;
}

void UpdateProgressDialog(HWND hBar, int percent) {
    if (!hBar) return;
    HWND hwndProgress = GetParent(hBar);
    ProgressDialogData* dialogData = (ProgressDialogData*)GetWindowLongPtrW(hwndProgress, GWLP_USERDATA);
    if (dialogData && dialogData->cancelled) return;
    HWND hText = GetDlgItem(hwndProgress, 0);
    std::wostringstream oss;
    oss << percent << L"%";
    SetWindowTextW(hText, oss.str().c_str());
    SendMessage(hBar, PBM_SETPOS, percent, 0);
}

void CloseProgressDialog(HWND hBar) {
    if (hBar) {
        HWND hwndProgress = GetParent(hBar);
        DestroyWindow(hwndProgress);
    }
}

int GetListBoxItemUnderMouse(HWND hList, POINT pt) {
    ScreenToClient(hList, &pt);
    int index = (int)SendMessage(hList, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
    if (HIWORD(index) == 0) {
        return LOWORD(index);
    }
    return -1;
}

void UpdateListBox(HWND hListBox, const BinEditor& editor) {
    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
    for (const auto& entry : editor.GetEntries()) {
        std::string displayText = entry.name + " (" + std::to_string(entry.size) + " bytes)";
        SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
    }
    SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)"");
}
