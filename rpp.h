#pragma once

// -----------------------------------------------------------------------------
//  RPP (Reaper project file) 简析器
//  只提取本插件需要的信息：轨道、item 的位置/长度/音高/名称/媒体文件。
// -----------------------------------------------------------------------------

#include <string>
#include <vector>

namespace ra {

struct RppItem {
    double position = 0.0;  // 秒
    double length = 0.0;    // 秒
    double pitch = 0.0;     // 半音（PLAYRATE 的第 3 个值）
    bool has_pitch = false;
    bool muted = false;
    std::wstring name;      // take 名称（ITEM 的 NAME）
    std::wstring file;      // SOURCE 的 FILE
};

struct RppTrack {
    std::wstring name;
    std::vector<RppItem> items;

    double Start() const;
    double End() const;
};

struct RppProject {
    std::vector<RppTrack> tracks;

    int ItemCount() const;
    bool Empty() const { return tracks.empty(); }
};

// 读取并解析文件。失败时返回 false，error 里是中文原因。
bool ParseRppFile(const std::wstring& path, RppProject* out, std::wstring* error);

// 直接解析 UTF-8 文本（测试用）。
bool ParseRppText(const std::string& utf8, RppProject* out, std::wstring* error);

}  // namespace ra
