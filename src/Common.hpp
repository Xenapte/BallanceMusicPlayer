#pragma once
#include <string>
#include <filesystem>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

// Convert UTF-8 std::string to UTF-16 std::wstring (used for Win32 wide APIs)
inline std::wstring Utf8ToUtf16(const std::string& utf8) {
    if (utf8.empty()) return L"";
#ifdef _WIN32
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wstr[0], size_needed);
    return wstr;
#else
    return std::wstring(utf8.begin(), utf8.end());
#endif
}

// Convert UTF-16 std::wstring to UTF-8 std::string
inline std::string Utf16ToUtf8(const std::wstring& utf16) {
    if (utf16.empty()) return "";
#ifdef _WIN32
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), (int)utf16.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), (int)utf16.size(), &str[0], size_needed, NULL, NULL);
    return str;
#else
    return std::string(utf16.begin(), utf16.end());
#endif
}

// C++20 standard filesystem path to UTF-8 std::string conversion
inline std::string PathToUtf8(const std::filesystem::path& path) {
    auto u8str = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8str.data()), u8str.size());
}

// UTF-8 std::string to C++20 standard filesystem path conversion
inline std::filesystem::path Utf8ToPath(const std::string& utf8) {
    return std::filesystem::path(reinterpret_cast<const char8_t*>(utf8.data()), 
                                 reinterpret_cast<const char8_t*>(utf8.data() + utf8.size()));
}

// Check if file extension is a supported audio format
inline bool IsSupportedAudioExtension(const std::string& ext) {
    static const std::set<std::string> supported = {
        ".mp3", ".wav", ".ogg", ".flac"
    };
    return supported.contains(ext);
}
