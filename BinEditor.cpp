#include "BinEditor.h"
#include "BinEditorUtils.h"
#include <filesystem>

BinEditor::BinEditor(const std::filesystem::path& binsPath) : tempBinsPath(binsPath) {
    tempDir = std::filesystem::path();
}

BinEditor::~BinEditor() {
    for (const auto& dir : tempDirs) {
        try {
            if (std::filesystem::exists(dir)) {
                std::filesystem::remove_all(dir);
            }
        } catch (const std::exception&) {
        }
    }
}

void BinEditor::RemoveTempDir(const std::filesystem::path& dir) {
    tempDirs.erase(dir);
}

void BinEditor::CleanupCurrentTemp() {
    if (!tempDir.empty() && std::filesystem::exists(tempDir)) {
        try {
            std::filesystem::remove_all(tempDir);
        } catch (const std::exception&) {
        }
        tempDir = std::filesystem::path();
    }
}

const std::vector<BinEntry>& BinEditor::GetEntries() const { return binEntries; }
const std::string& BinEditor::GetBinPath() const { return binPathOriginal; }
void BinEditor::SetBinPath(const std::string& path) {
    if (path == binPathOriginal) return;
    auto oldParent = std::filesystem::path(BinEditorUtils::ToWString(binPathOriginal)).parent_path();
    auto newParent = std::filesystem::path(BinEditorUtils::ToWString(path)).parent_path();
    binPathOriginal = path;
    if (newParent != oldParent || tempDir.empty()) {
        std::string tempDirName = BinEditorUtils::GenerateUniqueTempDir();
        tempDir = tempBinsPath / tempDirName;
        tempDirs.insert(tempDir);
    }
}
const std::filesystem::path& BinEditor::GetTempDir() const { return tempDir; }
std::vector<BinEntry>& BinEditor::GetEntriesMutable() { return binEntries; }
bool BinEditor::IsBinLoaded() const { return !binPathOriginal.empty() && !tempDir.empty(); }