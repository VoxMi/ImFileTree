#include "imfiletree.h"

#ifndef IMGUI_DISABLE
#include "imfiletree_internal.h"

#include "misc/nerd_font.h"
#include "misc/os_filesystem.h"
#include "misc/path_utils.h"
#include "misc/utf8_utils.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifdef _WIN32
#define ROOT_PATH_DEFAULT   ".\\"
#else
#define ROOT_PATH_DEFAULT   "./"
#endif

//-----------------------------------------------------------------------------
// [SECTION] Misc Helpers/Utilities
//-----------------------------------------------------------------------------

static int ImFileTreeRemoveDirRecursive(const char* path_name)
{
    ImFileTreeContext& gft = *GImFileTree;

    if(path_name == NULL || path_name[0] == '\0')
        return -1;
#ifdef _WIN32
    wchar_t path_name_w[PATH_MAX];
    str_utf8_to_utf16(path_name, path_name_w, _countof(path_name_w));

    DWORD attrs = GetFileAttributesW(path_name_w);
    if(attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        return -1;
#else
    struct stat st;
    if(stat(path_name, &st) != 0 || !S_ISDIR(st.st_mode))
        return -1;
#endif
    DIR* dir = fs_opendir(path_name);
    if(!dir)
        return -1;

    char sub_path[PATH_MAX];

    dirent* ent;
    while((ent = fs_readdir(dir)) != NULL)
    {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        int len = snprintf(sub_path, sizeof(sub_path), "%s" DIR_SEPARATOR "%s", path_name, ent->d_name);
        if(len < 0 || len >= (int)sizeof(sub_path))
        {
            fs_closedir(dir);
            return -1;
        }
#ifdef _WIN32
        str_utf8_to_utf16(sub_path, path_name_w, _countof(path_name_w));

        attrs = GetFileAttributesW(path_name_w);
        if(attrs == INVALID_FILE_ATTRIBUTES)
        {
            fs_closedir(dir);
            return -1;
        }

        int is_dir = attrs & FILE_ATTRIBUTE_DIRECTORY;
#else
        struct stat st;
        if(stat(sub_path, &st) != 0)
        {
            fs_closedir(dir);
            return -1;
        }

        int is_dir = S_ISDIR(st.st_mode);
#endif
        if(is_dir)
        {
            if(ImFileTreeRemoveDirRecursive(sub_path) != 0)
            {
                fs_closedir(dir);
                return -1;
            }
            else if(gft.FileCloseCallback)
                gft.FileCloseCallback(sub_path);
        }
        else
        {
            if(fs_file_remove(sub_path) != 0)
            {
                fs_closedir(dir);
                return -1;
            }
            else if(gft.FileCloseCallback)
                gft.FileCloseCallback(sub_path);
        }
    }

    fs_closedir(dir);

    if(fs_dir_remove(path_name) != 0)
        return -1;
    else if(gft.FileCloseCallback)
        gft.FileCloseCallback(path_name);

    return 0;
}

template <typename T>
static void ImFileTreeSortAlphabetical(ImVector<T>& elements, ImStringPool* string_pool)
{
    for(int i = 1; i < elements.Size; ++i)
    {
        T dir_entry = elements[i];
        const char* str1 = string_pool->at(elements[i].NameIndex);

        int j = i - 1;
        while(j >= 0)
        {
            const char* str2 = string_pool->at(elements[j].NameIndex);
            if(str1 && str2 && ImStricmp(str1, str2) < 0)
            {
                elements[j + 1] = elements[j];
                j--;
            }
            else
                break;
        }

        elements[j + 1] = dir_entry;
    }
}

static int IMGUI_CDECL ImFileTreeComparePaths(const void* lhs, const void* rhs)
{
    const char* str1 = ((ImFileTreeActionQueueEntry*)lhs)->PathName;
    const char* str2 = ((ImFileTreeActionQueueEntry*)rhs)->PathName;
    return strcmp(str1, str2);
};

static int ImFileTreeInputLabelCallback(ImGuiInputTextCallbackData* data)
{
    ImFileTreeInputLabelSelection* selection = (ImFileTreeInputLabelSelection*)(data->UserData);

    if(selection->Apply)
    {
        if(data->BufTextLen && data->BufTextLen >= selection->Start && data->BufTextLen >= selection->End)
        {
            data->SelectionStart = selection->Start;
            data->SelectionEnd = selection->End;
            data->CursorPos = selection->End;
        }

        selection->Apply = false;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// [SECTION] Multi-Select API
//-----------------------------------------------------------------------------

void ImFileTreeMultiSelection::Update(int path_idx)
{
    ImGuiContext& g = *GImGui;
    bool is_single_selection = false;

    // Shift + Click: Select range from anchor to current
    if(g.IO.KeyShift && ShiftAnchor != ImStringPool::INVALID_IDX)
    {
        int anchor_idx = ImStringPool::INVALID_IDX;
        int current_idx = ImStringPool::INVALID_IDX;

        for(int i = 0; i < PathsPrevious.Size; ++i)
        {
            if(PathsPrevious[i] == ShiftAnchor)
                anchor_idx = i;

            if(PathsPrevious[i] == path_idx)
                current_idx = i;
        }

        if(anchor_idx != ImStringPool::INVALID_IDX && current_idx != ImStringPool::INVALID_IDX)
        {
            int min_idx = anchor_idx < current_idx ? anchor_idx : current_idx;
            int max_idx = anchor_idx > current_idx ? anchor_idx : current_idx;

            // Shift + Ctrl: Add range to existing selection
            if(g.IO.KeyCtrl)
            {
                for(int i = min_idx; i <= max_idx; ++i)
                {
                    if(!Contains(PathsPrevious[i]))
                        PathsSelected.push_back(PathsPrevious[i]);
                }
            }
            else
            {
                // Replace selection with new range
                PathsSelected.clear();
                for(int i = min_idx; i <= max_idx; ++i)
                    PathsSelected.push_back(PathsPrevious[i]);
            }
        }
        else
        {
            // Fallback if anchor or current not visible
            if(!g.IO.KeyCtrl)
                PathsSelected.clear();

            if(!Contains(path_idx))
                PathsSelected.push_back(path_idx);
        }
    }
    // Ctrl + Click: Toggle selection
    else if(g.IO.KeyCtrl)
    {
        int found_idx = ImStringPool::INVALID_IDX;

        for(int i = 0; i < PathsSelected.Size; ++i)
        {
            if(PathsSelected[i] == path_idx)
            {
                found_idx = i;
                break;
            }
        }

        if(found_idx != ImStringPool::INVALID_IDX)
            PathsSelected.erase(PathsSelected.begin() + found_idx);
        else
            PathsSelected.push_back(path_idx);

        ShiftAnchor = path_idx;
    }
    // Single Click: Clear and select single item
    else
        is_single_selection = true;

    if(is_single_selection || g.IO.MouseClickedCount[0] >= 2)
        SetSingle(path_idx);
}

//-----------------------------------------------------------------------------
// [SECTION] Internal API
//-----------------------------------------------------------------------------

void ImFileTree::UpdateActionQueue()
{
    ImFileTreeContext& gft = *GImFileTree;

    gft.ActionQueue.Clear();

    if(gft.Selections.Size() >= 2)
    {
        // Get a list of selected paths
        ImVector<ImFileTreeActionQueueEntry> paths;
        paths.reserve(gft.Selections.Size());
        for(int i = 0; i < gft.Selections.Size(); ++i)
        {
            ImFileTreeActionQueueEntry path;
            path.PathIndex = gft.Selections[i];
            path.PathName = gft.StringPool->at(gft.Selections[i]);
            if(!path.PathName)
                continue;

            paths.push_back(path);
        }

        // Sort paths in alphabetical order
        qsort(paths.Data, (size_t)paths.Size, sizeof(ImFileTreeActionQueueEntry), ImFileTreeComparePaths);

        gft.ActionQueue.SrcPaths.reserve(paths.Size);

        const char* cur_path_root = nullptr;
        size_t cur_path_root_len = 0;

        // Filter the list excluding child files and directories if their root directories are in the deletion queue
        for(int i = 0; i < paths.size(); ++i)
        {
            bool is_child = false;

            ImFileTreeActionQueueEntry path = paths[i];
            size_t path_len = strlen(path.PathName);

            if(cur_path_root != nullptr && path_len > cur_path_root_len)
            {
                if(ImStrnicmp(path.PathName, cur_path_root, cur_path_root_len) == 0)
                {
                    if(path.PathName[cur_path_root_len] == '\\' || path.PathName[cur_path_root_len] == '/')
                        is_child = true;
                }
            }

            if(!is_child)
            {
                gft.ActionQueue.SrcPaths.push_back(path);
                cur_path_root = path.PathName;
                cur_path_root_len = path_len;
            }
        }
    }
    else if(gft.Selections.Size() == 1)
    {
        ImFileTreeActionQueueEntry path;
        path.PathIndex = gft.Selections[0];
        path.PathName = gft.StringPool->at(gft.Selections[0]);
        if(path.PathName)
            gft.ActionQueue.SrcPaths.push_back(path);
    }
    else if(gft.PopupMenuItemPathIndex != ImStringPool::INVALID_IDX)
    {
        ImFileTreeActionQueueEntry path;
        path.PathIndex = gft.PopupMenuItemPathIndex;
        path.PathName = gft.StringPool->at(gft.PopupMenuItemPathIndex);
        if(path.PathName)
            gft.ActionQueue.SrcPaths.push_back(path);
    }
}

void ImFileTree::UpdateDirContent(ImFileTreeDirEntry* dir_entry, const char* dir_path, bool check_mod_time)
{
    ImFileTreeContext& gft = *GImFileTree;

    if(!dir_path || dir_path[0] == '\0')
        return;

    if(!fs_dir_exists(dir_path))
        return;

    int64_t dir_mod_time = -1;
    if(check_mod_time)
    {
        dir_mod_time = fs_get_file_mod_time_ns(dir_path);
        if(dir_entry->ModTime == dir_mod_time)
            return;
    }

    DIR* dir = fs_opendir(dir_path);
    if(!dir)
        return;

    dir_entry->Dirs.clear();
    dir_entry->Files.clear();
    dir_entry->ModTime = dir_mod_time;

    dirent* ent;
    while((ent = fs_readdir(dir)) != NULL)
    {
        const char* file_name = ent->d_name;
        if(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0)
            continue;

        ImString full_path(dir_path);

        char last_char = full_path.back();
        if(!(last_char == '\\' || last_char == '/'))
            full_path += DIR_SEPARATOR;
        full_path += file_name;

        int name_idx = gft.StringPool->insert(file_name);
        int path_idx = gft.StringPool->insert(full_path.c_str());

        if(ent->d_type == DT_DIR)
        {
            ImFileTreeDirEntry dir_child;
            dir_child.NameIndex = name_idx;
            dir_child.PathIndex = path_idx;
            dir_child.IsOpen = false;
            dir_entry->Dirs.push_back(dir_child);
        }
        else
        {
            ImFileTreeFileDesc file_desc;
            file_desc.NameIndex = name_idx;
            file_desc.PathIndex = path_idx;
            const char* file_name_ext = path_get_file_name_ext(file_name, true);
            if(file_name_ext)
                file_desc.ExtensionIndex = gft.StringPool->insert(file_name_ext);

            if(!ImFileTreeIconMapGetIcon(gft.IconColorschemeByFilename, ICON_MAP_BY_FILENAME_SIZE_MAX, file_name, file_desc.Icon))
            {
                if(file_desc.ExtensionIndex != ImStringPool::INVALID_IDX)
                {
                    const char* file_extension = gft.StringPool->at(file_desc.ExtensionIndex);
                    if(file_extension)
                        ImFileTreeIconMapGetIcon(gft.IconColorschemeByFileExtension, ICON_MAP_BY_FILE_EXT_SIZE_MAX, file_extension, file_desc.Icon);
                }
            }

            dir_entry->Files.push_back(file_desc);
        }
    }

    fs_closedir(dir);

    if(gft.UpdateDirContentSort)
    {
        ImFileTreeSortAlphabetical(dir_entry->Dirs, gft.StringPool);
        ImFileTreeSortAlphabetical(dir_entry->Files, gft.StringPool);
    }
}

bool ImFileTree::InputLabel(const char* label, int path_idx, bool special_select)
{
    ImFileTreeContext& gft = *GImFileTree;

    bool value_changed = false;

    if(gft.InputLabelContext.State != ImFileTreeInputLabelState_Input)
    {
        gft.InputLabelContext.IsActive = false;
        return value_changed;
    }

    if(!gft.InputLabelContext.IsActive)
    {
        ImGui::SetKeyboardFocusHere();

        if(special_select)
        {
            int str_len = (int)strlen(gft.InputLabelContext.TextBuffer);
            if(str_len)
            {
                const char* dot_ptr = strrchr(gft.InputLabelContext.TextBuffer, '.');
                if(!dot_ptr || dot_ptr == gft.InputLabelContext.TextBuffer)
                {
                    gft.InputLabelContext.Selection.Start = 0;
                    gft.InputLabelContext.Selection.End = str_len;
                }
                else
                {
                    gft.InputLabelContext.Selection.Start = 0;
                    gft.InputLabelContext.Selection.End = (int)(dot_ptr - gft.InputLabelContext.TextBuffer);
                }
            }

            gft.InputLabelContext.Selection.Apply = true;
        }

        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImGuiID id = window->GetID("##ImFileTreeInputLabel");
        ImGuiInputTextState* state = ImGui::GetInputTextState(id);
        if(state == nullptr)
            state = &g.InputTextState;
        state->SelectAll();

        gft.InputLabelContext.IsActive = true;
    }

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
    ImGui::PushStyleColor(ImGuiCol_NavCursor, 0); // Hide keyboard navigation rectangle
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, style.FramePadding.y));
    int flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_ElideLeft | ImGuiInputTextFlags_CallbackAlways;
    if(ImGui::InputText("##ImFileTreeInputLabel", gft.InputLabelContext.TextBuffer, IM_COUNTOF(gft.InputLabelContext.TextBuffer), flags, ImFileTreeInputLabelCallback, &gft.InputLabelContext.Selection))
    {
        const char* old_path = gft.StringPool->at(path_idx);
        char* old_dir_name = path_get_dir_name(old_path);
        if(old_dir_name)
        {
            if(label[0] != '\0' && gft.InputLabelContext.TextBuffer[0] != '\0' && strcmp(label, gft.InputLabelContext.TextBuffer) != 0)
            {
                char new_path_buf[PATH_MAX];
                int len = snprintf(new_path_buf, sizeof(new_path_buf), "%s" DIR_SEPARATOR "%s", old_dir_name, gft.InputLabelContext.TextBuffer);
                if(len < (int)sizeof(new_path_buf))
                {
                    gft.InputLabelContext.OldPathIndex = path_idx;
                    gft.InputLabelContext.NewPathIndex = gft.StringPool->insert(new_path_buf);
                    gft.ActionQueue.Clear();
                    value_changed = true;
                }

                free(old_dir_name);
            }
            else
                gft.ActionQueue.Type = ImFileTreeActionType_None;
        }
        else
            gft.ActionQueue.Type = ImFileTreeActionType_None;

        gft.InputLabelContext.IsActive = false;
        gft.InputLabelContext.State = ImFileTreeInputLabelState_Done;
        gft.InputLabelContext.TextBuffer[0] = '\0';

        if(!value_changed)
        {
            gft.InputLabelContext.OldPathIndex = ImStringPool::INVALID_IDX;
            gft.InputLabelContext.NewPathIndex = ImStringPool::INVALID_IDX;
        }
    }
    else if(ImGui::IsItemDeactivated())
    {
        gft.ActionQueue.Type = ImFileTreeActionType_None;
        gft.InputLabelContext.IsActive = false;
        gft.InputLabelContext.State = ImFileTreeInputLabelState_None;
        gft.InputLabelContext.OldPathIndex = ImStringPool::INVALID_IDX;
        gft.InputLabelContext.NewPathIndex = ImStringPool::INVALID_IDX;
        gft.InputLabelContext.TextBuffer[0] = '\0';
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    return value_changed;
}

bool ImFileTree::PopupMenu(int menu_type)
{
    ImFileTreeContext& gft = *GImFileTree;

    bool popup_was_open = false;

    if(ImGui::BeginPopupContextItem())
    {
        popup_was_open = true;

        if(menu_type == ImFileTreePopupMenuType_OnFile)
        {
            if(ImGui::MenuItem("Open"))
                gft.ActionQueue.Type = ImFileTreeActionType_OpenFile;
        }
        else
        {
            if(ImGui::MenuItem("New File"))
                gft.ActionQueue.Type = ImFileTreeActionType_CreateFile;
            if(ImGui::MenuItem("New Folder"))
                gft.ActionQueue.Type = ImFileTreeActionType_CreateDir;
        }

        ImGui::Separator();

        if(menu_type != ImFileTreePopupMenuType_OnRoot)
        {
            if(ImGui::MenuItem("Cut", "Ctrl+X"))
                gft.ActionQueue.Type = ImFileTreeActionType_Cut;
            if(ImGui::MenuItem("Copy", "Ctrl+C"))
                gft.ActionQueue.Type = ImFileTreeActionType_Copy;

        }
        if(menu_type != ImFileTreePopupMenuType_OnFile)
        {
            if(ImGui::MenuItem("Paste", "Ctrl+V", false, !gft.ActionQueue.SrcPaths.empty()))
                gft.ActionQueue.Type = ImFileTreeActionType_Paste;
        }

        ImGui::Separator();

        if(ImGui::MenuItem("Copy Path"))
            gft.ActionQueue.Type = ImFileTreeActionType_CopyPath;

        if(menu_type != ImFileTreePopupMenuType_OnRoot)
        {
            ImGui::Separator();

            if(ImGui::MenuItem("Rename", "F2"))
                gft.ActionQueue.Type = ImFileTreeActionType_Rename;
            if(ImGui::MenuItem("Delete", "Delete"))
                gft.ActionQueue.Type = ImFileTreeActionType_Delete;
        }

        ImGui::EndPopup();
    }

    return popup_was_open;
}

void ImFileTree::RenderDir(ImFileTreeDirEntry* dir_entry)
{
    ImGuiContext& g = *GImGui;
    ImFileTreeContext& gft = *GImFileTree;

    bool is_double_click = g.IO.MouseClickedCount[0] >= 2;
    bool has_modifiers = g.IO.KeyCtrl || g.IO.KeyShift;

    if(dir_entry->NameIndex == ImStringPool::INVALID_IDX || dir_entry->PathIndex == ImStringPool::INVALID_IDX)
        return;

    const char* dir_name = gft.StringPool->at(dir_entry->NameIndex);
    const char* dir_path = gft.StringPool->at(dir_entry->PathIndex);
    if(!dir_name || !dir_path)
        return;

    // Mark the element as invalid for subsequent removal from the vector
    if(dir_name[0] == '?' && gft.InputLabelContext.State == ImFileTreeInputLabelState_None)
    {
        dir_entry->NameIndex = ImStringPool::INVALID_IDX;
        dir_entry->PathIndex = ImStringPool::INVALID_IDX;
        return;
    }

    gft.Selections.AddCurrent(dir_entry->PathIndex);

    ImGuiTreeNodeFlags dir_node_flags = ImGuiTreeNodeFlags_NoNavFocus | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesNone;
    if(gft.Selections.Contains(dir_entry->PathIndex))
        dir_node_flags |= ImGuiTreeNodeFlags_Selected;
    if(gft.InputLabelContext.State == ImFileTreeInputLabelState_None)
        dir_node_flags |= ImGuiTreeNodeFlags_OpenOnArrow;

    dir_node_flags |= ImGuiTreeNodeFlags_FramePadding;

    if(dir_entry->IsOpen)
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);

    dir_entry->IsOpen = ImGui::TreeNodeEx(dir_name, dir_node_flags, "");

    if(gft.LastItemFocused == dir_entry->PathIndex)
    {
        gft.ActionQueue.DstPath = gft.StringPool->at(dir_entry->PathIndex);
        gft.LastItemFocusedFrameBB.Min = ImGui::GetItemRectMin();
        gft.LastItemFocusedFrameBB.Max = ImGui::GetItemRectMax();
    }

    bool is_dir_first_open = false;
    if(ImGui::IsItemToggledOpen() && dir_entry->IsOpen)
        is_dir_first_open = true;

    ImGuiStorage* tree_node_ss = ImGui::GetStateStorage();
    ImGuiID tree_node_id = ImGui::GetItemID();

    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenBlockedByPopup))
    {
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            gft.Selections.ClickedPathIndex = dir_entry->PathIndex;
            gft.Selections.ClickedMousePos = g.IO.MousePos;
        }

        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if(has_modifiers)
            {
                if(!dir_entry->IsRoot)
                    gft.Selections.Update(dir_entry->PathIndex);

                gft.LastItemFocused = dir_entry->PathIndex;
            }
            else
            {
                if(gft.Selections.ClickedPathIndex == dir_entry->PathIndex)
                {
                    if(gft.Selections.ClickedMousePos == g.IO.MousePos)
                    {
                        if(!dir_entry->IsRoot)
                            gft.Selections.Update(dir_entry->PathIndex);
                        else
                            gft.Selections.Clear();

                        gft.LastItemFocused = dir_entry->PathIndex;
                    }
                }
            }

            gft.Selections.ClickedPathIndex = ImStringPool::INVALID_IDX;
        }

        if(ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            if(!gft.Selections.Contains(dir_entry->PathIndex))
                gft.Selections.Clear();

            gft.LastItemFocused = dir_entry->PathIndex;

            // Reset input state while renaming
            gft.ActionQueue.Type = ImFileTreeActionType_None;
            gft.InputLabelContext.IsActive = false;
            gft.InputLabelContext.State = ImFileTreeInputLabelState_None;
        }
    }

    if(gft.InputLabelContext.State == ImFileTreeInputLabelState_None)
    {
        if(PopupMenu(dir_entry->IsRoot ? ImFileTreePopupMenuType_OnRoot : ImFileTreePopupMenuType_OnDir))
        {
            gft.UpdateDirContentLock = true;
            gft.PopupMenuItemPathIndex = dir_entry->PathIndex;
        }

        if(!dir_entry->IsRoot && ImGui::BeginDragDropSource())
        {
            if(ImGui::GetDragDropPayload() == nullptr)
            {
                ImVector<int> paths;
                if(!gft.Selections.Contains(gft.Selections.ClickedPathIndex))
                    paths.push_back(dir_entry->PathIndex);
                else
                {
                    for(int i = 0; i < gft.Selections.PathsSelected.Size; ++i)
                        paths.push_back(gft.Selections.PathsSelected[i]);
                }

                ImGui::SetDragDropPayload("ImFileTreeDragDrop", paths.Data, (size_t)paths.size_in_bytes());
            }

            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            const int* payload_items = (int*)payload->Data;
            const int payload_count = payload->DataSize / (int)sizeof(int);
            if(payload_count != 0)
            {
                if(payload_count == 1)
                {
                    const char* name = gft.StringPool->at(dir_entry->NameIndex);
                    ImGui::Text(name ? name : "?");
                }
                else
                    ImGui::Text("%d", payload_count);
            }

            ImGui::EndDragDropSource();
        }

        if(ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ImFileTreeDragDrop");
            if(payload && payload->DataSize)
            {
                const ImGuiPayload* payload = ImGui::GetDragDropPayload();
                const int* payload_items = (int*)payload->Data;
                const int payload_count = payload->DataSize / (int)sizeof(int);

                if(payload_count)
                {
                    gft.Selections.Clear();
                    for(int i = 0; i < payload_count; ++i)
                        gft.Selections.AddSelected(payload_items[i]);
                    UpdateActionQueue();
                    gft.Selections.Clear();

                    gft.LastItemFocused = ImStringPool::INVALID_IDX;
                    gft.LastItemFocusedFrameBB.Min = ImVec2(0.0f, 0.0f);
                    gft.LastItemFocusedFrameBB.Max = ImVec2(0.0f, 0.0f);

                    gft.ActionQueue.Type = ImFileTreeActionType_Move;
                    gft.ActionQueue.DstPath = gft.StringPool->at(dir_entry->PathIndex);
                }
            }

            ImGui::EndDragDropTarget();
        }

        if(!ImGui::IsAnyMouseDown() && ImGui::IsItemHovered(ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", dir_path);

        if(gft.Selections.GetSingle() == dir_entry->PathIndex)
        {
            if(ImGui::IsKeyPressed(ImGuiKey_F2))
                gft.ActionQueue.Type = ImFileTreeActionType_Rename;
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Delete))
            gft.ActionQueue.Type = ImFileTreeActionType_Delete;

        if(g.IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
            gft.ActionQueue.Type = ImFileTreeActionType_Copy;

        if(g.IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X))
            gft.ActionQueue.Type = ImFileTreeActionType_Cut;

        if(g.IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
            gft.ActionQueue.Type = ImFileTreeActionType_Paste;

        if(gft.ActionQueue.Type == ImFileTreeActionType_Rename)
        {
            gft.UpdateDirContentLock = true;
            gft.InputLabelContext.State = ImFileTreeInputLabelState_Input;
            gft.InputLabelContext.OldPathIndex = dir_entry->PathIndex;
            snprintf(gft.InputLabelContext.TextBuffer, IM_COUNTOF(gft.InputLabelContext.TextBuffer), dir_name);
        }

        if(gft.PopupMenuItemPathIndex == dir_entry->PathIndex && (gft.ActionQueue.Type == ImFileTreeActionType_CreateDir || gft.ActionQueue.Type == ImFileTreeActionType_CreateFile))
        {
            // Force open the directory and update it's structure only if the node was not open.
            if(!dir_entry->IsOpen)
            {
                tree_node_ss->SetBool(tree_node_id, true);
                UpdateDirContent(dir_entry, dir_path, false);
            }

            ImString full_path(dir_path);
            char last_char = full_path.back();
            if(!(last_char == '\\' || last_char == '/'))
                full_path += DIR_SEPARATOR;
            full_path += "?";

            int name_idx = gft.StringPool->insert("?");
            int path_idx = gft.StringPool->insert(full_path.c_str());

            // This creates a new element with the temporary name "?", which, if canceled, will become invalid and be removed during the next iteration.
            if(gft.ActionQueue.Type == ImFileTreeActionType_CreateDir)
            {
                ImFileTreeDirEntry dir_child;
                dir_child.NameIndex = name_idx;
                dir_child.PathIndex = path_idx;
                dir_child.IsOpen = false;
                dir_entry->Dirs.push_front(dir_child);
            }
            else
            {
                ImFileTreeFileDesc file_desc;
                file_desc.NameIndex = name_idx;
                file_desc.PathIndex = path_idx;
                file_desc.Icon.Codepoint = NF_SETI_DEFAULT;
                file_desc.Icon.Color = IM_FILE_TREE_COLOR_DEFAULT;
                dir_entry->Files.push_front(file_desc);
            }

            // Select the node
            gft.Selections.SetSingle(path_idx);

            // Switch the node to edit mode
            gft.InputLabelContext.OldPathIndex = path_idx;
            gft.InputLabelContext.State = ImFileTreeInputLabelState_Input;

            gft.UpdateDirContentLock = true;
        }

        if(gft.ActionQueue.Type == ImFileTreeActionType_Paste && gft.LastItemFocused == dir_entry->PathIndex)
            tree_node_ss->SetBool(tree_node_id, true);
    }

    bool apply_alpha_mul = (gft.ActionQueue.IsActionCut && gft.ActionQueue.Contains(dir_entry->PathIndex));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(IM_FILE_TREE_COLOR_BLUE, apply_alpha_mul ? 0.5f : 1.0f));
    ImGui::TextUnformatted(dir_entry->IsOpen ? NF_FA_FOLDER_OPEN : NF_FA_FOLDER);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if(gft.InputLabelContext.State == ImFileTreeInputLabelState_Input && gft.InputLabelContext.OldPathIndex == dir_entry->PathIndex)
        InputLabel(dir_name, dir_entry->PathIndex);
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text, apply_alpha_mul ? 0.5f : 1.0f));
        ImGui::TextUnformatted(dir_name);
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    if(dir_entry->IsOpen)
    {
        if(!gft.UpdateDirContentLock && (gft.UpdateDirContentAllow || is_dir_first_open))
            UpdateDirContent(dir_entry, dir_path);

        // Subdirectories
        for(ImFileTreeDirEntry* sub_dir = dir_entry->Dirs.begin(); sub_dir != dir_entry->Dirs.end();)
        {
            if(sub_dir->NameIndex == ImStringPool::INVALID_IDX && sub_dir->PathIndex == ImStringPool::INVALID_IDX)
            {
                sub_dir = dir_entry->Dirs.erase(sub_dir);
                continue;
            }

            RenderDir(sub_dir);
            ++sub_dir;
        }

        // Files
        for(ImFileTreeFileDesc* file_desc = dir_entry->Files.begin(); file_desc != dir_entry->Files.end();)
        {
            if(file_desc->NameIndex == ImStringPool::INVALID_IDX && file_desc->PathIndex == ImStringPool::INVALID_IDX)
            {
                file_desc = dir_entry->Files.erase(file_desc);
                continue;
            }

            RenderFile(file_desc);
            ++file_desc;
        }

        ImGui::TreePop();
    }
}

