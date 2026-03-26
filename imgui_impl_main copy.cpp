// Dear ImGui: standalone example application for Windows API + DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp
#define NOMINMAX
#include "overlay.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <memory>
#include <chrono>
#include <tchar.h>
#include <Uxtheme.h>
#include <dwmapi.h>
#include "overlay.h"
#include "ui.h"
#include "cheat.h"
// Data
static ID3D11Device *g_pd3dDevice = nullptr;
static ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
static IDXGISwapChain *g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Simple helper function to load an image into a DX11 texture with common settings
bool LoadTextureFromMemory(const void *data, size_t data_size, ID3D11ShaderResourceView **out_srv, int *out_width,
                           int *out_height) {
    // Load from disk into a raw RGBA buffer
    int image_width = 0;
    int image_height = 0;
    unsigned char *image_data =
        stbi_load_from_memory((const unsigned char *)data, (int)data_size, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D *pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
    pTexture->Release();

    *out_width = image_width;
    *out_height = image_height;
    stbi_image_free(image_data);

    return true;
}

// Open and read a file, then forward to LoadTextureFromMemory()
bool LoadTextureFromFile(const char *file_name, ID3D11ShaderResourceView **out_srv, int *out_width, int *out_height) {
    FILE *f = fopen(file_name, "rb");
    if (f == NULL)
        return false;
    fseek(f, 0, SEEK_END);
    size_t file_size = (size_t)ftell(f);
    if (file_size == -1)
        return false;
    fseek(f, 0, SEEK_SET);
    void *file_data = IM_ALLOC(file_size);
    fread(file_data, 1, file_size, f);
    fclose(f);
    bool ret = LoadTextureFromMemory(file_data, file_size, out_srv, out_width, out_height);
    IM_FREE(file_data);
    return ret;
}

auto loadFont() {
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    const ImWchar charRanges[] = {
        0x0020, 0x00FF, 0xE000, 0xE226, 0,
    };
    builder.AddRanges(charRanges);
    builder.BuildRanges(&ranges);

    ImGui::GetIO().Fonts->AddFontFromFileTTF("assets/fonts/JetBrainsMonoNerdFont-Regular.ttf",
                                             18.f * ImGui::GetStyle().FontScaleDpi, nullptr, ranges.Data);
    ImFontConfig config;
    config.MergeMode = true; // 关键：设置为合并模式,将中文和表情字体合并
    config.PixelSnapH = true;
    // 增强字体粗细 / 对比度
    config.RasterizerMultiply = 1.2f;
    ImGui::GetIO().Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\msyh.ttc)", 18.0f * ImGui::GetStyle().FontScaleDpi,
                                             &config, ImGui::GetIO().Fonts->GetGlyphRangesChineseSimplifiedCommon());
    ImGui::GetIO().Fonts->Build();
}
ImFont *NotificationFont1;
ImFont *NotificationFont2;
auto loadBigFont() {
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    const ImWchar charRanges[] = {
        0x0020, 0x00FF, 0xE000, 0xE226, 0,
    };
    builder.AddRanges(charRanges);
    builder.BuildRanges(&ranges);

    NotificationFont1 =
        ImGui::GetIO().Fonts->AddFontFromFileTTF("assets/fonts/JetBrainsMonoNerdFont-Regular.ttf",
                                                 18.f * 3 * ImGui::GetStyle().FontScaleDpi, nullptr, ranges.Data);
    ImFontConfig config;
    config.MergeMode = true; // 关键：设置为合并模式,将中文和表情字体合并
    config.PixelSnapH = true;
    // 增强字体粗细 / 对比度
    config.RasterizerMultiply = 1.2f;
    NotificationFont2 = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        R"(C:\Windows\Fonts\msyh.ttc)", 18.0f * 3 * ImGui::GetStyle().FontScaleDpi, &config,
        ImGui::GetIO().Fonts->GetGlyphRangesChineseSimplifiedCommon());
}

namespace language {
    enum class Language { EN, ZH, COUNT };

    inline Language current = Language::ZH;

    namespace key {
        enum Enum { Menu_Cheat, Menu_Visuals, Cheat_GodMode, Menu_Language, COUNT };
    }

    // 每个翻译项都有一块可修改的缓冲区
    // 容量要足够放下最长的翻译（通常中文/俄文/德文会比较长）
    struct Translatable {
        static constexpr size_t MAX_LEN = 27; // 够用即可，含 \0

        char8_t data[(int)Language::COUNT][MAX_LEN + 1]{};

        const char8_t *get() const { return data[(int)current]; }

