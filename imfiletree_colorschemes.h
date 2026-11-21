#pragma once

#ifndef IMGUI_DISABLE
#include <imgui/imgui.h>

#include <stdint.h>

#define ICON_MAP_BY_FILENAME_SIZE_MAX   217
#define ICON_MAP_BY_FILE_EXT_SIZE_MAX   491

extern const ImU32 IM_FILE_TREE_COLOR_DEFAULT;
extern const ImU32 IM_FILE_TREE_COLOR_IGNORE;
extern const ImU32 IM_FILE_TREE_COLOR_BLACK;
extern const ImU32 IM_FILE_TREE_COLOR_BLACK_DARK;
extern const ImU32 IM_FILE_TREE_COLOR_BLUE;
extern const ImU32 IM_FILE_TREE_COLOR_GREEN;
extern const ImU32 IM_FILE_TREE_COLOR_GREY;
extern const ImU32 IM_FILE_TREE_COLOR_GREY_DARK;
extern const ImU32 IM_FILE_TREE_COLOR_GREY_LIGHT;
extern const ImU32 IM_FILE_TREE_COLOR_ORANGE;
extern const ImU32 IM_FILE_TREE_COLOR_PINK;
extern const ImU32 IM_FILE_TREE_COLOR_PURPLE;
extern const ImU32 IM_FILE_TREE_COLOR_RED;
extern const ImU32 IM_FILE_TREE_COLOR_STEEL;
extern const ImU32 IM_FILE_TREE_COLOR_YELLOW;
extern const ImU32 IM_FILE_TREE_COLOR_WHITE;

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