void ImFileTree::RenderFile(ImFileTreeFileDesc* file_desc)
{
    ImGuiContext& g = *GImGui;
    ImFileTreeContext& gft = *GImFileTree;

    bool is_double_click = g.IO.MouseClickedCount[0] >= 2;
    bool has_modifiers = g.IO.KeyCtrl || g.IO.KeyShift;

    if(file_desc->NameIndex == ImStringPool::INVALID_IDX || file_desc->PathIndex == ImStringPool::INVALID_IDX)
        return;

    const char* file_name = gft.StringPool->at(file_desc->NameIndex);
    if(!file_name)
        return;

    // Mark the element as invalid for subsequent removal from the vector
    if(file_name[0] == '?' && gft.InputLabelContext.State == ImFileTreeInputLabelState_None)
    {
        file_desc->NameIndex = ImStringPool::INVALID_IDX;
        file_desc->PathIndex = ImStringPool::INVALID_IDX;
        return;
    }

    gft.Selections.AddCurrent(file_desc->PathIndex);

    ImGuiTreeNodeFlags file_node_flags = ImGuiTreeNodeFlags_NoNavFocus | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesNone;
    if(gft.Selections.Contains(file_desc->PathIndex))
        file_node_flags |= ImGuiTreeNodeFlags_Selected;

    file_node_flags |= ImGuiTreeNodeFlags_FramePadding;

    ImGui::TreeNodeEx(file_name, file_node_flags, "");

    if(gft.LastItemFocused == file_desc->PathIndex)
    {
        gft.LastItemFocusedFrameBB.Min = ImGui::GetItemRectMin();
        gft.LastItemFocusedFrameBB.Max = ImGui::GetItemRectMax();
    }

    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenBlockedByPopup))
    {
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            gft.Selections.ClickedPathIndex = file_desc->PathIndex;
            gft.Selections.ClickedMousePos = g.IO.MousePos;
        }

        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if(has_modifiers)
            {
                gft.Selections.Update(file_desc->PathIndex);
                gft.LastItemFocused = file_desc->PathIndex;
            }
            else
            {
                if(gft.Selections.ClickedPathIndex == file_desc->PathIndex)
                {
                    if(gft.Selections.ClickedMousePos == g.IO.MousePos)
                    {
                        gft.Selections.Update(file_desc->PathIndex);
                        gft.LastItemFocused = file_desc->PathIndex;
                    }
                }
            }

            gft.Selections.ClickedPathIndex = ImStringPool::INVALID_IDX;
        }

        if(ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            if(!gft.Selections.Contains(file_desc->PathIndex))
                gft.Selections.Clear();

            gft.LastItemFocused = file_desc->PathIndex;

            // Reset input state while renaming
            gft.ActionQueue.Type = ImFileTreeActionType_None;
            gft.InputLabelContext.IsActive = false;
            gft.InputLabelContext.State = ImFileTreeInputLabelState_None;
        }
    }

    if(gft.InputLabelContext.State == ImFileTreeInputLabelState_None)
    {
        if(PopupMenu(ImFileTreePopupMenuType_OnFile))
        {
            gft.UpdateDirContentLock = true;
            gft.PopupMenuItemPathIndex = file_desc->PathIndex;
        }

        if(ImGui::BeginDragDropSource())
        {
            if(ImGui::GetDragDropPayload() == nullptr)
            {
                ImVector<int> paths;
                if(!gft.Selections.Contains(gft.Selections.ClickedPathIndex))
                    paths.push_back(file_desc->PathIndex);
                else
                {
                    for(int i = 0; i < gft.Selections.PathsSelected.Size; ++i)
                        paths.push_back(gft.Selections.PathsSelected[i]);
                }

                ImGui::SetDragDropPayload("ImFileTreeDragDrop", paths.Data, (size_t)paths.size_in_bytes());
            }

            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            const int* payload_items = (int*)payload->Data;
            const int payload_count = payload->DataSize / (int)sizeof(int);
            if(payload_count != 0)
            {
                if(payload_count == 1)
                {
                    const char* name = gft.StringPool->at(file_desc->NameIndex);
                    ImGui::Text(name ? name : "?");
                }
                else
                    ImGui::Text("%d", payload_count);
            }

            ImGui::EndDragDropSource();
        }

        if(!ImGui::IsAnyMouseDown() && ImGui::IsItemHovered(ImGuiHoveredFlags_NoSharedDelay | ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", gft.StringPool->at(file_desc->PathIndex));

        if(gft.Selections.GetSingle() == file_desc->PathIndex)
        {
            if(ImGui::IsKeyPressed(ImGuiKey_F2))
                gft.ActionQueue.Type = ImFileTreeActionType_Rename;
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Delete))
            gft.ActionQueue.Type = ImFileTreeActionType_Delete;

        if(g.IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
            gft.ActionQueue.Type = ImFileTreeActionType_Copy;

        if(g.IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X))
            gft.ActionQueue.Type = ImFileTreeActionType_Cut;

        if(gft.ActionQueue.Type == ImFileTreeActionType_Rename)
        {
            gft.UpdateDirContentLock = true;
            gft.InputLabelContext.State = ImFileTreeInputLabelState_Input;
            gft.InputLabelContext.OldPathIndex = file_desc->PathIndex;
            snprintf(gft.InputLabelContext.TextBuffer, IM_COUNTOF(gft.InputLabelContext.TextBuffer), file_name);
        }

        if(ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) || (is_double_click && ImGui::IsItemClicked(ImGuiMouseButton_Left))))
            gft.ActionQueue.Type = ImFileTreeActionType_OpenFile;
    }

    bool apply_alpha_mul = (gft.ActionQueue.IsActionCut && gft.ActionQueue.Contains(file_desc->PathIndex));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(file_desc->Icon.Color, apply_alpha_mul ? 0.5f : 1.0f));
    ImGui::TextUnformatted(file_desc->Icon.Codepoint);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if(gft.InputLabelContext.State == ImFileTreeInputLabelState_Input && gft.InputLabelContext.OldPathIndex == file_desc->PathIndex)
        InputLabel(file_name, file_desc->PathIndex, true);
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text, apply_alpha_mul ? 0.5f : 1.0f));
        ImGui::TextUnformatted(file_name);
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
}

