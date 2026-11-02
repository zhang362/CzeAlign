#include "rpp.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace ra {
namespace {

std::string Trim(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) {
        ++b;
    }
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) {
        --e;
    }
    return s.substr(b, e - b);
}

std::vector<std::string> SplitTokens(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        if (i >= s.size()) {
            break;
        }
        if (s[i] == '"') {
            ++i;
            const size_t start = i;
            while (i < s.size() && s[i] != '"') {
                ++i;
            }
            out.emplace_back(s.substr(start, i - start));
            if (i < s.size()) {
                ++i;
            }
        } else {
            const size_t start = i;
            while (i < s.size() && s[i] != ' ' && s[i] != '\t') {
                ++i;
            }
            out.emplace_back(s.substr(start, i - start));
        }
    }
    return out;
}

// 取第一个 token（key），其余作为值；值里的引号会被去掉。
std::string FirstToken(const std::string& s, std::string* rest) {
    size_t i = 0;
    while (i < s.size() && s[i] != ' ' && s[i] != '\t') {
        ++i;
    }
    if (rest) {
        *rest = Trim(s.substr(i));
    }
    return s.substr(0, i);
}

std::string Unquote(const std::string& s) {
    const std::string t = Trim(s);
    if (t.empty()) {
        return std::string();
    }
    if (t[0] == '"') {
        const size_t end = t.find('"', 1);
        if (end == std::string::npos) {
            return t.substr(1);
        }
        return t.substr(1, end - 1);
    }
    return t;
}

std::wstring ToWide(const std::string& s) {
    if (s.empty()) {
        return std::wstring();
    }
    int need = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                                   static_cast<int>(s.size()), nullptr, 0);
    UINT codepage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (need <= 0) {
        codepage = CP_ACP;
        flags = 0;
        need = MultiByteToWideChar(codepage, flags, s.data(), static_cast<int>(s.size()),
                                   nullptr, 0);
    }
    if (need <= 0) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(need), L'\0');
    MultiByteToWideChar(codepage, flags, s.data(), static_cast<int>(s.size()), out.data(), need);
    return out;
}

bool ReadWholeFile(const std::wstring& path, std::string* out) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart <= 0 ||
        size.QuadPart > 128LL * 1024 * 1024) {
        CloseHandle(handle);
        return false;
    }
    out->resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = ReadFile(handle, out->data(), static_cast<DWORD>(out->size()), &read, nullptr);
    CloseHandle(handle);
    if (!ok) {
        return false;
    }
    out->resize(read);
    return true;
}

// ---------------------------------------------------------------------------
//  行解析
// ---------------------------------------------------------------------------

struct Parser {
    RppProject* project = nullptr;
    std::vector<std::string> stack;
    RppTrack* track = nullptr;
    RppItem* item = nullptr;

    void Open(const std::string& tag) {
        stack.push_back(tag);
        if (tag == "TRACK") {
            project->tracks.emplace_back();
            track = &project->tracks.back();
            item = nullptr;
        } else if (tag == "ITEM" && track) {
            track->items.emplace_back();
            item = &track->items.back();
        }
    }

    void Close() {
        if (stack.empty()) {
            return;
        }
        const std::string tag = stack.back();
        stack.pop_back();
        if (tag == "ITEM") {
            item = nullptr;
        } else if (tag == "TRACK") {
            track = nullptr;
            item = nullptr;
        }
    }

    void KeyValue(const std::string& key, const std::string& value) {
        const std::string tag = stack.empty() ? std::string() : stack.back();

        if (tag == "TRACK" && track) {
            if (key == "NAME") {
                track->name = ToWide(Unquote(value));
            }
            return;
        }

        if (tag == "ITEM" && item) {
            if (key == "POSITION") {
                item->position = std::atof(value.c_str());
            } else if (key == "LENGTH") {
                item->length = std::atof(value.c_str());
            } else if (key == "NAME") {
                item->name = ToWide(Unquote(value));
            } else if (key == "MUTE") {
                item->muted = std::atof(value.c_str()) != 0.0;
            } else if (key == "PLAYRATE") {
                const std::vector<std::string> tokens = SplitTokens(value);
                const double rate = tokens.empty() ? 1.0 : std::atof(tokens[0].c_str());
                if (tokens.size() >= 3) {
                    item->pitch = std::atof(tokens[2].c_str());
                    item->has_pitch = true;
                } else if (rate > 0.0) {
                    item->pitch = 12.0 * std::log2(rate);
                    item->has_pitch = true;
                }
            }
            return;
        }

        if (tag == "SOURCE" && item) {
            if (key == "FILE") {
                item->file = ToWide(Unquote(value));
            }
            return;
        }
    }

    void Line(const std::string& raw) {
        const std::string line = Trim(raw);
        if (line.empty()) {
            return;
        }
        if (line[0] == '<') {
            size_t end = 1;
            while (end < line.size() && line[end] != ' ' && line[end] != '\t') {
                ++end;
            }
            Open(line.substr(1, end - 1));
            return;
        }
        if (line == ">") {
            Close();
            return;
        }
        std::string rest;
        const std::string key = FirstToken(line, &rest);
        KeyValue(key, rest);
    }
};

}  // namespace

double RppTrack::Start() const {
    if (items.empty()) {
        return 0.0;
    }
    double v = items.front().position;
    for (const RppItem& it : items) {
        v = std::min(v, it.position);
    }
    return v;
}

double RppTrack::End() const {
    double v = 0.0;
    for (const RppItem& it : items) {
        v = std::max(v, it.position + it.length);
    }
    return v;
}

int RppProject::ItemCount() const {
    int n = 0;
    for (const RppTrack& t : tracks) {
        n += static_cast<int>(t.items.size());
    }
    return n;
}

bool ParseRppText(const std::string& utf8, RppProject* out, std::wstring* error) {
    if (!out) {
        return false;
    }
    out->tracks.clear();

    std::string text = utf8;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    Parser parser;
    parser.project = out;

    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) {
            nl = text.size();
        }
        parser.Line(text.substr(pos, nl - pos));
        if (nl == text.size()) {
            break;
        }
        pos = nl + 1;
    }

    if (out->tracks.empty()) {
        if (error) {
            *error = L"没有找到 TRACK 段，可能不是 Reaper 工程文件（.rpp）。";
        }
        return false;
    }
    return true;
}

bool ParseRppFile(const std::wstring& path, RppProject* out, std::wstring* error) {
    std::string bytes;
    if (!ReadWholeFile(path, &bytes)) {
        if (error) {
            *error = L"无法读取 RPP 文件：\n" + path;
        }
        return false;
    }
    return ParseRppText(bytes, out, error);
}

}  // namespace ra
