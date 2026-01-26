#pragma once
#include <string>
#include <filesystem>
#include <ctime>
#include <vector>
#include <string>

namespace BinEditorUtils {
    std::string getExt(const std::string& name);
    std::string getMime(const std::string& ext);
    std::time_t to_time_t(std::filesystem::file_time_type tp);
    std::wstring ToWString(const std::string& str);
    std::string ToString(const std::wstring& wstr);
    std::string GenerateUniqueTempDir();
    bool IsRestrictedFile(const std::filesystem::path& path);
    std::string NormalizeName(std::string name);
    std::wstring GetTempPreviewFile(const std::filesystem::path& original);
}