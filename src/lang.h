#pragma once

// -----------------------------------------------------------------------------
//  界面语言：中文 / English / 日本語
//  默认跟随 AviUtl2 的语言设置（[Translate] Language=...），否则跟随系统。
// -----------------------------------------------------------------------------

#include <windows.h>

namespace lang {

enum class Id { Zh, En, Ja };

Id Current();
void SetLanguage(Id id);

inline const wchar_t* T(const wchar_t* zh, const wchar_t* en, const wchar_t* ja) {
    switch (Current()) {
        case Id::En:
            return en;
        case Id::Ja:
            return ja;
        default:
            return zh;
    }
}

// 启动时调用一次（打开对话框前也会再调用）
Id Detect();

// 只在第一次调用时检测（之后保留用户手动切换的结果）
void EnsureDetected();

}  // namespace lang