void ImFileTree::RenderRoot(ImFileTreeDirEntry* root_dir_entry, float scan_dir_period, bool sort_content)
{
    ImFileTreeContext& gft = *GImFileTree;

    gft.UpdateDirContentSort = sort_content;

    if(scan_dir_period < 1.0f)
        scan_dir_period = 1.0f;

    ImGuiIO& io = ImGui::GetIO();
    gft.UpdateDirContentTime += io.DeltaTime;
    if(gft.UpdateDirContentForce || gft.UpdateDirContentTime >= scan_dir_period)
    {
        gft.UpdateDirContentTime = 0.0f;
        gft.UpdateDirContentAllow = true;
        gft.UpdateDirContentForce = false;
    }

    if(gft.ActionQueue.IsActionCut && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        gft.ActionQueue.IsActionCut = false;
        gft.ActionQueue.Clear();
    }

    gft.Selections.Begin();
    RenderDir(root_dir_entry);
    gft.Selections.End();
}

//-----------------------------------------------------------------------------
// [SECTION] Context Utils
//-----------------------------------------------------------------------------

#ifndef GImFileTree
ImFileTreeContext* GImFileTree = nullptr;
#endif

ImFileTreeContext::ImFileTreeContext()
{
    StringPool = nullptr;
    IconColorschemeByFilename = nullptr;
    IconColorschemeByFileExtension = nullptr;

    LastItemFocused = ImStringPool::INVALID_IDX;
    PopupMenuItemPathIndex = ImStringPool::INVALID_IDX;

    UpdateDirContentTime = 0.0f;
    UpdateDirContentAllow = false;
    UpdateDirContentForce = false;
    UpdateDirContentLock = false;
    UpdateDirContentSort = false;

    FileOpenCallback = nullptr;
    FileCloseCallback = nullptr;
    FileRenameCallback = nullptr;
}

