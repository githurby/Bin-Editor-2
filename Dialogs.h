#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include "Type.h"
#include "BinEditor.h"

std::vector<std::wstring> OpenFileDialog(HWND hwnd, const wchar_t* filter, bool multiSelect = false);
std::wstring BrowseForFolder(HWND hwnd, const std::wstring& title, const std::filesystem::path& rootDir);
std::string SelectFolderForAdd(HWND hwnd, const BinEditor& editor);
std::string CombinePath(const std::string& folder, const std::wstring& filename, const std::filesystem::path& tempDir, const std::filesystem::path& selectedFolder);
AddResult AddFileToBin(HWND hwnd, HWND hList, const std::wstring& wpath, BinEditor& editor, const std::string& folder);
std::vector<ParsedMetadata> ParseMetadataFile(const std::string& metadataPath, HWND hwnd);
HWND CreateProgressDialog(HWND parent, const std::wstring& title, ProgressDialogData* dialogData);
void UpdateProgressDialog(HWND hBar, int percent);
void CloseProgressDialog(HWND hBar);
int GetListBoxItemUnderMouse(HWND hList, POINT pt);
void UpdateListBox(HWND hListBox, const BinEditor& editor);