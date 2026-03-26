#pragma once
#include <map>
#include <algorithm>
#include <vector>
#include <functional>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include <ranges>
#include "hello_imgui/hello_imgui.h"
inline void *logoTexture;
inline int logoWidth, logoHeight;

namespace ImVeil {
    struct Theme {
        ImU32 toggleSwitchBg = ImColor(114, 117, 125);   // rgb(114, 117, 125)
        ImU32 toggleSwitchAction = ImColor(0, 106, 255); // rgb(0, 106, 255)
        float toggleSwitchRounding = 15.f;

        ImU32 titleAction = ImColor(0, 106, 255); // rgb(0, 106, 255)
    };
    inline Theme theme;

    namespace animations {

        inline int fastLerpInt(const ImGuiID id, bool state, int min, int max, int speed) {

            static std::map<const ImGuiID, int> valuesMapInt;
            auto value = valuesMapInt.find(id);

            if (value == valuesMapInt.end()) {
                valuesMapInt.insert({id, 0});
                value = valuesMapInt.find(id);
            }

            const float frameRateSpeed = speed * (1.f - ImGui::GetIO().DeltaTime);

            if (state) {
                if (value->second < max)
                    value->second += frameRateSpeed;
            } else {
                if (value->second > min)
                    value->second -= frameRateSpeed;
            }

            value->second = std::clamp(value->second, min, max);

            return value->second;
        }

        inline float fastLerpFloat(const ImGuiID id, bool state, float min, float max, float speed) {

            static std::map<const ImGuiID, float> valuesMapFloat;
            auto value = valuesMapFloat.find(id);

            if (value == valuesMapFloat.end()) {
                valuesMapFloat.insert({id, 0.f});
                value = valuesMapFloat.find(id);
            }

            const float frameRateSpeed = speed * (1.f - ImGui::GetIO().DeltaTime);

            if (state) {
                if (value->second < max)
                    value->second += frameRateSpeed;
            } else {
                if (value->second > min)
                    value->second -= frameRateSpeed;
            }

            value->second = std::clamp(value->second, min, max);

            return value->second;
        }

        inline ImColor fastColorLerp(ImColor start, ImColor end, float stage) {
            ImVec4 lerp = ImLerp(ImVec4(start.Value.x, start.Value.y, start.Value.z, start.Value.w),
                                 ImVec4(end.Value.x, end.Value.y, end.Value.z, end.Value.w), stage);

            return ImGui::ColorConvertFloat4ToU32(lerp);
        }

    } // namespace animations

    inline auto toggleSwitch(const char *label, bool *v) -> bool {
        auto window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext &g = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        const float square_sz = ImGui::GetFrameHeight();
        const float slider_width = square_sz * 1.7f;  // 比例缩放，而不是硬编码 40.f
        const float circle_radius = square_sz * 0.4f; // 比例缩放，而不是硬编码 10.f
        const ImVec2 pos = window->DC.CursorPos;
        const float w = ImGui::GetContentRegionAvail().x;
        const ImRect total_bb(pos, pos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
        ImGui::ItemSize(total_bb, style.FramePadding.y);
        const bool is_visible = ImGui::ItemAdd(total_bb, id);
        const bool is_multi_select = (g.LastItemData.ItemFlags & ImGuiItemFlags_IsMultiSelect) != 0;
        if (!is_visible)
            if (!is_multi_select || !g.BoxSelectState.UnclipMode ||
                !g.BoxSelectState.UnclipRect.Overlaps(
                    total_bb)) // Extra layer of "no logic clip" for box-select support
            {
                IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                            g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable |
                                                (*v ? ImGuiItemStatusFlags_Checked : 0));
                return false;
            }

        // Range-Selection/Multi-selection support (header)
        bool checked = *v;
        if (is_multi_select)
            ImGui::MultiSelectItemHeader(id, &checked, NULL);

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

        // Range-Selection/Multi-selection support (footer)
        if (is_multi_select)
            ImGui::MultiSelectItemFooter(id, &checked, &pressed);
        else if (pressed)
            checked = !checked;

        if (*v != checked) {
            *v = checked;
            pressed = true; // return value
            ImGui::MarkItemEdited(id);
        }

        const ImVec2 check_pos = ImVec2{total_bb.Max.x - slider_width, total_bb.Min.y};
        const ImRect check_bb(check_pos, check_pos + ImVec2(slider_width, square_sz));
        const bool mixed_value = (g.LastItemData.ItemFlags & ImGuiItemFlags_MixedValue) != 0;

        auto circle = animations::fastLerpFloat(id, *v, square_sz * 0.1f,
                                                slider_width - circle_radius * 2 - square_sz * 0.1f, 0.3f);
        // circle = ImLerp(circle, *v ? 0.f : slider_width - circle_radius * 2, g.IO.DeltaTime * 12.f);
        if (is_visible) {
            ImGui::RenderNavCursor(total_bb, id);

            ImGui::RenderFrame(check_bb.Min, check_bb.Max,
                               ImGui::GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive
                                                  : hovered         ? ImGuiCol_FrameBgHovered
                                                  : *v              ? ImGuiCol_FrameBg
                                                                    : ImGuiCol_WindowBg),
                               true, theme.toggleSwitchRounding);

            ImU32 check_col = *v ? theme.toggleSwitchAction : theme.toggleSwitchBg;
            window->DrawList->AddCircleFilled(ImVec2{check_bb.Min.x + circle_radius + circle, check_bb.GetCenter().y},
                                              circle_radius, check_col);
        }

