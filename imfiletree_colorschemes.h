#pragma once

#ifndef IMGUI_DISABLE
#include <imgui/imgui.h>

#include "misc/color_utils.h"

#include <stdint.h>

#define ICON_MAP_BY_FILENAME_SIZE_MAX   217
#define ICON_MAP_BY_FILE_EXT_SIZE_MAX   491

static const ImU32 IM_FILE_TREE_COLOR_DEFAULT      = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#D4D7D6");
static const ImU32 IM_FILE_TREE_COLOR_IGNORE       = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#41535B");
static const ImU32 IM_FILE_TREE_COLOR_BLACK        = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#0E1112");
static const ImU32 IM_FILE_TREE_COLOR_BLACK_DARK   = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#090B0D");
static const ImU32 IM_FILE_TREE_COLOR_BLUE         = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#519ABA");
static const ImU32 IM_FILE_TREE_COLOR_GREEN        = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#8DC149");
static const ImU32 IM_FILE_TREE_COLOR_GREY         = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#4D5A5E");
static const ImU32 IM_FILE_TREE_COLOR_GREY_DARK    = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#1F2326");
static const ImU32 IM_FILE_TREE_COLOR_GREY_LIGHT   = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#6D8086");
static const ImU32 IM_FILE_TREE_COLOR_ORANGE       = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#E37933");
static const ImU32 IM_FILE_TREE_COLOR_PINK         = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#F55385");
static const ImU32 IM_FILE_TREE_COLOR_PURPLE       = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#A074C4");
static const ImU32 IM_FILE_TREE_COLOR_RED          = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#CC3E44");
static const ImU32 IM_FILE_TREE_COLOR_STEEL        = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#7494A3");
static const ImU32 IM_FILE_TREE_COLOR_YELLOW       = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#CBCB41");
static const ImU32 IM_FILE_TREE_COLOR_WHITE        = CONSTEXPR_STR_COLOR_EXTRACT_HEX("#D4D7D6");

struct ImFileTreeIcon
{
    const char* Codepoint;
    ImU32       Color;
};

struct ImFileTreeIconColor
{
    uint32_t        KeyHash;
    ImFileTreeIcon  Icon;
};

extern ImFileTreeIconColor icon_map_by_filename_nvim_web_devicons_default[ICON_MAP_BY_FILENAME_SIZE_MAX];
extern ImFileTreeIconColor icon_map_by_file_ext_nvim_web_devicons_default[ICON_MAP_BY_FILE_EXT_SIZE_MAX];

extern ImFileTreeIconColor icon_map_by_filename_nvim_web_devicons_light[ICON_MAP_BY_FILENAME_SIZE_MAX];
extern ImFileTreeIconColor icon_map_by_file_ext_nvim_web_devicons_light[ICON_MAP_BY_FILE_EXT_SIZE_MAX];

bool ImFileTreeIconMapGetIcon(ImFileTreeIconColor* map, int map_size, const char* key, ImFileTreeIcon& icon);
#endif // IMGUI_DISABLE
