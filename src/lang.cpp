#include "lang.h"

#include <string>

namespace lang {
namespace {

Id g_language = Id::Zh;
bool g_detected = false;

std::wstring ToLower(std::wstring s) {
    for (wchar_t& c : s) {
        if (c >= L'A' && c <= L'Z') {
            c = static_cast<wchar_t>(c - L'A' + L'a');
        }
    }
    return s;
}

bool ReadIniLanguage(const std::wstring& path, Id* language) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 8 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }
    std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok) {
        return false;
    }
    bytes.resize(read);

    const int need =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                            static_cast<int>(bytes.size()), nullptr, 0);
    if (need <= 0) {
        return false;
    }
    std::wstring text(static_cast<size_t>(need), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
                        static_cast<int>(bytes.size()), text.data(), need);

    const size_t section = text.find(L"[Translate]");
    if (section == std::wstring::npos) {
        return false;
    }
    size_t pos = text.find(L'\n', section);
    if (pos == std::wstring::npos) {
        return false;
    }
    ++pos;
    while (pos < text.size()) {
        size_t line_end = text.find(L'\n', pos);
        if (line_end == std::wstring::npos) {
            line_end = text.size();
        }
        std::wstring line = text.substr(pos, line_end - pos);
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (line.rfind(L"Language=", 0) == 0) {
            const std::wstring value = ToLower(line.substr(9));
            if (value.find(L"english") != std::wstring::npos || value == L"en" ||
                value.find(L"en-us") != std::wstring::npos ||
                value.find(L"en-gb") != std::wstring::npos) {
                *language = Id::En;
            } else if (value.find(L"japanese") != std::wstring::npos ||
                       value.find(L"日本語") != std::wstring::npos || value == L"ja" ||
                       value.find(L"ja-jp") != std::wstring::npos) {
                *language = Id::Ja;
            } else {
                *language = Id::Zh;
            }
            return true;
        }
        if (!line.empty() && line[0] == L'[') {
            return false;
        }
        pos = line_end + 1;
    }
    return false;
}

std::wstring ExeDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir = path;
    const size_t slash = dir.find_last_of(L'\\');
    if (slash != std::wstring::npos) {
        dir.resize(slash);
    }
    return dir;
}

}  // namespace

Id Current() {
    return g_language;
}

void SetLanguage(Id id) {
    g_language = id;
}

Id Detect() {
    g_detected = true;
    wchar_t program_data[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"ProgramData", program_data, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        Id detected = Id::Zh;
        if (ReadIniLanguage(std::wstring(program_data) + L"\\aviutl2\\aviutl2.ini", &detected)) {
            g_language = detected;
            return detected;
        }
    }

    Id detected = Id::Zh;
    if (ReadIniLanguage(ExeDirectory() + L"\\data\\aviutl2.ini", &detected)) {
        g_language = detected;
        return detected;
    }

    switch (GetUserDefaultUILanguage() & 0x3ff) {
        case LANG_JAPANESE:
            g_language = Id::Ja;
            break;
        case LANG_ENGLISH:
            g_language = Id::En;
            break;
        default:
            g_language = Id::Zh;
            break;
    }
    return g_language;
}

void EnsureDetected() {
    if (!g_detected) {
        Detect();
    }
}

}  // namespace lang
