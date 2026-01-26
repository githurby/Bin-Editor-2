#pragma once
#include "Type.h"
#include <vector>
#include <string>
#include <filesystem>
#include <set>

class BinEditor {
private:
    std::vector<BinEntry> binEntries;
    std::string binPathOriginal;
    std::filesystem::path tempDir;
    std::set<std::filesystem::path> tempDirs;
    std::filesystem::path tempBinsPath;

public:
    static constexpr uint64_t MAX_FILE_SIZE = 1'000'000'000;

    BinEditor(const std::filesystem::path& binsPath);
    ~BinEditor();

    void RemoveTempDir(const std::filesystem::path& dir);
    void CleanupCurrentTemp();

    const std::vector<BinEntry>& GetEntries() const;
    const std::string& GetBinPath() const;
    void SetBinPath(const std::string& path);
    const std::filesystem::path& GetTempDir() const;
    std::vector<BinEntry>& GetEntriesMutable();
    bool IsBinLoaded() const;
};