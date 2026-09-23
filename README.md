# About
An implementation of file tree for [Dear ImGui](https://github.com/ocornut/imgui) with [Nerd Font](https://www.nerdfonts.com/) icons (glyphs) support.

# Features
- Support for multiple root directories with recursive display of their contents
- Basic operations on directories and files via the context menu:
    - Create and Delete
    - Copy, Cut and Paste
    - Rename
    - Copy Path
- Several hotkeys for basic operations
- Multi-Select feature throw CTRL + Mouse Click and SHIFT + Mouse Click
- Drag-And-Drop feature
- Colored icons for directories and file types

<img width="675" height="577" alt="imfiletree_popup_menu" src="https://github.com/user-attachments/assets/3602c3c2-3d1e-4bcb-b002-c45459684219" />
<img width="675" height="577" alt="imfiletree_popup_doalog" src="https://github.com/user-attachments/assets/e2d1ac27-33ce-4e25-bb75-fcde43ff5808" />

# Dependencies
- Vanilla ImGui 1.92+
- Nerd Font
- C++14 or later

# Integration
1) Set up an [Dear ImGui](https://github.com/ocornut/imgui) environment in your project.
2) Add files 'imfiletree.h', 'imfiletree_internal.h', 'imfiletree_colorschemes.h', 'imfiletree.cpp', 'imfiletree_colorschemes.cpp' and directory 'misc' with all its contents to your sources.
3) Add an additional dependency 'imstring.h' and 'imstring.cpp' from [ImString](https://github.com/VoxMi/ImString) repository to your sources.
4) Uncomment the '#define IMGUI_USE_WCHAR32' in the 'imconfig.h' file to use the full set of icons from Nerd Fonts.
5) Download one of the TTF file from [Nerd Font](https://www.nerdfonts.com/font-downloads/) site and use it in your project:
```cpp
ImGuiIO& io = ImGui::GetIO();
io.Fonts->AddFontFromFileTTF("./resources/fonts/DejaVuSansMNerdFont-Regular.ttf", 16.0f);
```
6) Create and destroy an `ImFileTree` wherever you do so for your `ImGuiContext`:
```cpp
ImGui::CreateContext();
ImFileTree::CreateContext();
...
ImFileTree::DestroyContext();
ImGui::DestroyContext();
```
7) Setup callbacks:
```cpp
ImFileTreeContext& gft = *ImFileTree::GetCurrentContext();
gft.FileOpenCallback = YourFunctionFileOpen;
gft.FileCloseCallback = YourFunctionFileClose;
gft.FileRenameCallback = YourFunctionFileRename;
```
8) Add one or more different root directories (recommended to be called once before the main rendering loop):
```cpp
ImFileTree::AddRootPath("C:\\path_to_dir_one");
ImFileTree::AddRootPath("C:\\path_to_dir_two");
```
9) Render file tree inside application main loop:
```cpp
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
if(ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar))
    ImFileTree::Render();
ImGui::End();
ImGui::PopStyleVar();
```
