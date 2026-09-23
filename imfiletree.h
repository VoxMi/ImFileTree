#pragma once
#ifndef IMGUI_DISABLE

struct ImFileTreeContext;

//-----------------------------------------------------------------------------
// [SECTION] End-user API Functions
//-----------------------------------------------------------------------------

namespace ImFileTree
{
    ImFileTreeContext*  CreateContext();
    void                DestroyContext(ImFileTreeContext* ctx = nullptr);
    ImFileTreeContext*  GetCurrentContext();
    void                SetCurrentContext(ImFileTreeContext* ctx);

    void                AddRootPath(const char* root_path);
    void                Render(float scan_dir_period = 1.0f, bool sort_content = false);

    void                StyleIconColorsDefault();
    void                StyleIconColorsLight();
}
#endif // #ifndef IMGUI_DISABLE
