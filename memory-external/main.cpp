#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <d3d9.h>

#define STB_IMAGE_IMPLEMENTATION
#include "classes/stb_image.h"

#include "classes/utils.h"
#include "memory/memory.hpp"
#include "classes/vector.hpp"
#include "hacks/reader.hpp"
#include "hacks/hack.hpp"
#include "classes/globals.hpp"
#include "classes/render.hpp"
#include "classes/auto_updater.hpp"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")

bool finish = false;
bool showMenu = true;

LPDIRECT3D9       g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pDevice = NULL;
HWND              g_hMenu = NULL;

#define OVERLAY_TIMER_ID 1
#define OVERLAY_FPS 60

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool LoadTextureFromFileDX9(IDirect3DDevice9* device, const char* filename, IDirect3DTexture9** out_texture, int* out_width, int* out_height)
{
    int x, y, n;
    unsigned char* data = stbi_load(filename, &x, &y, &n, 4);
    if (!data) return false;
    *out_width = x;
    *out_height = y;

    if (FAILED(device->CreateTexture(x, y, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, out_texture, NULL)))
    {
        stbi_image_free(data);
        return false;
    }

    D3DLOCKED_RECT lockedRect;
    if (FAILED((*out_texture)->LockRect(0, &lockedRect, NULL, 0)))
    {
        (*out_texture)->Release();
        stbi_image_free(data);
        return false;
    }
    unsigned char* dest = (unsigned char*)lockedRect.pBits;
    for (int j = 0; j < y; ++j)
        memcpy(dest + j * lockedRect.Pitch, data + j * x * 4, x * 4);

    (*out_texture)->UnlockRect(0);
    stbi_image_free(data);
    return true;
}

inline bool ImGuiSwitch(const char* label, bool* v)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float height = ImGui::GetFrameHeight();
    float width = height * 1.60f;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton(label, ImVec2(width, height));
    bool clicked = ImGui::IsItemClicked();
    if (clicked) *v = !*v;
    float t = *v ? 1.0f : 0.0f;
    ImU32 track_color = *v ? IM_COL32(255, 0, 128, 255) : IM_COL32(60, 60, 60, 220);
    ImU32 knob_color = IM_COL32(255, 255, 255, 255);

    draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), track_color, height * 0.49f);
    draw_list->AddCircleFilled(ImVec2(p.x + height * 0.5f + t * (width - height), p.y + height * 0.5f), height * 0.38f, knob_color, 20);

    return clicked;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g::hdcBuffer = CreateCompatibleDC(NULL);
        g::hbmBuffer = CreateCompatibleBitmap(GetDC(hWnd), g::gameBounds.right, g::gameBounds.bottom);
        SelectObject(g::hdcBuffer, g::hbmBuffer);
        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);

        SetTimer(hWnd, OVERLAY_TIMER_ID, 1000 / OVERLAY_FPS, NULL);
        std::cout << "[overlay] Window created successfully" << std::endl;
        Beep(500, 100);
        break;
    }
    case WM_TIMER:
    {
        if (wParam == OVERLAY_TIMER_ID) {
            HWND fg = GetForegroundWindow();
            if (fg == g_game.process->hwnd_ || fg == g_hMenu) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        break;
    }
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        FillRect(g::hdcBuffer, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));

        HWND fg = GetForegroundWindow();
        if (fg == g_game.process->hwnd_ || fg == g_hMenu)
            hack::loop();

        BitBlt(hdc, 0, 0, g::gameBounds.right, g::gameBounds.bottom, g::hdcBuffer, 0, 0, SRCCOPY);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY:
        KillTimer(hWnd, OVERLAY_TIMER_ID);
        DeleteDC(g::hdcBuffer);
        DeleteObject(g::hbmBuffer);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool InitD3D(HWND hWnd, int w, int h)
{
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_pD3D) return false;
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.hDeviceWindow = hWnd;
    pp.BackBufferWidth = w;
    pp.BackBufferHeight = h;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    return SUCCEEDED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &g_pDevice));
}

