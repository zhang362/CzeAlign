#pragma once

#include <windows.h>

#include <string>

#include "importer.h"

// 显示导入对话框。返回 true 表示用户点了「生成」，options 里是结果。
// 失败时 error_out 非空（用于提示），返回 false。
bool ShowImportDialog(HWND owner, HINSTANCE instance, const std::wstring& initial_path,
                      ra::ImportOptions* options, std::wstring* error_out);
