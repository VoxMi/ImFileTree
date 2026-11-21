#pragma once
#ifndef IMGUI_DISABLE

#include "imgui_internal.h"

#include "imfiletree_colorschemes.h"
#include "imstring/imstring.h"

//-----------------------------------------------------------------------------
// [SECTION] ImFileTree Enums
//-----------------------------------------------------------------------------

enum ImFileTreePopupMenuType_
{
    ImFileTreePopupMenuType_None = 0,
    ImFileTreePopupMenuType_OnRoot,
    ImFileTreePopupMenuType_OnDir,
    ImFileTreePopupMenuType_OnFile
};

enum ImFileTreeActionType_
{
    ImFileTreeActionType_None = 0,
    ImFileTreeActionType_Cut,
    ImFileTreeActionType_Copy,
    ImFileTreeActionType_Paste,
    ImFileTreeActionType_Move,
    ImFileTreeActionType_Rename,
    ImFileTreeActionType_OpenFile,
    ImFileTreeActionType_CreateDir,
    ImFileTreeActionType_CreateFile,
    ImFileTreeActionType_Delete,
    ImFileTreeActionType_CopyPath,
};

enum ImFileTreeInputLabelState_
{
    ImFileTreeInputLabelState_None = 0,
    ImFileTreeInputLabelState_Input,
    ImFileTreeInputLabelState_Done
};

//-----------------------------------------------------------------------------
// [SECTION] File And Directory Descriptors
//-----------------------------------------------------------------------------

struct ImFileTreeFileDesc
{
    int             NameIndex       = ImStringPool::INVALID_IDX;
    int             PathIndex       = ImStringPool::INVALID_IDX;
    int             ExtensionIndex  = ImStringPool::INVALID_IDX;
    ImFileTreeIcon  Icon;
};

struct ImFileTreeDirEntry
{
    int     NameIndex   = ImStringPool::INVALID_IDX;
    int     PathIndex   = ImStringPool::INVALID_IDX;
    int64_t ModTime     = 0;
    bool    IsOpen      = false;
    bool    IsRoot      = false;

    ImVector<ImFileTreeDirEntry> Dirs;
    ImVector<ImFileTreeFileDesc> Files;
};

//-----------------------------------------------------------------------------
// [SECTION] Multi-Select API
//-----------------------------------------------------------------------------

struct ImFileTreeMultiSelection
{
    ImVector<int>       PathsSelected;
    ImVector<int>       PathsPrevious;
    ImVector<int>       PathsCurrent;
    int                 ShiftAnchor         = ImStringPool::INVALID_IDX;
    int                 ClickedPathIndex    = ImStringPool::INVALID_IDX;
    ImVec2              ClickedMousePos;

    void                Clear()                         { PathsSelected.clear(); PathsPrevious.clear(); PathsCurrent.clear(); ShiftAnchor = ImStringPool::INVALID_IDX; };
    void                AddCurrent(int path_idx)        { PathsCurrent.push_back(path_idx); }
    void                AddSelected(int path_idx)       { PathsSelected.push_back(path_idx); }
    bool                Contains(int path_idx) const    { for(int i = 0; i < PathsSelected.Size; ++i) { if(PathsSelected[i] == path_idx) return true; } return false; };
    bool                Empty() const                   { return PathsSelected.empty(); }
    int                 Size() const                    { return PathsSelected.size(); }

    inline int&         operator[](int i)               { IM_ASSERT(i >= 0 && i < PathsSelected.size()); return PathsSelected[i]; }
    inline const int&   operator[](int i) const         { IM_ASSERT(i >= 0 && i < PathsSelected.size()); return PathsSelected[i]; }

    int                 GetSingle()                     { if(PathsSelected.size() == 1) return PathsSelected[0]; return ImStringPool::INVALID_IDX; }
    void                SetSingle(int path_idx)         { PathsSelected.clear(); PathsSelected.push_back(path_idx); ShiftAnchor = path_idx; }

    void                Begin()                         { PathsCurrent.clear(); }
    void                End()                           { PathsPrevious.swap(PathsCurrent); }

    void                Update(int path_idx);
};

//-----------------------------------------------------------------------------
// [SECTION] Action Queue Structures
//-----------------------------------------------------------------------------

struct ImFileTreeActionQueueEntry
{
    int         PathIndex   = ImStringPool::INVALID_IDX;
    const char* PathName    = nullptr;
};

struct ImFileTreeActionQueue
{
    int             Type            = ImFileTreeActionType_None;
    bool            IsActionCut     = false;
    bool            IsActionDelete  = false;
    const char*     MessageFormat   = nullptr;
    const char*     DstPath         = nullptr;
    ImVector<ImFileTreeActionQueueEntry> SrcPaths;

    void Clear()                        { SrcPaths.clear(); }
    bool Contains(int path_idx) const   { for(int i = 0; i < SrcPaths.Size; ++i) { if(SrcPaths[i].PathIndex == path_idx) return true; } return false; };
};

//-----------------------------------------------------------------------------
// [SECTION] Input Label Structures
//-----------------------------------------------------------------------------

struct ImFileTreeInputLabelSelection
{
    int  Start   = 0;
    int  End     = 0;
    bool Apply   = false;
};

struct ImFileTreeInputLabelContext
{
    bool    IsActive        = false;
    int     State           = ImFileTreeInputLabelState_None;
    int     OldPathIndex    = ImStringPool::INVALID_IDX;
    int     NewPathIndex    = ImStringPool::INVALID_IDX;
    char    TextBuffer[256];
    ImFileTreeInputLabelSelection Selection;
};

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

struct ImFileTreeContext
{
    ImVector<ImFileTreeDirEntry*>   RootDirs;
    ImFileTreeMultiSelection        Selections;
    ImFileTreeActionQueue           ActionQueue;
    ImFileTreeInputLabelContext     InputLabelContext;
    ImStringPool*                   StringPool;
    ImFileTreeIconColor*            IconColorschemeByFilename;
    ImFileTreeIconColor*            IconColorschemeByFileExtension;

    int         LastItemFocused;
    ImRect      LastItemFocusedFrameBB;
    int         PopupMenuItemPathIndex;

    float       UpdateDirContentTime;
    bool        UpdateDirContentAllow;
    bool        UpdateDirContentForce;
    bool        UpdateDirContentLock;
    bool        UpdateDirContentSort;

    void        (*FileOpenCallback)(const char* file_path);
    void        (*FileCloseCallback)(const char* file_path);
    void        (*FileRenameCallback)(const char* old_file_name, const char* new_file_name);

    ImFileTreeContext();
    ~ImFileTreeContext();
};

#ifndef GImFileTree
extern ImFileTreeContext* GImFileTree;
#endif

//-----------------------------------------------------------------------------
// [SECTION] Internal API
//-----------------------------------------------------------------------------

namespace ImFileTree
{
    void Initialize(ImFileTreeContext* ctx);

    void UpdateActionQueue();
    void UpdateDirContent(ImFileTreeDirEntry* dir_entry, const char* dir_path, bool check_mod_time = true);

    bool InputLabel(const char* label, int entry_path_idx, bool special_select = false);
    bool PopupMenu(int menu_type);

    void RenderDir(ImFileTreeDirEntry* dir_entry);
    void RenderFile(ImFileTreeFileDesc* file_desc);
    void RenderRoot(ImFileTreeDirEntry* root_dir_entry, float scan_dir_period, bool sort_content);
}
#endif // #ifndef IMGUI_DISABLE