        // 可选：初始化时填充默认值
        void init(const char *en, const char *zh) {
            strncpy((char *)data[(int)Language::EN], en, MAX_LEN);
            strncpy((char *)data[(int)Language::ZH], zh, MAX_LEN);
            data[(int)Language::EN][MAX_LEN] = '\0';
            data[(int)Language::ZH][MAX_LEN] = '\0';
        }
    };

    // 所有翻译项都定义为 static 变量
    inline Translatable t_Menu_Cheat{.data = {u8"Cheat", u8"作弊"}};
    inline Translatable t_Menu_Visuals{.data = {u8"Visuals", u8"视觉"}};
    inline Translatable t_Cheat_GodMode{.data = {u8"God Mode", u8"无敌"}};
    inline Translatable t_Menu_Language{.data = {u8"Language", u8"语言"}};

// 宏保持兼容性
#define TR(k) language::t_##k.get()
} // namespace language

struct KeyBinding {
    int vkKey;
    std::function<void(ImGuiIO &, bool)> handler;
};

class InputManager {
  private:
    // 存储所有按键状态
    std::map<int, bool> keyStates;
    HWND hwnd;

    // 按键到字符的映射
    static constexpr std::array<std::pair<int, std::pair<char, char>>, 10> specialKeyMap{{
        {VK_OEM_1, {';', ':'}},
        {VK_OEM_PLUS, {'=', '+'}},
        {VK_OEM_COMMA, {',', '<'}},
        {VK_OEM_MINUS, {'-', '_'}},
        {VK_OEM_PERIOD, {'.', '>'}},
        {VK_OEM_2, {'/', '?'}},
        {VK_OEM_3, {'`', '~'}},
        {VK_OEM_4, {'[', '{'}},
        {VK_OEM_5, {'\\', '|'}},
        {VK_OEM_6, {']', '}'}},
    }};

    static constexpr std::array<std::pair<int, ImGuiKey>, 4> specialKeys{{
        {VK_BACK, ImGuiKey_Backspace},
        {VK_RETURN, ImGuiKey_Enter},
        {VK_TAB, ImGuiKey_Tab},
        {VK_DELETE, ImGuiKey_Delete},
    }};

    static constexpr std::array<std::pair<int, ImGuiKey>, 4> arrowKeys{{
        {VK_UP, ImGuiKey_UpArrow},
        {VK_DOWN, ImGuiKey_DownArrow},
        {VK_LEFT, ImGuiKey_LeftArrow},
        {VK_RIGHT, ImGuiKey_RightArrow},
    }};

  public:
    InputManager(HWND hwnd) : hwnd(hwnd) {};

    // 统一的按键处理函数
    void processKeyPress(int vkKey, ImGuiIO &io) {
        bool &toggleState = keyStates[vkKey];
        bool keyDown = (GetAsyncKeyState(vkKey) & 0x8000) != 0;
        bool shiftHeld = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000);

