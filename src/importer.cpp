#include "importer.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "plugin2.h"

namespace ra {
namespace {

std::wstring FileStem(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    return name;
}

std::wstring FormatPitch(double pitch) {
    wchar_t buffer[32]{};
    const double rounded = std::round(pitch);
    if (std::fabs(pitch - rounded) < 0.01) {
        std::swprintf(buffer, 32, L"%+d", static_cast<int>(rounded));
    } else {
        std::swprintf(buffer, 32, L"%+.2f", pitch);
    }
    return buffer;
}

}  // namespace

std::wstring MakeObjectName(const RppTrack& /*track*/, const RppItem& item) {
    std::wstring base = item.name;
    if (base.empty()) {
        base = FileStem(item.file);
    }
    if (base.empty()) {
        base = L"物件";
    }
    if (item.has_pitch) {
        base += L" ";
        base += FormatPitch(item.pitch);
    }
    if (base.size() > 100) {
        base.resize(100);
    }
    return base;
}

std::vector<PlannedObject> BuildPlan(const RppProject& project, const ImportOptions& options) {
    struct Ref {
        double position;
        double length;
        int track;
        size_t item;
    };

    const double range_start = std::max(0.0, options.range_start_sec);
    const double range_end =
        options.range_end_sec > range_start ? options.range_end_sec : 1.0e18;

    std::vector<Ref> refs;
    for (int track_index : options.track_indices) {
        if (track_index < 0 || track_index >= static_cast<int>(project.tracks.size())) {
            continue;
        }
        const RppTrack& track = project.tracks[static_cast<size_t>(track_index)];
        for (size_t i = 0; i < track.items.size(); ++i) {
            const RppItem& item = track.items[i];
            if (item.muted || item.length <= 0.0) {
                continue;
            }
            // 只保留与时间范围重叠的部分
            const double start = std::max(item.position, range_start);
            const double end = std::min(item.position + item.length, range_end);
            if (end - start <= 0.0) {
                continue;
            }
            refs.push_back({start, end - start, track_index, i});
        }
    }

    std::stable_sort(refs.begin(), refs.end(),
                     [](const Ref& a, const Ref& b) { return a.position < b.position; });

    std::vector<PlannedObject> plan;
    plan.reserve(refs.size());

    const int step = std::max(1, options.empty_layers + 1);
    const int limit = std::clamp(options.layer_limit, 1, 100);
    const int start_layer = std::clamp(options.start_layer, 0, limit - 1);
    // 在不超出第 100 层的前提下，这一组楼梯最多能放几个物件
    const int capacity = std::max(1, (limit - 1 - start_layer) / step + 1);

    int slot = 0;
    int current_cycle = -1;
    const int wrap_layers = std::max(1, options.wrap_layers);

    for (const Ref& ref : refs) {
        const RppTrack& track = project.tracks[static_cast<size_t>(ref.track)];
        const RppItem& item = track.items[ref.item];

        if (options.wrap_mode == ImportOptions::WRAP_TIME && options.wrap_seconds > 0.0) {
            const int cycle = static_cast<int>(
                std::floor((ref.position - range_start) / options.wrap_seconds));
            if (cycle != current_cycle) {
                current_cycle = cycle;
                slot = 0;
            }
        } else if (slot >= wrap_layers) {
            slot = 0;  // 每 wrap_layers 层回到起始层
        }

        PlannedObject object;
        object.layer = start_layer + (slot % capacity) * step;
        object.frame = options.start_frame +
                       static_cast<int>(std::llround((ref.position - range_start) * options.fps));
        if (object.frame < 0) {
            object.frame = 0;
        }
        object.length = std::max(1, static_cast<int>(std::llround(ref.length * options.fps)));
        object.name = MakeObjectName(track, item);
        plan.push_back(std::move(object));

        ++slot;
    }

    return plan;
}

void CreateSlice(EDIT_SECTION* edit, const std::vector<PlannedObject>& plan, size_t begin,
                 size_t end, bool name_objects, CreateState* state) {
    if (!edit || !edit->create_object || !state) {
        return;
    }
    if (state->effect.empty()) {
        state->effect = L"画像ファイル";
    }

    static const wchar_t* kAlternatives[] = {L"画像ファイル", L"図形", L"グループ制御"};
    const size_t count = std::min(end, plan.size());

    for (size_t i = begin; i < count; ++i) {
        const PlannedObject& object = plan[i];

        // 先把这一批的第一个当成「类型探测」；成功过一次之后就不再换类型，
        // 免得因为图层重叠导致的失败把后面所有物件都换成备用类型。
        OBJECT_HANDLE handle =
            edit->create_object(state->effect.c_str(), object.layer, object.frame, object.length);
        if (!handle && !state->type_verified) {
            for (const wchar_t* alternative : kAlternatives) {
                if (state->effect == alternative) {
                    continue;
                }
                handle =
                    edit->create_object(alternative, object.layer, object.frame, object.length);
                if (handle) {
                    state->effect = alternative;
                    break;
                }
            }
        }

        if (!handle) {
            ++state->failed;
            continue;
        }
        state->type_verified = true;
        ++state->created;
        if (name_objects && edit->set_object_name) {
            edit->set_object_name(handle, object.name.c_str());
        }
    }
}

ImportResult CreateObjects(EDIT_SECTION* edit, const std::vector<PlannedObject>& plan,
                           const ImportOptions& options) {
    ImportResult result;
    if (!edit || !edit->create_object) {
        return result;
    }

    CreateState state;
    state.effect = options.effect.empty() ? L"画像ファイル" : options.effect;
    CreateSlice(edit, plan, 0, plan.size(), options.name_objects, &state);

    result.created = state.created;
    result.failed = state.failed;
    if (result.created > 0 && edit->set_edited_state) {
        edit->set_edited_state();
    }
    return result;
}

}  // namespace ra