        const ImVec2 label_pos = ImVec2{total_bb.Min.x, check_bb.Min.y + style.FramePadding.y};
        if (g.LogEnabled)
            ImGui::LogRenderedText(&label_pos, mixed_value ? "[~]" : *v ? "[x]" : "[ ]");
        if (is_visible && label_size.x > 0.0f)
            ImGui::RenderText(label_pos, label);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                                    g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable |
                                        (*v ? ImGuiItemStatusFlags_Checked : 0));
        return pressed;
    }

    inline bool SliderScalar(const char *label, ImGuiDataType data_type, void *p_data, const void *p_min,
                             const void *p_max, const char *format, ImGuiSliderFlags flags) {
        auto window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        auto &g = *GImGui;
        const auto &style = g.Style;
        const auto id = window->GetID(label);
        const float w = ImGui::GetContentRegionAvail().x;
        const float slider_size = 8.f;

        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
        const ImVec2 text_pos = window->DC.CursorPos;
        const ImRect frame_bb(window->DC.CursorPos + ImVec2(0, label_size.y + style.FramePadding.y * 2.0f),
                              window->DC.CursorPos + ImVec2(0, label_size.y + style.FramePadding.y * 2.0f) +
                                  ImVec2(w, slider_size));
        const ImRect total_bb(window->DC.CursorPos,
                              frame_bb.Max +
                                  ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));

        const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
            return false;

        // Default format string when passing NULL
        if (format == NULL)
            format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

        const bool hovered = ImGui::ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
        bool temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive(id);
        if (!temp_input_is_active) {
            // Tabbing or CTRL+click on Slider turns it into an input box
            const bool clicked = hovered && ImGui::IsMouseClicked(0, ImGuiInputFlags_None, id);
            const bool make_active = (clicked || g.NavActivateId == id);
            if (make_active && clicked)
                ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
            if (make_active && temp_input_allowed)
                if ((clicked && g.IO.KeyCtrl) ||
                    (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))
                    temp_input_is_active = true;

            // Store initial value (not used by main lib but available as a convenience but some mods e.g. to
            // revert)
            if (make_active)
                memcpy(&g.ActiveIdValueOnActivation, p_data, ImGui::DataTypeGetInfo(data_type)->Size);

            if (make_active && !temp_input_is_active) {
                ImGui::SetActiveID(id, window);
                ImGui::SetFocusID(id, window);
                ImGui::FocusWindow(window);
                g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
            }
        }

        if (temp_input_is_active) {
            // Only clamp CTRL+Click input when ImGuiSliderFlags_ClampOnInput is set (generally via
            // ImGuiSliderFlags_AlwaysClamp)
            const bool clamp_enabled = (flags & ImGuiSliderFlags_ClampOnInput) != 0;
            return ImGui::TempInputScalar(frame_bb, id, label, data_type, p_data, format, clamp_enabled ? p_min : NULL,
                                          clamp_enabled ? p_max : NULL);
        }

        // Draw frame
        const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive
                                                   : hovered        ? ImGuiCol_FrameBgHovered
                                                                    : ImGuiCol_FrameBg);
        ImGui::RenderNavCursor(frame_bb, id);
        ImGui::RenderFrame({frame_bb.Min.x, frame_bb.GetCenter().y - slider_size / 2},
                           {frame_bb.Max.x, frame_bb.GetCenter().y + slider_size / 2}, frame_col, true,
                           g.Style.FrameRounding);

        // Slider behavior
        ImRect grab_bb;
        const bool value_changed =
            ImGui::SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb);
        if (value_changed)
            ImGui::MarkItemEdited(id);

        // Render grab
        if (grab_bb.Max.x > grab_bb.Min.x)
            window->DrawList->AddCircleFilled(
                grab_bb.GetCenter(), slider_size,
                ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab));

        // Display value using user-provided display format so user can add prefix/suffix/decorations to the
        // value.
        char value_buf[64];
        const char *value_buf_end =
            value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
        if (g.LogEnabled)
            ImGui::LogSetNextTextDecoration("{", "}");
        const ImVec2 value_text_size = ImGui::CalcTextSize(value_buf, value_buf_end, false);
        ImGui::RenderTextClipped({frame_bb.Max.x - value_text_size.x, text_pos.y}, {frame_bb.Max.x, frame_bb.Min.y},
                                 value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

        if (label_size.x > 0.0f)
            ImGui::RenderText(text_pos, label);

        return value_changed;
    }
    inline bool SliderInt(const char *label, int *v, int v_min, int v_max, const char *format,
                          ImGuiSliderFlags flags = 0) {
        return SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
    }
    inline bool SliderFloat(const char *label, float *v, float v_min, float v_max, const char *format,
                            ImGuiSliderFlags flags = 0) {
        return SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
    }

    inline bool button(const char *label) { return ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 0)); }

    namespace notifications {
        enum class Severity : std::uint8_t {
            Success,
            Warning,
            Error,
        };
        struct Notification {
            std::string message;
            Severity level{Severity::Success};
            float age{0.0f};         // 已存在时间
            float lifetime{3.0f};    // 计划显示时长（秒）
            float appear_anim{0.0f}; // 出现动画进度 [0,1]
        };
        inline auto activeNotifications = std::vector<Notification>{};

        inline void post(std::string_view text, Severity severity = Severity::Success, float duration = 3.0f) {
            activeNotifications.push_back(Notification{
                .message = std::string{text},
                .level = severity,
                .age = 0.0f,
                .lifetime = duration,
                .appear_anim = 0.0f,
            });
        }

        [[nodiscard]] inline constexpr float ease_out_cubic(float t) noexcept {
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        [[nodiscard]] inline constexpr ImColor theme_color_for(Severity s) noexcept {
            switch (s) {
            case Severity::Success:
                return ImColor(80, 200, 120);
            case Severity::Warning:
                return ImColor(230, 190, 70);
            case Severity::Error:
                return ImColor(230, 80, 80);
            }
            return ImColor(80, 200, 120);
        }

        [[nodiscard]] inline constexpr std::u8string_view icon_for(Severity s) noexcept {
            switch (s) {
            case Severity::Success:
                return u8"\uf49e";
            case Severity::Warning:
                return u8"\uea6c";
            case Severity::Error:
                return u8"\uea87";
            }
            return u8"\uf49e";
        }
        inline void rendernotifications() {
            if (activeNotifications.empty()) {
                return;
            }

            auto &drawList = *ImGui::GetForegroundDrawList();
            const auto &io = ImGui::GetIO();
            const float deltaTime = io.DeltaTime;
            const ImVec2 display_size = ImGui::GetMainViewport()->WorkSize;
            // ── DPI 缩放适配 ──────────────────────────────────────────────
            // 确保从 Viewport 获取缩放，这样在多显示器切换时依然准确
            float s = ImGui::GetMainViewport()->DpiScale;

            // ── 布局常量（全部应用缩放） ──────────────────────────────────
            const float padding = ImGui::GetFontSize() / 10; // 通知框距离屏幕边缘（右上角）的距离
            const float MIN_W = ImGui::GetFontSize() * 5;
            const float MAX_W = ImGui::GetFontSize() * 10;
            const float spacing = ImGui::GetFontSize() / 10; // 多个通知同时出现时，它们之间的垂直距离
            const float ACCENT_BAR_W = 4.0f * s;
            const float PROGRESS_H = ImGui::GetFontSize() / 10;
            const float ROUNDING = ImGui::GetFontSize() / 10;
            const float FADE_DURATION = 0.25f;

            float nextY = padding;

            for (std::size_t i = 0; i < activeNotifications.size();) {
                auto &n = activeNotifications[i];

                // ── 状态更新 ──────────────────────────────────────────────
                n.age += deltaTime;
                n.appear_anim += deltaTime * 5.0f;

                if (n.age > n.lifetime) {
                    activeNotifications.erase(activeNotifications.begin() + i);
                    continue;
                }

                // ── 动画与透明度计算 ──────────────────────────────────────
                float alpha = 1.0f;
                if (n.age < FADE_DURATION)
                    alpha = n.age / FADE_DURATION;
                else if (n.age > n.lifetime - FADE_DURATION)
                    alpha = (n.lifetime - n.age) / FADE_DURATION;

                const float t_appear = std::clamp(n.appear_anim, 0.0f, 1.0f);
                const float eased_t = ease_out_cubic(t_appear);

                // 关键修复：动画位移 slide_dist 也必须乘以 s，否则在笔记本上会显得位移过大
                const float slide_dist = (1.0f - eased_t) * 80.0f * s;

                // ── 尺寸计算 ──────────────────────────────────────────────
                const auto icon = icon_for(n.level);
                // 注意：CalcTextSize 结果已经包含了 FontScale，不需要额外乘 s
                const auto icon_size = ImGui::CalcTextSize((char *)icon.data());
                const auto text_size = ImGui::CalcTextSize(n.message.c_str());

                float spacing = ImGui::GetFontSize() / 2; // icon 和文字的间距
                const float content_width = icon_size.x + spacing + text_size.x;
                const float width = std::clamp(content_width, MIN_W, MAX_W);
                const float height = ImMax(ImGui::GetFontSize() * 1.5f, icon_size.y + (16.0f * s));

                // ── 坐标对齐逻辑 ──────────────────────────────────────────
                // x 坐标基于 display_size.x，确保无论屏幕多大都贴着右边
                const float x = display_size.x - width - padding + slide_dist;
                const float y = nextY;

                const ImVec2 min{x, y};
                const ImVec2 max{x + width, y + height};

                // ── 渲染层 ────────────────────────────────────────────────
                const ImColor accent = theme_color_for(n.level);
                const ImColor bg_col{15, 15, 20, int(230 * alpha)};

                // 背景主体
                drawList.AddRectFilled(min, max, bg_col, ROUNDING);

                // 左侧装饰条
                drawList.AddRectFilled(min, {min.x + ACCENT_BAR_W, max.y},
                                       ImColor(accent.Value.x, accent.Value.y, accent.Value.z, alpha), ROUNDING,
                                       ImDrawFlags_RoundCornersLeft);

                // 图标在通知框内部的起点位置
                const float icon_x_off = ImGui::GetFontSize() / 5;
                const float icon_y_off = (height - icon_size.y) * 0.5f;
                drawList.AddText({x + icon_x_off, y + icon_y_off},
                                 ImColor(accent.Value.x, accent.Value.y, accent.Value.z, alpha), (char *)icon.data());

                // 文字：颜色逻辑简化
                ImU32 text_col = IM_COL32(240, 240, 245, int(255 * alpha));
                if (n.level == Severity::Error)
                    text_col = IM_COL32(255, 200, 200, int(255 * alpha));

                // 文字偏移：基于图标宽度和间距
                // 文字在通知框内部的起点位置
                const float text_x_off = icon_x_off + icon_size.x + ImGui::GetFontSize() / 2;
                const float text_y_off = (height - text_size.y) * 0.5f;
                drawList.AddText({x + text_x_off, y + text_y_off}, text_col, n.message.c_str());

                // 底部进度条
                const float progress_ratio = std::clamp(1.0f - (n.age / n.lifetime), 0.0f, 1.0f);
                drawList.AddRectFilled({x, max.y - PROGRESS_H}, {x + (width * progress_ratio), max.y},
                                       ImColor(accent.Value.x, accent.Value.y, accent.Value.z, alpha * 0.8f), ROUNDING,
                                       ImDrawFlags_RoundCornersBottom);

                // ── 叠加偏移量 ────────────────────────────────────────────
                nextY += height + spacing;
                ++i;
            }
        }
    } // namespace notifications

    inline float easeOut(float t) noexcept {
        return 1.0f - (1.0f - t) * (1.0f - t); // 後段減速，常用于淡出
    }

    inline float easeIn(float t) noexcept {
        return t * t; // 前段加速，常用于淡入
    }

    /// @brief 界面导航系统核心结构
    /// 使用函数式风格组织导航层级，支持主导航 + 子导航两级结构
    struct UiNavigation {
        // ─── 主导航淡入淡出 ────────────────────────────────────────
        std::size_t currentMainIndex{0};    // 当前正在显示的主导航索引
        std::size_t targetMainIndex{0};     // 用户请求切换到的目标索引
        bool isMainTransitioning{false};    // 是否正在进行主导航过渡
        float mainTransitionProgress{0.0f}; // 0.0 = 完全显示当前 → 1.0 = 完全显示目标

        // ─── 子导航淡入淡出 ────────────────────────────────────────
        std::size_t currentSubIndex{0};
        std::size_t targetSubIndex{0};
        bool isSubTransitioning{false};
        float subTransitionProgress{1.0f};

        void requestMainSwitch(std::size_t newIndex) {
            if (newIndex >= mainItems_.size() || newIndex == currentMainIndex) {
                return;
            }
            targetMainIndex = newIndex;
            isMainTransitioning = true;
            mainTransitionProgress = 0.0f;
            // 可选：在这里可以触发音效、记录切换时间等
        }
        void requestSubSwitch(std::size_t newIndex) {
            auto &currentMain = mainItems_[currentMainIndex];
            if (newIndex >= currentMain.subItems.size() || newIndex == currentSubIndex) {
                return;
            }
            targetSubIndex = newIndex;
            isSubTransitioning = true;
            subTransitionProgress = 0.0f;
        }
        void updateTransitions(float deltaTime) {
            constexpr float kFadeSpeed = 3.0f; // 可调：越大越快

            // 主导航过渡
            if (isMainTransitioning) {
                mainTransitionProgress += deltaTime * kFadeSpeed;
                if (mainTransitionProgress >= 1.0f) {
                    mainTransitionProgress = 1.0f;
                    currentMainIndex = targetMainIndex;
                    isMainTransitioning = false;
                    // 主导航切换完成时，通常重置子导航到第0项
                    currentSubIndex = 0;
                    targetSubIndex = 0;
                    subTransitionProgress = 0.0f;
                    isSubTransitioning = true;
                }
            }

            // 子导航过渡（独立于主导航）
            if (isSubTransitioning) {
                subTransitionProgress += deltaTime * kFadeSpeed;
                if (subTransitionProgress >= 1.0f) {
                    subTransitionProgress = 1.0f;
                    currentSubIndex = targetSubIndex;
                    isSubTransitioning = false;
                }
            }
        }
        float easeInOutSine(float x) const { return -(cosf(3.14159f * x) - 1.0f) / 2.0f; }

        // 写法1：使用透明度（最常用、最稳定）
        void beginMainContent() const {
            float alpha = 1.0f;

            if (isMainTransitioning) {
                // 两种常见风格任选其一：

                // 风格 A：线性交叉淡化（最常用）
                // alpha = (mainTransitionProgress < 0.5f)
                //             ? (1.0f - mainTransitionProgress * 2.0f)  // 先淡出旧内容
                //             : (mainTransitionProgress * 2.0f - 1.0f); // 再淡入新内容

                // 使用 EaseInOut 让透明度变化更符合物理感
                float eased = easeInOutSine(mainTransitionProgress);
                // alpha = easeIn(mainTransitionProgress);
                // 风格 B：简单同时淡出淡入（重叠感更强）
                // alpha = 1.0f - mainTransitionProgress;   // 只用旧内容淡出
                // 或 alpha = mainTransitionProgress;       // 只用新内容淡入
            }

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        }

        void endMainContent() const {
            ImGui::PopStyleVar(); // 永远 pop，匹配上面的 push
        }

        // 子导航内容同理（可复用相同逻辑，或独立调整速度）
        void beginSubContent() const {
            float eased = easeInOutSine(subTransitionProgress);
            // 淡入时伴随向上平移效果
            float offsetY = (1.0f - eased) * 10.0f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, eased);

            // 或使用更复杂的缓动函数
            // float eased = smoothstep(0.0f, 1.0f, subTransitionProgress);
            // ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f - eased);
        }

        void endSubContent() const { ImGui::PopStyleVar(); }

        using Action = std::function<void(void *)>;

        /// @brief 子导航项（叶子节点，点击后执行具体功能）
        struct SubItem {
            std::u8string title;
            Action action;

            SubItem(std::u8string_view title, Action act) : title(title), action(std::move(act)) {}
        };

        /// @brief 主导航项（可包含多个子项）
        struct MainItem {
            std::u8string title;
            std::u8string iconUnicode;
            std::vector<SubItem> subItems;
            std::size_t selectedSubIndex{0};

            MainItem(std::u8string_view title, std::u8string_view icon) : title(title), iconUnicode(icon) {}

            /// @brief 便捷添加子项（完美转发 lambda 或 functor）
            template <typename F> MainItem &addSubItem(std::u8string_view title, F &&action) {
                subItems.emplace_back(title, std::forward<F>(action));
                return *this;
            }
        };

        /// @brief 添加一个主导航分类并返回引用以便链式添加子项
        MainItem &addMainItem(std::u8string_view title, std::u8string_view icon) {
            mainItems_.emplace_back(title, icon);
            return mainItems_.back();
        }

        void render(void *userData) {
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

            // 自适应窗口大小，保持比例
            constexpr float kBaseWidth = 890.0f;
            constexpr float kBaseHeight = 620.0f;
            float targetWidth = std::clamp(displaySize.x * 0.6f, 800.0f, 1600.0f);
            float targetHeight = targetWidth * (kBaseHeight / kBaseWidth);

            // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * (targetWidth / kBaseWidth));

            if (!ImGui::Begin("CheatMainUI", nullptr,
                              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
                ImGui::End();
                ImGui::PopStyleVar();
                ImGui::PopFont();
                return;
            }

            // 主题切换按钮（右上角）
            {
                // ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 40.0f);
                // ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                // ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
                // ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
                // ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ctx_.GetActiveAccent());
                // ImGui::SetWindowFontScale(2.0f);

                // if (ImGui::Button(reinterpret_cast<const char *>(ctx_.theme.icon())))
                // {
                // }

                // ImGui::SetWindowFontScale(1.0f);
                // ImGui::PopStyleColor(4);
            }

            // Logo + 主导航 + 版本信息
            ImGui::BeginGroup();
            {
                IM_ASSERT(logoTexture);
                ImGui::Image(logoTexture, ImVec2(static_cast<float>(logoWidth), static_cast<float>(logoHeight)));

                for (auto &&[idx, item] : std::views::enumerate(mainItems_)) {
                    if (drawMainTab(item.title, item.iconUnicode, targetMainIndex == idx)) {
                        requestMainSwitch(idx);
                    }
                }

                // 底部版本信息
                const float footerH = ImGui::GetTextLineHeightWithSpacing();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y - footerH);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 120, 120, 255));
                ImGui::TextDisabled("by shay v0.0.1");
                ImGui::PopStyleColor();
            }
            ImGui::EndGroup();

            ImGui::SameLine(0.0f, ImGui::GetFontSize() * 2);

            // 右侧内容区
            ImGui::BeginGroup();
            {
                if (!mainItems_.empty()) {
                    auto &currentMain = mainItems_[targetMainIndex];

                    float dt = ImGui::GetIO().DeltaTime;
                    updateTransitions(dt);

                    // 子导航栏（横向）
                    for (auto &&[i, sub] : std::views::enumerate(currentMain.subItems)) {
                        bool isActive = (i == currentSubIndex) || (isSubTransitioning && i == targetSubIndex);
                        if (drawSubTab(sub.title, isActive)) {
                            requestSubSwitch(i);
                        }
                        ImGui::SameLine(0, ImGui::GetFontSize());
                    }

                    ImGui::Dummy({0, ImGui::GetFontSize() * 2});

                    ImGui::BeginGroup();
                    {
                        beginMainContent(); // ← 淡入淡出控制开始
                        beginSubContent();

                        // 执行当前（或正在过渡的）内容
                        if (!currentMain.subItems.empty()) {
                            std::size_t idx = isSubTransitioning ? targetSubIndex : currentSubIndex;
                            if (idx < currentMain.subItems.size()) {
                                currentMain.subItems[idx].action(userData);
                            }
                        }

                        endSubContent();
                        endMainContent(); // ← 淡入淡出控制结束
                    }
                    ImGui::EndGroup();
                }
            }
            ImGui::EndGroup();

            // ImGui::PopStyleColor(4);
            // ImGui::PopStyleVar();
            ImGui::End();
        }

      private:
        // 自定义绘图函数（内部使用）
        bool drawMainTab(std::u8string_view label, std::u8string_view iconUtf8, bool isSelected) {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            const auto &style = ImGui::GetStyle();
            ImGuiID id = window->GetID(reinterpret_cast<const char *>(label.data()));
            float iconSize = ImGui::GetFontSize() * 2;
            const ImVec2 iconSz =
                ImGui::GetFont()->CalcTextSizeA(iconSize, FLT_MAX, 0, reinterpret_cast<const char *>(iconUtf8.data()));
            const ImVec2 textSz = ImGui::CalcTextSize(reinterpret_cast<const char *>(label.data()), nullptr, true);

            const float totalWidth = iconSz.x + textSz.x + 10.0f;
            const float totalHeight = ImMax(iconSz.y, textSz.y) + style.FramePadding.y * 2.0f;

            // 保持所有主导航 tab 宽度一致
            static float maxTabWidth = 0.0f;
            maxTabWidth = ImMax(maxTabWidth, totalWidth + style.FramePadding.x * 4.0f);
            // maxTabWidth = ImMax(maxTabWidth, static_cast<float>(ctx_.logoWidth));

            ImVec2 size = {maxTabWidth, totalHeight};
            ImVec2 pos = window->DC.CursorPos;
            ImRect bb(pos, pos + size);

            ImGui::ItemSize(size, style.FramePadding.y);
            if (!ImGui::ItemAdd(bb, id))
                return false;

            bool hovered, held;
            bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

            // 图标绘制发光特效
            if (isSelected) {
                glowCircle(bb.Min + iconSz / 2, 1.0f, iconSize / 2, IM_COL32(80, 180, 255, 255), 15);
            }

            ImU32 iconColor = isSelected ? theme.titleAction : ImGui::GetColorU32(ImGuiCol_Text);
            ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

            // 图标
            window->DrawList->AddText(ImGui::GetFont(), iconSize, bb.Min, iconColor,
                                      reinterpret_cast<const char *>(iconUtf8.data()));
            // 文字

            ImVec2 textPos = {bb.Min.x + iconSize, bb.GetCenter().y - (iconSz.y - textSz.y) * 0.5f};
            window->DrawList->AddText(textPos, textColor, reinterpret_cast<const char *>(label.data()));

            return pressed;
        }
        void glowCircle(const ImVec2 &center, float core_radius = 3.5f, float glow_radius = 14.0f,
                        ImU32 color = IM_COL32(255, 80, 220, 255), // 主体色
                        int glow_layers = 10, float thickness_base = 2.2f, int inner_alpha = 110) {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            if (glow_layers < 1)
                glow_layers = 1;
            if (glow_radius <= core_radius)
                glow_radius = core_radius + 6.0f;

            const float radius_step = (glow_radius - core_radius) / (float)glow_layers;
            const float alpha_step = (float)inner_alpha / (float)glow_layers;

            // 先画中心实心小圆（最亮的点）
            window->DrawList->AddCircleFilled(center, core_radius, color);

            // 再从内到外画多层透明描边，制造光晕
            for (int i = 1; i <= glow_layers; ++i) {
                float radius = core_radius + radius_step * (float)i;
                float thickness = thickness_base * (1.0f - 0.6f * (float)i / (float)glow_layers); // 越外越细
                int alpha = inner_alpha - (int)(alpha_step * (float)(i - 1));

                // alpha 最低控制在 8～12 左右，避免完全看不见但又不突兀
                if (alpha < 10)
                    alpha = 10;

                ImU32 col = (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);

                window->DrawList->AddCircle(center, radius, col, 0, thickness);
            }
        }
        bool drawSubTab(std::u8string_view label, bool isSelected) {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            const auto &style = ImGui::GetStyle();
            ImGuiID id = window->GetID(reinterpret_cast<const char *>(label.data()));
            ImVec2 labelSize = ImGui::CalcTextSize(reinterpret_cast<const char *>(label.data()), nullptr, true);

            ImVec2 pos = window->DC.CursorPos;
            ImVec2 size = ImGui::CalcItemSize({}, labelSize.x + style.FramePadding.x * 2.0f,
                                              labelSize.y + style.FramePadding.y * 2.0f);

            ImRect bb(pos, pos + size);
            ImGui::ItemSize(size, style.FramePadding.y);
            if (!ImGui::ItemAdd(bb, id))
                return false;

            bool hovered, held;
            bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

            // 下划线动画长度
            float underlineLen = animations::fastLerpFloat(id, isSelected || hovered, 0.0f, bb.GetWidth(), 5.0f);
            underlineLen = (underlineLen < 1.0f) ? 0.0f : underlineLen;

            // 蓝色下划线（可提取为主题色）
            ImU32 underlineColor = theme.titleAction;
            window->DrawList->AddRectFilled(ImVec2(bb.GetCenter().x - underlineLen * 0.5f, bb.Max.y),
                                            ImVec2(bb.GetCenter().x + underlineLen * 0.5f, bb.Max.y + 2.0f),
                                            underlineColor);

            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text));
            ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding,
                                     reinterpret_cast<const char *>(label.data()), nullptr, &labelSize,
                                     style.ButtonTextAlign, &bb);
            ImGui::PopStyleColor();

            return pressed;
        }

        std::vector<MainItem> mainItems_;
        std::size_t activeMainIndex_{0};
    };

    inline void setupImVeilTheme() {
        ImGuiStyle &style = ImGui::GetStyle();

        bool darkMode = false;

        // 共通的圓角設定（兩種模式都用）
        style.WindowRounding = 0.0f;
        style.ChildRounding = 6.0f;
        style.PopupRounding = 6.0f;
        style.FrameRounding = 0.0f;
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding = 5.0f;
        style.TabRounding = 5.0f;

        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;

        ImVec4 *colors = style.Colors;

        if (darkMode) {
            // ── 深色模式 ────────────────────────────────────────────────
            ImGui::StyleColorsDark();

            // 你原本的核心顏色
            ImVec4 accent = ImVec4(142 / 255.f, 132 / 255.f, 255 / 255.f, 1.00f); // #8e84ff
            ImVec4 bg_base = ImVec4(37 / 255.f, 36 / 255.f, 53 / 255.f, 1.00f);   // #252435

            colors[ImGuiCol_WindowBg] = bg_base;
            colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
            colors[ImGuiCol_PopupBg] = ImVec4(bg_base.x, bg_base.y, bg_base.z, 0.94f);

            colors[ImGuiCol_Border] = ImVec4(65 / 255.f, 69 / 255.f, 89 / 255.f, 0.50f);

            colors[ImGuiCol_FrameBg] = ImVec4(81 / 255.f, 87 / 255.f, 109 / 255.f, 0.80f);
            colors[ImGuiCol_FrameBgHovered] = ImLerp(colors[ImGuiCol_FrameBg], accent, 0.25f);
            colors[ImGuiCol_FrameBgActive] = ImLerp(colors[ImGuiCol_FrameBg], accent, 0.50f);

            colors[ImGuiCol_TitleBg] = ImVec4(30 / 255.f, 29 / 255.f, 45 / 255.f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(30 / 255.f, 29 / 255.f, 45 / 255.f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(30 / 255.f, 29 / 255.f, 45 / 255.f, 0.70f);

            colors[ImGuiCol_CheckMark] = accent;
            colors[ImGuiCol_SliderGrab] = accent;
            colors[ImGuiCol_SliderGrabActive] = ImLerp(accent, ImVec4(1, 1, 1, 1), 0.30f);

            colors[ImGuiCol_Button] = ImVec4(48 / 255.f, 49 / 255.f, 68 / 255.f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImLerp(colors[ImGuiCol_Button], accent, 0.30f);
            colors[ImGuiCol_ButtonActive] = ImLerp(colors[ImGuiCol_Button], accent, 0.55f);

            colors[ImGuiCol_Header] = ImVec4(81 / 255.f, 87 / 255.f, 109 / 255.f, 0.40f);
            colors[ImGuiCol_HeaderHovered] = ImLerp(colors[ImGuiCol_Header], accent, 0.40f);
            colors[ImGuiCol_HeaderActive] = ImLerp(colors[ImGuiCol_Header], accent, 0.60f);
        } else {
            // ── 亮色模式 ────────────────────────────────────────────────
            ImGui::StyleColorsLight();

            // 你原本的核心顏色
            ImVec4 accent = ImVec4(0 / 255.f, 106 / 255.f, 255 / 255.f, 1.00f); // #006aff
            ImVec4 bg_tint = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);                // 半透藍

            colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
            colors[ImGuiCol_PopupBg] = ImVec4(0.96f, 0.96f, 0.98f, 0.98f);

            colors[ImGuiCol_FrameBg] = ImColor(0.26f, 0.59f, 0.98f, 0.40f);
            colors[ImGuiCol_FrameBgHovered] = ImLerp(colors[ImGuiCol_FrameBg], accent, 0.20f);
            colors[ImGuiCol_FrameBgActive] = ImLerp(colors[ImGuiCol_FrameBg], accent, 0.40f);

            colors[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);

            colors[ImGuiCol_CheckMark] = accent;
            colors[ImGuiCol_SliderGrab] = accent;
            colors[ImGuiCol_SliderGrabActive] = ImLerp(accent, ImVec4(0, 0, 0, 1), 0.20f);

            colors[ImGuiCol_Button] = ImColor(0.26f, 0.59f, 0.98f, 0.40f);
            colors[ImGuiCol_ButtonHovered] = ImLerp(colors[ImGuiCol_Button], accent, 0.25f);
            colors[ImGuiCol_ButtonActive] = ImLerp(colors[ImGuiCol_Button], accent, 0.45f);

            colors[ImGuiCol_Header] = ImColor(0.26f, 0.59f, 0.98f, 0.40f);
            colors[ImGuiCol_HeaderHovered] = ImLerp(colors[ImGuiCol_Header], accent, 0.30f);
            colors[ImGuiCol_HeaderActive] = ImLerp(colors[ImGuiCol_Header], accent, 0.50f);
        }

        // 如果你之後想把 bg_tint 用在自訂區域（像 BeginRegion 的背景），就自己保留變數或另外處理
        // 這裡不強制塞進全局 Colors
    }
} // namespace ImVeil