        // 按键按下事件
        if (!toggleState && keyDown) {
            handleKeyDown(vkKey, shiftHeld, io);
            toggleState = true;
        }
        // 按键释放事件
        else if (toggleState && !keyDown) {
            handleKeyUp(vkKey, io);
            toggleState = false;
        }
    }

    void update(ImGuiIO &io) {
        // 处理字母键 A-Z
        for (char c : std::views::iota('A', 'Z' + 1)) {
            processKeyPress(c, io);
        }

        // 处理数字键 0-9
        for (char c : std::views::iota('0', '9' + 1)) {
            processKeyPress(c, io);
        }

        // 处理鼠标按键
        processMouseButton(VK_LBUTTON, ImGuiMouseButton_Left, io);
        processMouseButton(VK_RBUTTON, ImGuiMouseButton_Right, io);
        processMouseButton(VK_MBUTTON, ImGuiMouseButton_Middle, io);

        // 处理控制键
        // processControlKey(VK_LCONTROL, ImGuiKey_LeftCtrl, ImGuiMod_Ctrl, io);
        // processControlKey(VK_LSHIFT, ImGuiKey_LeftShift, ImGuiMod_Shift, io);

        // 处理功能键
        for (const auto &[vkKey, imguiKey] : specialKeys) {
            processSpecialKey(vkKey, imguiKey, io);
        }

        // 处理方向键
        for (const auto &[vkKey, imguiKey] : arrowKeys) {
            processSpecialKey(vkKey, imguiKey, io);
        }

        // 处理特殊字符键
        for (const auto &[vkKey, chars] : specialKeyMap) {
            processKeyPress(vkKey, io);
        }

        // 处理空格
        processKeyPress(VK_SPACE, io);
    }

  private:
    bool isKeyDown(int vkKey) const { return (GetAsyncKeyState(vkKey) & 0x8000) != 0; }

    bool isShiftHeld() const {
        return (GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000);
    }

    char getCharFromKey(int vkKey, bool shiftHeld) const {
        // 字母键
        if (vkKey >= 'A' && vkKey <= 'Z') {
            return shiftHeld ? (char)vkKey : (char)(vkKey + 32);
        }

        // 特殊字符键
        for (const auto &[key, chars] : specialKeyMap) {
            if (key == vkKey) {
                return shiftHeld ? chars.second : chars.first;
            }
        }

        // 空格
        if (vkKey == VK_SPACE) {
            return ' ';
        }

        return 0;
    }

    void handleKeyDown(int vkKey, bool shiftHeld, ImGuiIO &io) {
        if (char charCode = getCharFromKey(vkKey, shiftHeld)) {
            io.AddInputCharacter(charCode);
        }
    }

    void handleKeyUp(int vkKey, ImGuiIO &io) {
        // 按键释放时的处理
    }

    void processMouseButton(int vkKey, ImGuiMouseButton_ imguiButton, ImGuiIO &io) {
        bool &toggleState = keyStates[vkKey];
        bool keyDown = isKeyDown(vkKey);

        if (!toggleState && keyDown) {
            io.AddMouseButtonEvent(imguiButton, true);
            toggleState = true;
        } else if (toggleState && !keyDown) {
            io.AddMouseButtonEvent(imguiButton, false);
            toggleState = false;
        }
    }

    void processControlKey(int vkKey, ImGuiKey imguiKey, ImGuiIO &io) {
        bool &toggleState = keyStates[vkKey];
        bool keyDown = isKeyDown(vkKey);

        if (!toggleState && keyDown) {
            io.AddKeyEvent(imguiKey, true);
            if (auto *keyData = ImGui::GetKeyData(imguiKey)) {
                keyData->Down = true;
            }
            toggleState = true;
        } else if (toggleState && !keyDown) {
            io.AddKeyEvent(imguiKey, false);
            if (auto *keyData = ImGui::GetKeyData(imguiKey)) {
                keyData->Down = false;
            }
            toggleState = false;
        }
    }

    void processSpecialKey(int vkKey, ImGuiKey imguiKey, ImGuiIO &io) {
        bool &toggleState = keyStates[vkKey];
        bool keyDown = isKeyDown(vkKey);

        if (!toggleState && keyDown) {
            io.AddKeyEvent(imguiKey, true);
            toggleState = true;
        } else if (toggleState && !keyDown) {
            io.AddKeyEvent(imguiKey, false);
            toggleState = false;
        }
    }
};
static WNDCLASSEXW wc;
auto createOverlayHWND() -> HWND {
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    // Create application window
    wc = {sizeof(wc), CS_HREDRAW | CS_VREDRAW,
          WndProc,    0L,
          0L,         GetModuleHandle(nullptr),
          nullptr,    nullptr,
          nullptr,    nullptr,
          L"class04", nullptr};

    WNDCLASSEXW windowClass = wc;
    windowClass.hInstance = GetModuleHandle(nullptr);
    windowClass.lpszClassName = L"class04";

    auto classAtom = ::RegisterClassExW(&windowClass);
    using CreateWindowInBandFunc =
        HWND(WINAPI *)(DWORD, ATOM, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID, DWORD);

    // HWND hwnd =
    //     reinterpret_cast<CreateWindowInBandFunc>(GetProcAddress(GetModuleHandleW(L"user32.dll"),
    //     "CreateWindowInBand"))(
    //         WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, classAtom, wc.lpszClassName,
    //         WS_POPUP, 0, 0, screenWidth, screenHeight, nullptr, nullptr, wc.hInstance, nullptr, 2);
    HWND hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
                                  wc.lpszClassName, L"classui04", WS_POPUP, 0, 0, screenWidth, screenHeight, nullptr,
                                  nullptr, wc.hInstance, nullptr);
    return hwnd;
}

#ifdef USE_VAINGLORY
#else
#endif

