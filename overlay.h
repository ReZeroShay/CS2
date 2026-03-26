#pragma once
#include <windows.h>
#include <string_view>
#include "ui.h"
struct OverlayContext {
    auto event() {
        // GetAsyncKeyState不需要应用程序获取焦点
        // 静态变量记录上一帧的状态
        static bool tabWasDown = false;
        static bool qWasDown = false;
        static bool wWasDown = false;

        // 获取当前物理状态
        bool tabIsDown = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
        bool qIsDown = (GetAsyncKeyState('Q') & 0x8000) != 0;
        bool wIsDown = (GetAsyncKeyState('W') & 0x8000) != 0;

        // 逻辑：Tab + Q (组合键返回)
        if (tabIsDown && qIsDown) {
            return true;
        }

        // 逻辑：Tab + W (切换菜单显示)
        // 只有当 W 是这一帧刚按下的（且 Tab 按住时），才触发一次
        if (tabIsDown && wIsDown && !wWasDown) {
            isMenuVisible = !isMenuVisible;
        }

        // 更新状态供下一帧使用
        tabWasDown = tabIsDown;
        qWasDown = qIsDown;
        wWasDown = wIsDown;

        return false;
    }
    auto renderCustomCursor() {
        // 手动绘制一个鼠标
        const ImVec2 mousePos = ImGui::GetMousePos();
        ImDrawList *drawList = ImGui::GetForegroundDrawList();

        float size = 18.0f;

        // 箭头三角形
        ImVec2 p1 = mousePos;
        ImVec2 p2 = ImVec2(mousePos.x + size * 0.5f, mousePos.y + size);
        ImVec2 p3 = ImVec2(mousePos.x + size, mousePos.y + size * 0.7f);
        // 阴影
        ImVec2 shadowOffset = ImVec2(2, 2);
        drawList->AddTriangleFilled(ImVec2(p1.x + 2, p1.y + 2), ImVec2(p2.x + 2, p2.y + 2), ImVec2(p3.x + 2, p3.y + 2),
                                    IM_COL32(0, 0, 0, 120));

        // 主体
        drawList->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 255, 255, 255));

        // 黑色描边
        drawList->AddTriangle(p1, p2, p3, IM_COL32(0, 0, 0, 255), 1.5f);

        // 中间斜线（模拟系统鼠标样式）
        drawList->AddLine(p1, ImVec2(mousePos.x + size * 0.6f, mousePos.y + size * 0.6f), IM_COL32(0, 0, 0, 255), 1.5f);
    }
    auto mainLoop() -> bool {
        auto shoutExit = event();
        // 应用程序没有焦点，收不到正常的鼠标消息
        // 因此绕过 Windows 消息机制，强行通过系统底层接口“偷”到鼠标位置并塞给 ImGui
        if (isMenuVisible) {
            ImGuiIO &io = ImGui::GetIO();
            POINT mousePosition;
            GetCursorPos(&mousePosition);
            ScreenToClient(hwnd, &mousePosition);
            io.MousePos.x = mousePosition.x;
            io.MousePos.y = mousePosition.y;
            renderCustomCursor();
        }

        return shoutExit;
    }

    bool isMenuVisible = false;
    HWND hwnd;
};