void menu_thread()
{
    const int MW = 720;
    const int MH = 520;

    static IDirect3DTexture9* my_logo = nullptr;
    static int logo_width = 0, logo_height = 0;
    static bool logo_loaded = false;

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MenuWndProc, 0, 0,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DexterMenuClass", NULL };
    RegisterClassEx(&wc);

    g_hMenu = CreateWindowEx(
        WS_EX_TOPMOST,
        "DexterMenuClass", "Dexter MENU",
        WS_POPUP | WS_VISIBLE,
        1920 - MW, 540 - MH / 2,
        MW, MH,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    DWORD winStyle = GetClassLong(g_hMenu, GCL_STYLE);
    SetClassLong(g_hMenu, GCL_STYLE, winStyle & ~CS_DROPSHADOW);

    if (!InitD3D(g_hMenu, MW, MH)) return;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;

    ImFont* fontDefault = io.Fonts->AddFontDefault();

    ImGuiStyle& imguiStyle = ImGui::GetStyle();
    imguiStyle.WindowRounding = 12.0f;
    imguiStyle.FrameRounding = 6.0f;
    imguiStyle.ItemSpacing = ImVec2(10, 8);
    imguiStyle.WindowPadding = ImVec2(15, 15);
    imguiStyle.FramePadding = ImVec2(8, 6);

    imguiStyle.Colors[ImGuiCol_WindowBg] = ImColor(15, 15, 15, 245);
    imguiStyle.Colors[ImGuiCol_ChildBg] = ImColor(25, 18, 22, 200);
    imguiStyle.Colors[ImGuiCol_PopupBg] = ImColor(20, 15, 20, 240);

    imguiStyle.Colors[ImGuiCol_Border] = ImColor(255, 0, 128, 180);
    imguiStyle.Colors[ImGuiCol_BorderShadow] = ImColor(150, 0, 80, 80);

    imguiStyle.Colors[ImGuiCol_FrameBg] = ImColor(30, 22, 28, 200);
    imguiStyle.Colors[ImGuiCol_FrameBgHovered] = ImColor(50, 30, 45, 230);
    imguiStyle.Colors[ImGuiCol_FrameBgActive] = ImColor(60, 35, 55, 240);

    imguiStyle.Colors[ImGuiCol_CheckMark] = ImColor(255, 0, 128, 255);
    imguiStyle.Colors[ImGuiCol_SliderGrab] = ImColor(255, 0, 128, 255);
    imguiStyle.Colors[ImGuiCol_SliderGrabActive] = ImColor(255, 50, 150, 255);

    imguiStyle.Colors[ImGuiCol_Button] = ImColor(40, 28, 38, 200);
    imguiStyle.Colors[ImGuiCol_ButtonHovered] = ImColor(100, 40, 80, 230);
    imguiStyle.Colors[ImGuiCol_ButtonActive] = ImColor(120, 50, 100, 250);

    imguiStyle.Colors[ImGuiCol_Header] = ImColor(60, 35, 50, 200);
    imguiStyle.Colors[ImGuiCol_HeaderHovered] = ImColor(100, 50, 80, 230);
    imguiStyle.Colors[ImGuiCol_HeaderActive] = ImColor(120, 60, 100, 250);

    imguiStyle.Colors[ImGuiCol_Tab] = ImColor(35, 25, 32, 200);
    imguiStyle.Colors[ImGuiCol_TabHovered] = ImColor(80, 40, 65, 230);
    imguiStyle.Colors[ImGuiCol_TabActive] = ImColor(100, 50, 80, 250);
    imguiStyle.Colors[ImGuiCol_TabUnfocused] = ImColor(30, 22, 28, 150);
    imguiStyle.Colors[ImGuiCol_TabUnfocusedActive] = ImColor(60, 35, 50, 200);

    imguiStyle.Colors[ImGuiCol_TitleBg] = ImColor(10, 10, 10, 255);
    imguiStyle.Colors[ImGuiCol_TitleBgActive] = ImColor(25, 15, 22, 255);
    imguiStyle.Colors[ImGuiCol_TitleBgCollapsed] = ImColor(10, 10, 10, 200);

    imguiStyle.Colors[ImGuiCol_Separator] = ImColor(255, 0, 128, 150);
    imguiStyle.Colors[ImGuiCol_SeparatorHovered] = ImColor(255, 0, 128, 200);
    imguiStyle.Colors[ImGuiCol_SeparatorActive] = ImColor(255, 0, 128, 255);

    imguiStyle.Colors[ImGuiCol_ScrollbarBg] = ImColor(15, 15, 15, 200);
    imguiStyle.Colors[ImGuiCol_ScrollbarGrab] = ImColor(100, 50, 80, 200);
    imguiStyle.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(120, 60, 100, 230);
    imguiStyle.Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(140, 70, 120, 250);

    imguiStyle.Colors[ImGuiCol_Text] = ImColor(200, 200, 200, 255);
    imguiStyle.Colors[ImGuiCol_TextDisabled] = ImColor(100, 100, 100, 200);

    ImGui_ImplWin32_Init(g_hMenu);
    ImGui_ImplDX9_Init(g_pDevice);

    ShowWindow(g_hMenu, SW_SHOW);

    if (!logo_loaded && g_pDevice)
    {
        const char* logoPath = "dexterlogo.png";
        logo_loaded = LoadTextureFromFileDX9(g_pDevice, logoPath, &my_logo, &logo_width, &logo_height);
        if (!logo_loaded) {
            std::cout << "[LOGO] Falha ao carregar a logo: " << logoPath << std::endl;
        }
    }

    static int tab_id = 0;
    bool menu_show_box_esp = false;
    bool menu_show_skeleton_esp = false;
    bool menu_show_box_esp_team = false;
    bool menu_show_skeleton_esp_team = false;
    bool menu_show_head_tracker = false;
    bool menu_show_console = false;
    bool menu_streamproof = false;
    bool menu_show_health_bar = false;
    bool menu_show_armor_bar = false;
    bool menu_show_extra_flags = false;
    bool menu_show_flag_name = false;
    bool menu_show_flag_health = false;
    bool menu_show_flag_armor = false;
    bool menu_show_flag_weapon = false;
    bool menu_show_flag_weapon_icon = false;
    bool menu_show_flag_distance = false;
    bool menu_show_flag_money = false;
    bool menu_show_flag_flashed = false;
    bool menu_show_flag_defusing = false;
    float colEnemy[3] = { 1.0f, 0.0f, 0.502f };
    float colTeam[3] = { 0.0f, 1.0f, 0.196f };

    MSG msg;
    static bool lastInsertState = false;

    while (!finish)
    {
        while (PeekMessage(&msg, g_hMenu, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        bool insertPressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insertPressed && !lastInsertState) {
            showMenu = !showMenu;
            if (showMenu) {
                ShowWindow(g_hMenu, SW_SHOW);
            }
            else {
                ShowWindow(g_hMenu, SW_HIDE);
            }
        }
        lastInsertState = insertPressed;

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (showMenu)
        {
            static bool window_initialized = false;
            if (!window_initialized) {
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
                ImGui::SetNextWindowSize(ImVec2((float)MW, (float)MH), ImGuiCond_Once);
                window_initialized = true;
            }

            ImGui::Begin("##main", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

            ImGui::BeginGroup();
            {
                ImGui::BeginChild("sidebar", ImVec2(200, MH - 2), true);
                if (my_logo) {
                    float w = 150, h = 60;
                    ImVec4 tint = ImVec4(1.0f, 0.0f, 0.502f, 1.0f);
                    ImGui::SetCursorPosX((200 - w) / 2.0f);
                    ImGui::Image((void*)my_logo, ImVec2(w, h), ImVec2(0, 0), ImVec2(1, 1), tint, ImVec4(0, 0, 0, 0));
                    ImGui::Spacing();
                }
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.156f, 0.156f, 0.156f, 0.941f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.0f, 0.502f, 0.706f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.0f, 0.502f, 0.627f));

                if (ImGui::Button("Visuals", ImVec2(-1, 40))) tab_id = 0;
                if (ImGui::Button("Flags", ImVec2(-1, 40))) tab_id = 1;
                if (ImGui::Button("Settings", ImVec2(-1, 40))) tab_id = 2;

                ImGui::PopStyleColor(3);

                ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 30);
                ImGui::TextColored(ImColor(80, 80, 80), "INSERT = toggle menu");
                ImGui::EndChild();
            }
            ImGui::EndGroup();

            ImGui::SameLine(0, 0);

            ImGui::BeginGroup();
            {
                ImGui::BeginChild("content", ImVec2(MW - 200 - 2, MH - 2), true);
                if (tab_id == 0)
                {
                    ImGui::Text("Visuals");
                    ImGui::Spacing();
                    ImGui::PushID("BoxESP");     ImGuiSwitch("##switch", &menu_show_box_esp); ImGui::SameLine(); ImGui::TextUnformatted("Box ESP"); ImGui::PopID();
                    ImGui::PushID("Skeleton");   ImGuiSwitch("##switch", &menu_show_skeleton_esp); ImGui::SameLine(); ImGui::TextUnformatted("Skeleton"); ImGui::PopID();
                    ImGui::PushID("Head");       ImGuiSwitch("##switch", &menu_show_head_tracker); ImGui::SameLine(); ImGui::TextUnformatted("Head Tracker"); ImGui::PopID();

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    ImGui::Text("Team ESP:");
                    ImGui::PushID("BoxESPTeam");       ImGuiSwitch("##switch", &menu_show_box_esp_team); ImGui::SameLine(); ImGui::TextUnformatted("Box ESP"); ImGui::PopID();
                    ImGui::PushID("SkeletonTeam");     ImGuiSwitch("##switch", &menu_show_skeleton_esp_team); ImGui::SameLine(); ImGui::TextUnformatted("Skeleton"); ImGui::PopID();

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    ImGui::Text("ESP Visible:");
                    ImGui::PushID("Health"); ImGuiSwitch("##switch", &menu_show_health_bar); ImGui::SameLine(); ImGui::TextUnformatted("Health Bar"); ImGui::PopID();
                    ImGui::PushID("Armor");  ImGuiSwitch("##switch", &menu_show_armor_bar);  ImGui::SameLine(); ImGui::TextUnformatted("Armor Bar"); ImGui::PopID();

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    ImGui::Text("ESP Colors:");
                    ImGui::ColorEdit3("Enemy##col", colEnemy, ImGuiColorEditFlags_NoInputs);
                    ImGui::ColorEdit3("Team##col", colTeam, ImGuiColorEditFlags_NoInputs);
                }
                if (tab_id == 1)
                {
                    ImGui::Text("Enabled flags:");
                    ImGui::Spacing();
                    ImGui::PushID("ExtraFlags"); ImGuiSwitch("##switch", &menu_show_extra_flags); ImGui::SameLine(); ImGui::TextUnformatted("Enable Flags"); ImGui::PopID();
                    ImGui::Spacing();
                    ImGui::Indent(20.0f);
                    ImGui::PushID("FlagName");    ImGuiSwitch("##switch", &menu_show_flag_name);    ImGui::SameLine(); ImGui::TextUnformatted("Name"); ImGui::PopID();
                    ImGui::PushID("FlagHealth");  ImGuiSwitch("##switch", &menu_show_flag_health);  ImGui::SameLine(); ImGui::TextUnformatted("Health"); ImGui::PopID();
                    ImGui::PushID("FlagArmor");   ImGuiSwitch("##switch", &menu_show_flag_armor);   ImGui::SameLine(); ImGui::TextUnformatted("Armor"); ImGui::PopID();
                    ImGui::PushID("FlagWeapon");  ImGuiSwitch("##switch", &menu_show_flag_weapon);  ImGui::SameLine(); ImGui::TextUnformatted("Weapon Names"); ImGui::PopID();
                    ImGui::PushID("FlagWeaponI"); ImGuiSwitch("##switch", &menu_show_flag_weapon_icon); ImGui::SameLine(); ImGui::TextUnformatted("Weapon Icons"); ImGui::PopID();
                    ImGui::PushID("FlagDist");    ImGuiSwitch("##switch", &menu_show_flag_distance); ImGui::SameLine(); ImGui::TextUnformatted("Distance"); ImGui::PopID();
                    ImGui::PushID("FlagMoney");   ImGuiSwitch("##switch", &menu_show_flag_money);   ImGui::SameLine(); ImGui::TextUnformatted("Money"); ImGui::PopID();
                    ImGui::PushID("FlagFlash");   ImGuiSwitch("##switch", &menu_show_flag_flashed); ImGui::SameLine(); ImGui::TextUnformatted("Flashed"); ImGui::PopID();
                    ImGui::PushID("FlagDefuse");  ImGuiSwitch("##switch", &menu_show_flag_defusing); ImGui::SameLine(); ImGui::TextUnformatted("Defusing"); ImGui::PopID();
                    ImGui::Unindent();

                    ImGui::Spacing();
                    ImGui::Text("Flag render distance:");
                    ImGui::SliderInt("##dist", &config::flag_render_distance, 5, 400, "%dm");
                }
                if (tab_id == 2)
                {
                    ImGui::Text("Settings");
                    ImGui::Spacing();
                    ImGui::PushID("Console"); ImGuiSwitch("##switch", &menu_show_console); ImGui::SameLine(); ImGui::TextUnformatted("Console"); ImGui::PopID();
                    ImGui::PushID("Streamproof"); ImGuiSwitch("##switch", &menu_streamproof); ImGui::SameLine(); ImGui::TextUnformatted("Streamproof"); ImGui::PopID();

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    ImGui::Text("Cache Refresh Rate:");
                    ImGui::SliderInt("##cache", &config::cache_refresh_rate, 1, 20, "%dms");
                    ImGui::Spacing();
                    if (ImGui::Button("Save Config", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 28)))
                        config::save();
                    ImGui::SameLine();
                    if (ImGui::Button("Unload ESP", ImVec2(ImGui::GetContentRegionAvail().x, 28)))
                        finish = true;
                }
                ImGui::EndChild();
            }
            ImGui::EndGroup();

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            float thickness = 2.0f;
            ImVec2 p_min = ImVec2(thickness, thickness);
            ImVec2 p_max = ImVec2((float)MW - thickness, (float)MH - thickness);

            draw_list->AddRect(p_min, p_max, IM_COL32(255, 0, 128, 255), 0.0f, 15, thickness);

            ImGui::End();
        }

        config::show_box_esp = menu_show_box_esp;
        config::show_skeleton_esp = menu_show_skeleton_esp;
        config::show_box_esp_team = menu_show_box_esp_team;
        config::show_skeleton_esp_team = menu_show_skeleton_esp_team;
        config::show_head_tracker = menu_show_head_tracker;
        config::show_extra_flags = menu_show_extra_flags;
        config::show_console = menu_show_console;
        config::streamproof = menu_streamproof;
        config::show_health_bar = menu_show_health_bar;
        config::show_armor_bar = menu_show_armor_bar;
        config::show_flag_name = menu_show_flag_name;
        config::show_flag_health = menu_show_flag_health;
        config::show_flag_armor = menu_show_flag_armor;
        config::show_flag_weapon = menu_show_flag_weapon;
        config::show_flag_weapon_icon = menu_show_flag_weapon_icon;
        config::show_flag_distance = menu_show_flag_distance;
        config::show_flag_money = menu_show_flag_money;
        config::show_flag_flashed = menu_show_flag_flashed;
        config::show_flag_defusing = menu_show_flag_defusing;

        ImGui::EndFrame();

        g_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.f, 0);
        if (SUCCEEDED(g_pDevice->BeginScene())) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pDevice->EndScene();
        }
        g_pDevice->Present(NULL, NULL, NULL, NULL);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    if (my_logo)
        my_logo->Release();

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_pDevice) g_pDevice->Release();
    if (g_pD3D)    g_pD3D->Release();
    DestroyWindow(g_hMenu);
    UnregisterClass("DexterMenuClass", GetModuleHandle(NULL));
}