// Main code
int overlay(std::optional<RequestContext> request, HWND hwnd) {

    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    /*
    WS_POPUP 表示：
    透明

    窗口 没有标题栏

    窗口 没有边框

    可以自由放置在屏幕上

    常用于 Overlay、游戏 HUD、工具提示、弹出菜单
    */

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // 让窗口不被屏幕捕获 / 录屏 / 截图捕捉到
    //  Drv->HideWindow(hwnd, WDA_EXCLUDEFROMCAPTURE);
    //  HideWindow(hwnd, WDA_EXCLUDEFROMCAPTURE);

    // Extend frame for visual transparency
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    // 让窗口客户区使用 Aero Glass / DWM 透明效果
    //  Set alpha to 255 (fully opaque)
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    // io.ConfigDockingAlwaysTabBar = true;
    // io.ConfigDockingTransparentPayload = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling,
                                     // changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale; // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true
                                     // automatically overrides this for every window depending on the current monitor)
    io.ConfigDpiScaleFonts = true; // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor
                                   // DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true; // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular
    // ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or
    // AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small
    //   threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code
    // (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double
    // backslash \\ ! style.FontSizeBase = 20.0f; io.Fonts->AddFontDefaultVector(); io.Fonts->AddFontDefaultBitmap();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    // IM_ASSERT(font != nullptr);
    loadFont();
    loadBigFont();
    ImVeil::setupImVeilTheme();

    LoadTextureFromFile("assets/logo_129_43.png", (ID3D11ShaderResourceView **)&logoTexture, &logoWidth, &logoHeight);
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0, 0, 0, 0);

    OverlayContext overlayContext{.hwnd = hwnd};

    ImVeil::UiNavigation ui;

    auto &view = ui.addMainItem(TR(Menu_Cheat), u8"\ueaa4");
    view.addSubItem(u8"逃离鸭科夫", [](void *userData) {
        // ImVeil::BeginRegion(ctx, "角色", ImVec2(260, 300));
        bool s;
        ImVeil::toggleSwitch("无敌", &s);
        ImVeil::toggleSwitch("无限冲刺", &s);
        ImVeil::toggleSwitch("无限耐力", &s);
        ImVeil::toggleSwitch("玩家隐身 中立单位", &s);
    });
    view.addSubItem(u8"逃离鸭科夫1", [](void *userData) {
        // ImVeil::BeginRegion(ctx, "角色", ImVec2(260, 300));
        bool s;
        ImVeil::toggleSwitch("无敌", &s);
        ImVeil::toggleSwitch("无限冲刺", &s);
        ImVeil::toggleSwitch("无限耐力", &s);
        ImVeil::toggleSwitch("玩家隐身 中立单位", &s);
    });
    auto &view1 = ui.addMainItem(TR(Menu_Language), u8"\ueb01");
    view1.addSubItem(u8"设置", [](void *userData) {
        // ImVeil::BeginRegion(ctx, "角色", ImVec2(260, 300));
        static bool zh = true;
        static bool en = false;
        if (ImVeil::toggleSwitch("English", &en)) {
            if (en == true) {
                zh = false;
            }
        }
        if (ImVeil::toggleSwitch("中文", &zh)) {
            if (zh == true) {
                en = false;
            }
        };
    });
    ImVeil::notifications::post("启动成功");
    ImVeil::notifications::post("按下Tab + W打开菜单");
    ImVeil::notifications::post("按下Tab + Q退出作弊");

    cheat::SnapshotManager snapshots;
    auto reader = std::make_unique<MemoryReaderAdapter<WinProcessMemoryReader>>(pid);
    std::jthread jt(cheat::readTask, std::ref(snapshots), std::move(reader));
    InputManager inputManager(hwnd);
    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) {
            jt.request_stop();
            break;
        }

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (overlayContext.mainLoop()) {
            done = true;
        }

        ImGui::PushFont(NotificationFont1);
        ImGui::PushFont(NotificationFont2);
        ImVeil::notifications::rendernotifications();
        ImGui::PopFont();
        ImGui::PopFont();
        if (overlayContext.isMenuVisible) {
            inputManager.update(ImGui::GetIO());
            ui.render(nullptr);
        } else {
            ImGui::GetIO().ClearInputKeys();
        }
        const auto *currentSnap = snapshots.tryGetLatst();
        if (currentSnap) {
            cheat::render(*currentSnap);
        }
        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                                                 clear_color.z * clear_color.w, clear_color.w};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0); // Present with vsync
        // HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd) {
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode
    // differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                                                featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                                &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
                                            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
                                            &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    // Disable DXGI's default Alt+Enter fullscreen behavior.
    // - You are free to leave this enabled, but it will not work properly with multiple viewports.
    // - This must be done for all windows associated to the device. Our DX11 backend does this automatically for
    // secondary viewports that it creates.
    IDXGIFactory *pSwapChainFactory;
    if (SUCCEEDED(g_pSwapChain->GetParent(IID_PPV_ARGS(&pSwapChainFactory)))) {
        pSwapChainFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
        pSwapChainFactory->Release();
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget() {
    ID3D11Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite
// your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
// clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and hide them
// from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
