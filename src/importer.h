#pragma once

// -----------------------------------------------------------------------------
//  RPP → AviUtl2 物件 的排布逻辑
//  纯计算部分（BuildPlan）不依赖宿主，方便单独测试。
// -----------------------------------------------------------------------------

#include <string>
#include <vector>

#include "rpp.h"

struct EDIT_SECTION;

namespace ra {

struct ImportOptions {
    enum WrapMode { WRAP_LAYERS = 0, WRAP_TIME = 1 };

    std::wstring path;
    std::vector<int> track_indices;  // 0 起

    int start_layer = 0;    // 0 起（UI 上是 1 起）
    int empty_layers = 1;   // 两个物件之间空几层
    int start_frame = 0;    // AviUtl2 里的起始帧（对应时间范围的开始）
    bool name_objects = true; // 用 item 名 + 音高命名物件

    // 回卷方式：每 wrap_layers 层回到起始层，或每 wrap_seconds 秒回到起始层
    int wrap_mode = WRAP_LAYERS;
    int wrap_layers = 8;
    double wrap_seconds = 10.0;

    // RPP 时间范围（秒）。range_end_sec <= range_start_sec 表示到工程结尾。
    double range_start_sec = 0.0;
    double range_end_sec = 0.0;

    std::wstring effect = L"画像ファイル";
    double fps = 60.0;
    int layer_limit = 100;  // AviUtl2 固定 100 层
};

struct PlannedObject {
    int layer = 0;
    int frame = 0;
    int length = 1;
    std::wstring name;
};

struct ImportResult {
    int created = 0;
    int failed = 0;
    int fallback = 0;
};

// 分批生成时在多次调用之间保持的状态
struct CreateState {
    std::wstring effect = L"画像ファイル";
    bool type_verified = false;  // 已经成功建过物件后就不再尝试其它类型
    int created = 0;
    int failed = 0;
};

// 按时间顺序把所有选中轨道的 item 排成一个「阶梯」：每个物件下移
// (empty_layers + 1) 层，超过 layer_limit 后回到 start_layer。
std::vector<PlannedObject> BuildPlan(const RppProject& project, const ImportOptions& options);

// 生成 plan[begin, end) 区间的物件（可以在一次 call_edit_section 里调用一部分）。
void CreateSlice(EDIT_SECTION* edit, const std::vector<PlannedObject>& plan, size_t begin,
                 size_t end, bool name_objects, CreateState* state);

// 一次性生成全部（测试与简单调用用）。
ImportResult CreateObjects(EDIT_SECTION* edit, const std::vector<PlannedObject>& plan,
                           const ImportOptions& options);

// 物件名：item 名（或媒体文件名）+ 音高，例如 "ding_c.mp3 +4"。
std::wstring MakeObjectName(const RppTrack& track, const RppItem& item);

}  // namespace ra