ImFileTreeContext::~ImFileTreeContext()
{
    Selections.Clear();
    StringPool->clear();
    IM_DELETE(StringPool);
}

ImFileTreeContext* ImFileTree::CreateContext()
{
    ImFileTreeContext* ctx = IM_NEW(ImFileTreeContext)();
    if(GImFileTree == nullptr)
        SetCurrentContext(ctx);
    Initialize(ctx);
    return ctx;
}

void ImFileTree::DestroyContext(ImFileTreeContext* ctx)
{
    if(ctx == nullptr)
        ctx = GetCurrentContext();
    if(GImFileTree == ctx)
        SetCurrentContext(nullptr);
    IM_DELETE(ctx);
}

ImFileTreeContext* ImFileTree::GetCurrentContext()
{
    return GImFileTree;
}

void ImFileTree::SetCurrentContext(ImFileTreeContext* ctx)
{
    GImFileTree = ctx;
}

void ImFileTree::Initialize(ImFileTreeContext* ctx)
{
    ctx->StringPool = IM_NEW(ImStringPool);
    StyleIconColorsDefault();
}

//-----------------------------------------------------------------------------
// [SECTION] End-user API Functions
//-----------------------------------------------------------------------------

void ImFileTree::AddRootPath(const char* root_path)
{
    ImFileTreeContext& gft = *GImFileTree;

    if(root_path == nullptr || root_path[0] == '\0')
        root_path = ROOT_PATH_DEFAULT;

#ifdef _WIN32
    char full_path_buf[MAX_PATH];
    char* full_path = _fullpath(full_path_buf, root_path, MAX_PATH);
#else
    char full_path_buf[PATH_MAX];
    char* full_path = realpath(root_path, full_path_buf);
#endif
    if(!full_path)
        return;

    int full_path_len = (int)strlen(full_path);
    if(full_path[full_path_len - 1] == '\\' || full_path[full_path_len - 1] == '/')
        full_path[full_path_len - 1] = '\0';

    if(!fs_dir_exists(full_path))
        return;

    bool path_is_present = false;

    for(int i = 0; i < gft.RootDirs.Size; i++)
    {
        ImFileTreeDirEntry* root_dir = gft.RootDirs[i];
        if(!root_dir || root_dir->PathIndex == ImStringPool::INVALID_IDX)
            continue;

        const char* root_dir_path = gft.StringPool->at(root_dir->PathIndex);
        if(!root_dir_path)
            continue;

        if(strcmp(root_dir_path, full_path) == 0)
        {
            path_is_present = true;
            break;
        }
    }

    if(path_is_present)
        return;

    ImFileTreeDirEntry* new_root_dir = IM_NEW(ImFileTreeDirEntry);

    char* base_name = path_get_base_name(full_path);

    new_root_dir->NameIndex = gft.StringPool->insert(base_name);
    new_root_dir->PathIndex = gft.StringPool->insert(full_path);
    new_root_dir->IsOpen = true;
    new_root_dir->IsRoot = true;

    if(base_name)
        IM_FREE(base_name);

    UpdateDirContent(new_root_dir, full_path);

    gft.RootDirs.push_back(new_root_dir);
}