void read_thread() {
    while (!finish) {
        g_game.loop();
        std::this_thread::sleep_for(std::chrono::milliseconds(config::cache_refresh_rate));
    }
}

int main() {
    SetConsoleTitleA("Dexter Project - CS2 External ESP");

    std::cout << "============================================" << std::endl;
    std::cout << "       Dexter Project - CS2 External ESP" << std::endl;
    std::cout << "       Desenvolvido por: DEV JOHN" << std::endl;
    std::cout << "       Offsets: a2x/cs2-dumper" << std::endl;
    std::cout << "============================================\n" << std::endl;

    std::cout << "[config] Reading configuration." << std::endl;
    if (config::read())
        std::cout << "[config] Successfully read configuration file\n" << std::endl;
    else
        std::cout << "[config] Error reading config file, resetting to the default state\n" << std::endl;

    config::esp_box_color_enemy = RGB(255, 0, 128);
    config::esp_skeleton_color_enemy = RGB(255, 0, 128);
    config::esp_box_color_team = RGB(0, 255, 50);
    config::esp_skeleton_color_team = RGB(0, 255, 50);
    config::team_esp = false;

    std::cout << "[offsets] Reading offsets from file offsets.json." << std::endl;
    if (updater::read())
        std::cout << "[offsets] Successfully read offsets file\n" << std::endl;
    else
        std::cout << "[offsets] Error reading offsets file, resetting to the default state\n" << std::endl;

    g_game.init();

    if (g_game.buildNumber != updater::build_number) {
        std::cout << "[cs2] Build number doesnt match, offsets may be outdated." << std::endl;
        std::cout << "[cs2] Run atualizar_offsets.bat to update!" << std::endl;
        std::cout << "[cs2] Press any key to continue" << std::endl;
        std::cin.get();
    }
    else {
        std::cout << "[cs2] Offsets are up to date! have fun!" << std::endl;
    }

    render::InitWeaponFont(28);
    if (render::weaponFontLoaded)
        std::cout << "[font] Weapon icon font loaded!" << std::endl;
    else
        std::cout << "[font] CS2GunIcons.ttf not found, using text fallback" << std::endl;

    std::cout << "[overlay] Waiting to focus game..." << std::endl;
    while (GetForegroundWindow() != g_game.process->hwnd_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        g_game.process->UpdateHWND();
        ShowWindow(g_game.process->hwnd_, TRUE);
    }
    std::cout << "[overlay] Creating overlay..." << std::endl;

    std::thread menuThread(menu_thread);
    menuThread.detach();

    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WndProc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = WHITE_BRUSH;
    wc.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongA(g_game.process->hwnd_, -6));
    wc.lpszMenuName = " ";
    wc.lpszClassName = " ";
    RegisterClassExA(&wc);

    GetClientRect(g_game.process->hwnd_, &g::gameBounds);

    HWND hWnd = CreateWindowExA(
        WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        " ", "Enochian.exe", WS_POPUP,
        g::gameBounds.left, g::gameBounds.top,
        g::gameBounds.right - g::gameBounds.left,
        g::gameBounds.bottom + g::gameBounds.left,
        NULL, NULL, NULL, NULL);

    if (!hWnd) return 0;

    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(hWnd, TRUE);

    std::thread read(read_thread);

    std::cout << "[menu] Press INSERT to toggle menu" << std::endl;
    std::cout << "[menu] Press END to unload" << std::endl;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) && !finish)
    {
        if (GetAsyncKeyState(VK_END) & 0x8000) finish = true;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    read.detach();
    render::CleanupCache();
    Beep(700, 100); Beep(700, 100);
    std::cout << "[Dexter Project] Unloading ESP..." << std::endl;
    DeleteDC(g::hdcBuffer);
    DeleteObject(g::hbmBuffer);
    DestroyWindow(hWnd);
    g_game.close();

#ifdef NDEBUG
    std::cout << "[Dexter Project] Press any key to close" << std::endl;
    std::cin.get();
#endif

    return 1;
}