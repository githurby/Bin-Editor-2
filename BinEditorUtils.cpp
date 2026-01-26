#include "Resource.h"
#include "BinEditorUtils.h"
#include <algorithm>
#include <windows.h>
#include <sstream>
#include <filesystem>

namespace BinEditorUtils {
    std::string getExt(const std::string& name) {
        size_t dot = name.find_last_of('.');
        if (dot != std::string::npos) return name.substr(dot);
        return "";
    }

    std::string getMime(const std::string& ext) {
        std::string lower_ext = ext;
        std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
        if (lower_ext == ".jpg" || lower_ext == ".jpeg") return "image/jpeg";
        if (lower_ext == ".png") return "image/png";
        if (lower_ext == ".gif") return "image/gif";
        if (lower_ext == ".bmp") return "image/bmp";
        if (lower_ext == ".mp3") return "audio/mpeg";
        if (lower_ext == ".wav") return "audio/wav";
        if (lower_ext == ".ogg") return "audio/ogg";
        if (lower_ext == ".wma") return "audio/x-ms-wma";
        if (lower_ext == ".txt") return "text/plain";
        if (lower_ext == ".html") return "text/html";
        if (lower_ext == ".pdf") return "application/pdf";
        if (lower_ext == ".zip") return "application/zip";
        // Add more as needed
        return "application/octet-stream";
    }

    std::time_t to_time_t(std::filesystem::file_time_type tp) {
        using namespace std::chrono;
        auto dur = tp.time_since_epoch();
        auto sec = duration_cast<seconds>(dur).count();
    #ifdef _WIN32
        sec -= 11644473600LL;  // Subtract Windows FILETIME offset (1601 to 1970 in seconds)
    #endif
        return static_cast<std::time_t>(sec);
    }

    std::wstring ToWString(const std::string& str) {
        size_t len = str.length();
        std::wstring wstr;
        wstr.resize(len + 1);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), len + 1, &wstr[0], len + 1);
        return wstr;
    }

    std::string ToString(const std::wstring& wstr) {
        size_t len = wstr.length();
        std::string str;
        str.resize(len + 1);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), len + 1, &str[0], len + 1, nullptr, nullptr);
        return str;
    }

    std::string GenerateUniqueTempDir() {
        std::time_t now = std::time(nullptr);
        std::stringstream ss;
        ss << "bintemp_" << now;
        return ss.str();
    }

    bool IsRestrictedFile(const std::filesystem::path& path) {
        std::wstring filename = path.filename().wstring();
        std::wstring ext = path.extension().wstring();
        return ext == L".bin" ||
               ext == L".h" ||
               filename == L"metadata.h";
    }

    std::string NormalizeName(std::string name) {
        while (name.rfind("./", 0) == 0) {
            name = name.substr(2);
        }
        return name;
    }

    std::wstring GetTempPreviewFile(const std::filesystem::path& original) {
        wchar_t tempPath[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempPath) == 0) {
            return L"";
        }
        wchar_t tempFile[MAX_PATH];
        if (GetTempFileNameW(tempPath, L"bep", 0, tempFile) == 0) {
            return L"";
        }
        std::filesystem::path tempP(tempFile);
        try {
            std::filesystem::copy_file(original, tempP, std::filesystem::copy_options::overwrite_existing);
            std::filesystem::path origExt = original.extension();
            std::filesystem::path newTemp = tempP.replace_extension(origExt);
            std::filesystem::rename(tempP, newTemp);
            return newTemp.wstring();
        } catch (const std::exception&) {
            return L"";
        }
        return L"";
    }
}