void ImFileTree::Render(float scan_dir_period, bool sort_content)
{
    ImFileTreeContext& gft = *GImFileTree;

    if(gft.RootDirs.Size == 0)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f)); // For popups

    for(int i = 0; i < gft.RootDirs.Size; i++)
    {
        ImFileTreeDirEntry* root_dir = gft.RootDirs[i];
        if(!root_dir || root_dir->PathIndex == ImStringPool::INVALID_IDX)
            continue;

        RenderRoot(root_dir, scan_dir_period, sort_content);
    }

    if(gft.LastItemFocused != ImStringPool::INVALID_IDX)
        ImGui::GetWindowDrawList()->AddRect(gft.LastItemFocusedFrameBB.Min, gft.LastItemFocusedFrameBB.Max, ImGui::GetColorU32(ImGuiCol_NavHighlight), 0.0f, 0, 1.0f);

    ImFileTreeDirEntry* last_root_dir = gft.RootDirs[gft.RootDirs.Size - 1];

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if(window->ScrollMax.y == 0.0f)
    {
        ImGui::InvisibleButton("##RootEmptySpace", ImVec2(ImGui::GetContentRegionAvail().x + window->Scroll.x, ImGui::GetContentRegionAvail().y));
        //ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(0, 255, 0, 255), 4.0f, ImDrawListFlags_None, 1.0f);

        if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            gft.LastItemFocused = ImStringPool::INVALID_IDX;
            gft.LastItemFocusedFrameBB.Min = ImVec2(0.0f, 0.0f);
            gft.LastItemFocusedFrameBB.Max = ImVec2(0.0f, 0.0f);
            gft.Selections.Clear();
        }

        if(ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenBlockedByPopup))
        {
            gft.LastItemFocused = last_root_dir->PathIndex;
            gft.Selections.Clear();
        }

        if(PopupMenu(ImFileTreePopupMenuType_OnRoot))
        {
            gft.UpdateDirContentLock = true;
            gft.PopupMenuItemPathIndex = last_root_dir->PathIndex;
        }
    }

    if(gft.InputLabelContext.State == ImFileTreeInputLabelState_Done)
    {
        if(gft.ActionQueue.Type == ImFileTreeActionType_CreateDir)
        {
            const char* new_dir_path = gft.StringPool->at(gft.InputLabelContext.NewPathIndex);
            if(new_dir_path)
            {
                if(!fs_path_exists(new_dir_path))
                    fs_dir_create(new_dir_path);
            }
        }
        else if(gft.ActionQueue.Type == ImFileTreeActionType_CreateFile)
        {
            const char* new_file_path = gft.StringPool->at(gft.InputLabelContext.NewPathIndex);
            if(new_file_path)
            {
                if(!fs_path_exists(new_file_path))
                    fs_file_create(new_file_path);
            }
        }
        else if(gft.ActionQueue.Type == ImFileTreeActionType_Rename)
        {
            const char* old_path = gft.StringPool->at(gft.InputLabelContext.OldPathIndex);
            const char* new_path = gft.StringPool->at(gft.InputLabelContext.NewPathIndex);

            if(old_path && new_path)
            {
                fs_file_rename(old_path, new_path);
                if(gft.FileRenameCallback)
                    gft.FileRenameCallback(old_path, new_path);
            }
        }

        gft.UpdateDirContentForce = true;

        gft.ActionQueue.Type = ImFileTreeActionType_None;
        gft.InputLabelContext.State = ImFileTreeInputLabelState_None;
        gft.InputLabelContext.OldPathIndex = ImStringPool::INVALID_IDX;
        gft.InputLabelContext.NewPathIndex = ImStringPool::INVALID_IDX;
    }
    else if(gft.ActionQueue.Type == ImFileTreeActionType_Cut || gft.ActionQueue.Type == ImFileTreeActionType_Copy)
    {
        UpdateActionQueue();

        gft.ActionQueue.IsActionCut = gft.ActionQueue.Type == ImFileTreeActionType_Cut;
        gft.ActionQueue.Type = ImFileTreeActionType_None;
    }
    else if(gft.ActionQueue.Type == ImFileTreeActionType_Paste)
    {
        const char* dst_path = gft.ActionQueue.DstPath;
        if(dst_path)
        {
            for(int i = 0; i < gft.ActionQueue.SrcPaths.size(); ++i)
            {
                if(fs_path_exists(dst_path))
                {
                    char dst_path_name[MAX_PATH];
                    const char* src_path_name = gft.ActionQueue.SrcPaths[i].PathName;
#ifdef _WIN32
                    wchar_t src_path_name_w[PATH_MAX];
                    str_utf8_to_utf16(src_path_name, src_path_name_w, _countof(src_path_name_w));

                    DWORD attrs = GetFileAttributesW(src_path_name_w);
                    if(attrs == INVALID_FILE_ATTRIBUTES)
                        continue;

                    int is_dir = attrs & FILE_ATTRIBUTE_DIRECTORY;
#else
                    struct stat st;
                    if(stat(src_path_name, &st) != 0)
                        continue;

                    int is_dir = S_ISDIR(st.st_mode);
#endif
                    // Skip deletion of the source path if the destination and source directories match
                    if(gft.ActionQueue.IsActionCut)
                    {
                        char* src_path = path_get_dir_name(src_path_name);
                        if(src_path)
                        {
                            if(strcmp(src_path, dst_path) == 0)
                            {
                                gft.ActionQueue.IsActionCut = false;
                                continue;
                            }
                        }
                    }

                    if(is_dir)
                    {
                        fs_get_unique_name_for_existing_path(src_path_name, dst_path, dst_path_name, IM_COUNTOF(dst_path_name), true);

                        if(dst_path_name[0] != '\0' && fs_dir_copy(src_path_name, dst_path_name) == 0)
                        {
                            if(gft.ActionQueue.IsActionCut)
                                ImFileTreeRemoveDirRecursive(src_path_name);
                        }
                    }
                    else
                    {
                        const char* src_base_name = path_get_base_name_no_alloc(src_path_name);
                        fs_get_unique_name_for_existing_path(src_path_name, dst_path, dst_path_name, IM_COUNTOF(dst_path_name), false);

                        if(dst_path_name[0] != '\0' && fs_file_copy(src_path_name, dst_path_name) == 0)
                        {
                            if(gft.ActionQueue.IsActionCut)
                            {
                                if(fs_file_remove(src_path_name) == 0 && gft.FileCloseCallback)
                                    gft.FileCloseCallback(src_path_name);
                            }
                        }
                    }
                }
            }
        }

        if(gft.ActionQueue.IsActionCut)
        {
            gft.ActionQueue.Clear();
            gft.ActionQueue.IsActionCut = false;
        }

        gft.ActionQueue.Type = ImFileTreeActionType_None;
        gft.UpdateDirContentForce = true;
    }
    else if(gft.ActionQueue.Type == ImFileTreeActionType_OpenFile)
    {
        for(int i = 0; i < gft.Selections.PathsSelected.size(); i++)
        {
            const char* file_path = gft.StringPool->at(gft.Selections.PathsSelected[i]);
            if(gft.FileOpenCallback)
                gft.FileOpenCallback(file_path);
        }

        gft.ActionQueue.Type = ImFileTreeActionType_None;
    }
    else if(gft.ActionQueue.Type == ImFileTreeActionType_Move || gft.ActionQueue.Type == ImFileTreeActionType_Delete)
    {
        gft.ActionQueue.MessageFormat = nullptr;

        if(gft.ActionQueue.Type == ImFileTreeActionType_Delete)
        {
            UpdateActionQueue();
            gft.ActionQueue.IsActionDelete = true;
        }
        else
            gft.ActionQueue.IsActionDelete = false;

        int action_queue_paths_count = gft.ActionQueue.SrcPaths.size();
        if(action_queue_paths_count)
        {
            if(action_queue_paths_count >= 2)
            {
                if(gft.ActionQueue.Type == ImFileTreeActionType_Delete)
                    gft.ActionQueue.MessageFormat = "Are you sure you want to permanently delete the following %d files/folders and their contents?";
                else
                    gft.ActionQueue.MessageFormat = "Are you sure you want to move the following %d files/folders into '%s'?";
            }
            else if(action_queue_paths_count == 1)
            {
                if(gft.ActionQueue.Type == ImFileTreeActionType_Delete)
                {
#ifdef _WIN32
                    wchar_t src_path_name_w[PATH_MAX];
                    str_utf8_to_utf16(gft.ActionQueue.SrcPaths[0].PathName, src_path_name_w, _countof(src_path_name_w));

                    DWORD attrs = GetFileAttributesW(src_path_name_w);
                    int is_dir = attrs & FILE_ATTRIBUTE_DIRECTORY;
#else
                    struct stat st;
                    stat(gft.ActionQueue.SrcPaths[0].PathName, &st);

                    int is_dir = S_ISDIR(st.st_mode);
#endif
                    if(is_dir)
                        gft.ActionQueue.MessageFormat = "Are you sure you want to permanently delete '%s' and their contents?";
                    else
                        gft.ActionQueue.MessageFormat = "Are you sure you want to permanently delete '%s'?";
                }
                else
                    gft.ActionQueue.MessageFormat = "Are you sure you want to move '%s' into '%s'?";
            }

            if(gft.ActionQueue.MessageFormat)
            {
                ImGui::OpenPopup("DeleteOrMoveItems");
                const char* button_name = gft.ActionQueue.IsActionDelete ? "Delete" : "Move";
                ImGui::SetFocusID(ImGui::GetID(button_name), ImGui::GetCurrentWindow());
                ImGui::SetNavCursorVisible(true);
            }
        }

        gft.ActionQueue.Type = ImFileTreeActionType_None;
    }
    else if(gft.ActionQueue.Type == ImFileTreeActionType_CopyPath)
    {
        ImGuiTextBuffer clipboard_buf;
        if(!gft.Selections.PathsSelected.empty())
        {
            for(int i = 0; i < gft.Selections.PathsSelected.size(); ++i)
                clipboard_buf.appendf("%s\n", gft.StringPool->at(gft.Selections.PathsSelected[i]));
        }
        else
            clipboard_buf.appendf("%s\n", gft.StringPool->at(gft.PopupMenuItemPathIndex));

        if(!clipboard_buf.empty())
        {
            clipboard_buf.Buf[clipboard_buf.size() - 1] = '\0';
            ImGui::SetClipboardText(clipboard_buf.c_str());
        }

        gft.ActionQueue.Type = ImFileTreeActionType_None;
    }

    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 30.0f, 0.0f), ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("DeleteOrMoveItems", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        int action_queue_paths_count = gft.ActionQueue.SrcPaths.size();
        if(!action_queue_paths_count)
            ImGui::TextWrapped("Unknown error");
        else if(action_queue_paths_count == 1)
        {
            const char* src_base_name = path_get_base_name_no_alloc(gft.ActionQueue.SrcPaths[0].PathName);
            if(src_base_name)
            {
                if(gft.ActionQueue.IsActionDelete)
                    ImGui::TextWrapped(gft.ActionQueue.MessageFormat, src_base_name);
                else
                {
                    const char* dst_base_name = path_get_base_name_no_alloc(gft.ActionQueue.DstPath);
                    ImGui::TextWrapped(gft.ActionQueue.MessageFormat, src_base_name, dst_base_name);
                }
            }
        }
        else if(action_queue_paths_count >= 2)
        {
            if(gft.ActionQueue.IsActionDelete)
                ImGui::TextWrapped(gft.ActionQueue.MessageFormat, gft.ActionQueue.SrcPaths.size());
            else
            {
                const char* dst_base_name = path_get_base_name_no_alloc(gft.ActionQueue.DstPath);
                ImGui::TextWrapped(gft.ActionQueue.MessageFormat, gft.ActionQueue.SrcPaths.size(), dst_base_name);
            }

            int items_total = gft.ActionQueue.SrcPaths.size();
            int items_show_max = ImMin(items_total, 10);
            int items_additional = items_total - items_show_max;

            ImGui::Indent(10.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextLink));

            for(int i = 0; i < items_show_max; ++i)
            {
                const char* base_name = path_get_base_name_no_alloc(gft.ActionQueue.SrcPaths[i].PathName);
                if(base_name)
                    ImGui::TextWrapped(base_name);
            }

            if(items_additional)
                ImGui::TextWrapped("...%d additional file(s) not shown", items_additional);

            ImGui::PopStyleColor();
            ImGui::Unindent(10.0f);
        }

        ImGui::Separator();

        const float button_width = 120.0f;
        const float total_buttons_width = (button_width + ImGui::GetStyle().ItemSpacing.x) * (action_queue_paths_count ? 2.0f : 1.0f) - ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - total_buttons_width);

        if(action_queue_paths_count)
        {
            const char* button_name = gft.ActionQueue.IsActionDelete ? "Delete" : "Move";

            if(ImGui::Button(button_name, ImVec2(button_width, 0.0f)))
            {
                for(int i = 0; i < gft.ActionQueue.SrcPaths.size(); ++i)
                {
                    const char* src_path_name = gft.ActionQueue.SrcPaths[i].PathName;
#ifdef _WIN32
                    wchar_t src_path_name_w[PATH_MAX];
                    str_utf8_to_utf16(src_path_name, src_path_name_w, _countof(src_path_name_w));

                    DWORD attrs = GetFileAttributesW(src_path_name_w);
                    if(attrs == INVALID_FILE_ATTRIBUTES)
                        continue;

                    int is_dir = attrs & FILE_ATTRIBUTE_DIRECTORY;
#else
                    struct stat st;
                    if(stat(src_path_name, &st) != 0)
                        continue;

                    int is_dir = S_ISDIR(st.st_mode);
#endif
                    if(gft.ActionQueue.IsActionDelete)
                    {
                        if(is_dir)
                            ImFileTreeRemoveDirRecursive(src_path_name);
                        else
                        {
                            if(fs_file_remove(src_path_name) == 0 && gft.FileCloseCallback)
                                gft.FileCloseCallback(src_path_name);
                        }
                    }
                    else
                    {
                        const char* src_base_name = path_get_base_name_no_alloc(src_path_name);

                        char dst_path_name[PATH_MAX];
                        int len = snprintf(dst_path_name, sizeof(dst_path_name), "%s" DIR_SEPARATOR "%s", gft.ActionQueue.DstPath, src_base_name);
                        if(len < (int)sizeof(dst_path_name))
                        {
                            if(!fs_path_exists(dst_path_name))
                            {
                                if(is_dir)
                                    fs_dir_move(src_path_name, dst_path_name);
                                else
                                    fs_file_move(src_path_name, dst_path_name);
                            }
                        }
                    }
                }

                gft.ActionQueue.Clear();
                gft.ActionQueue.IsActionDelete = false;
                gft.UpdateDirContentForce = true;

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
        }

        if(ImGui::Button("Cancel", ImVec2(button_width, 0.0f)))
            ImGui::CloseCurrentPopup();

        if(ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if(g.OpenPopupStack.Size == 0 && gft.InputLabelContext.State == ImFileTreeInputLabelState_None)
        gft.UpdateDirContentLock = false;

    gft.UpdateDirContentAllow = false;

    ImGui::PopStyleVar(2);
}

void ImFileTree::StyleIconColorsDefault()
{
    ImFileTreeContext& gft = *GImFileTree;

    gft.IconColorschemeByFilename = icon_map_by_filename_nvim_web_devicons_default;
    gft.IconColorschemeByFileExtension = icon_map_by_file_ext_nvim_web_devicons_default;
}

void ImFileTree::StyleIconColorsLight()
{
    ImFileTreeContext& gft = *GImFileTree;

    gft.IconColorschemeByFilename = icon_map_by_filename_nvim_web_devicons_light;
    gft.IconColorschemeByFileExtension = icon_map_by_file_ext_nvim_web_devicons_light;
}
#endif // #ifndef IMGUI_DISABLE
