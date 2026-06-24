#include "gui/MainGui.hpp"
#include "app/GuiApplication.hpp"
#include "backends/ILive2DBackend.hpp"
#include "render/ModelScanner.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "util/PathUtil.hpp"
#include "util/NativeDialogs.hpp"
#include "gui/ExportDialogs.hpp"
#include "gui/ConsoleLog.hpp"

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

namespace l2dgui
{

static void copyPathToBuffer(char* buffer, size_t size, const std::filesystem::path& p)
{
    const auto s = utf8Path(p);
    std::snprintf(buffer, size, "%s", s.c_str());
}

static bool ToggleButton(const char* label, bool* v)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float height = ImGui::GetFrameHeight();
    float width = height * 1.55f;
    float radius = height * 0.50f;

    ImGui::InvisibleButton(label, ImVec2(width, height));
    bool clicked = false;
    if (ImGui::IsItemClicked())
    {
        *v = !*v;
        clicked = true;
    }

    ImU32 col_bg = *v ? IM_COL32(76, 217, 100, 255) : IM_COL32(220, 220, 220, 255);

    draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, radius);
    draw_list->AddCircleFilled(ImVec2(*v ? (p.x + width - radius) : (p.x + radius), p.y + radius), radius - 1.5f, IM_COL32(255, 255, 255, 255));
    
    if (label && label[0] != '#' && label[1] != '#')
    {
        ImGui::SameLine();
        ImGui::Text("%s", label);
    }
    return clicked;
}

static void DrawUserIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    float head_radius = size * 0.26f;
    ImVec2 head_center = ImVec2(center.x, center.y - size * 0.18f);
    draw_list->AddCircleFilled(head_center, head_radius, color);

    draw_list->PathClear();
    draw_list->PathArcTo(ImVec2(center.x, center.y + size * 0.45f), size * 0.40f, 1.0f * IM_PI, 2.0f * IM_PI);
    draw_list->PathLineTo(ImVec2(center.x - size * 0.40f, center.y + size * 0.45f));
    draw_list->PathFillConvex(color);
}

static void DrawGearIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    float r_out = size * 0.35f;
    float r_in = size * 0.15f;
    int teeth_count = 8;
    float tooth_width = size * 0.12f;
    float tooth_length = size * 0.15f;
    for (int i = 0; i < teeth_count; ++i)
    {
        float angle = i * (2.0f * IM_PI / teeth_count);
        float c = std::cos(angle);
        float s = std::sin(angle);
        ImVec2 p0 = ImVec2(center.x + r_in * c, center.y + r_in * s);
        ImVec2 p1 = ImVec2(center.x + (r_out + tooth_length) * c, center.y + (r_out + tooth_length) * s);
        draw_list->AddLine(p0, p1, color, tooth_width);
    }
    draw_list->AddCircle(center, r_out, color, 0, size * 0.12f);
    draw_list->AddCircle(center, r_in, color, 0, size * 0.08f);
}

static void DrawFolderIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    float w = size * 0.90f;
    float h = size * 0.70f;
    ImVec2 p_min = ImVec2(center.x - w * 0.5f, center.y - h * 0.45f);
    ImVec2 p_max = ImVec2(center.x + w * 0.5f, center.y + h * 0.55f);
    draw_list->AddRectFilled(ImVec2(p_min.x, p_min.y + h * 0.25f), p_max, color, 1.5f);
    ImVec2 tab_min = p_min;
    ImVec2 tab_max = ImVec2(p_min.x + w * 0.45f, p_min.y + h * 0.30f);
    draw_list->AddRectFilled(tab_min, tab_max, color, 1.0f);
}

static void DrawPlayIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    float r = size * 0.42f;
    ImVec2 p0 = ImVec2(center.x - r * 0.6f, center.y - r);
    ImVec2 p1 = ImVec2(center.x - r * 0.6f, center.y + r);
    ImVec2 p2 = ImVec2(center.x + r * 0.9f, center.y);
    draw_list->AddTriangleFilled(p0, p1, p2, color);
}

static void DrawPauseIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    float w = size * 0.18f;
    float h = size * 0.80f;
    float gap = size * 0.18f;
    ImVec2 l_min = ImVec2(center.x - gap * 0.5f - w, center.y - h * 0.5f);
    ImVec2 l_max = ImVec2(center.x - gap * 0.5f, center.y + h * 0.5f);
    ImVec2 r_min = ImVec2(center.x + gap * 0.5f, center.y - h * 0.5f);
    ImVec2 r_max = ImVec2(center.x + gap * 0.5f + w, center.y + h * 0.5f);
    draw_list->AddRectFilled(l_min, l_max, color, 1.0f);
    draw_list->AddRectFilled(r_min, r_max, color, 1.0f);
}

static void DrawStopIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    float r = size * 0.38f;
    ImVec2 p_min = ImVec2(center.x - r, center.y - r);
    ImVec2 p_max = ImVec2(center.x + r, center.y + r);
    draw_list->AddRectFilled(p_min, p_max, color, 1.5f);
}

static void DrawResetIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    float radius = size * 0.35f;
    float thickness = size * 0.12f;
    draw_list->PathClear();
    draw_list->PathArcTo(center, radius, -0.6f * IM_PI, 1.1f * IM_PI);
    draw_list->PathStroke(color, false, thickness);
    ImVec2 tip_pt = ImVec2(center.x, center.y - radius);
    ImVec2 p1 = ImVec2(center.x + size * 0.25f, center.y - radius - size * 0.22f);
    ImVec2 p2 = ImVec2(center.x + size * 0.25f, center.y - radius + size * 0.22f);
    draw_list->AddTriangleFilled(tip_pt, p1, p2, color);
}

static void DrawSaveIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    float w = size * 0.70f;
    float h = size * 0.70f;
    ImVec2 p_min = ImVec2(center.x - w * 0.5f, center.y - h * 0.5f);
    ImVec2 p_max = ImVec2(center.x + w * 0.5f, center.y + h * 0.5f);
    
    // Outer floppy shape with clipped top-right corner
    draw_list->PathClear();
    draw_list->PathLineTo(p_min); // top-left
    draw_list->PathLineTo(ImVec2(p_max.x - w * 0.20f, p_min.y)); // top-right before clip
    draw_list->PathLineTo(ImVec2(p_max.x, p_min.y + h * 0.20f)); // top-right after clip
    draw_list->PathLineTo(p_max); // bottom-right
    draw_list->PathLineTo(ImVec2(p_min.x, p_max.y)); // bottom-left
    draw_list->PathFillConvex(color);

    // Label area (top mini rect)
    ImVec2 lbl_min = ImVec2(center.x - w * 0.25f, p_min.y + h * 0.10f);
    ImVec2 lbl_max = ImVec2(center.x + w * 0.15f, p_min.y + h * 0.35f);
    draw_list->AddRectFilled(lbl_min, lbl_max, IM_COL32(255, 255, 255, 255), 0.5f);

    // Sliding shutter (bottom mini rect)
    ImVec2 shut_min = ImVec2(center.x - w * 0.25f, center.y + h * 0.10f);
    ImVec2 shut_max = ImVec2(center.x + w * 0.25f, p_max.y);
    draw_list->AddRectFilled(shut_min, shut_max, IM_COL32(200, 200, 200, 255), 1.0f);
}

static void DrawLoadIcon(ImDrawList* draw_list, ImVec2 center, float size, ImU32 color)
{
    // Folder with an arrow pointing up
    DrawFolderIcon(draw_list, center, size, color);

    // Mini green arrow pointing up
    float r = size * 0.22f;
    ImVec2 arrow_center = ImVec2(center.x, center.y + size * 0.08f);
    ImVec2 p0 = ImVec2(arrow_center.x, arrow_center.y - r);
    ImVec2 p1 = ImVec2(arrow_center.x - r * 0.7f, arrow_center.y + r * 0.3f);
    ImVec2 p2 = ImVec2(arrow_center.x + r * 0.7f, arrow_center.y + r * 0.3f);
    draw_list->AddTriangleFilled(p0, p1, p2, IM_COL32(76, 217, 100, 255));
}


void MainGui::drawIconSidebar(GuiApplication& app)
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(40.0f, vp->WorkSize.y));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // Push light gray background for sidebar window
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.96f, 0.96f, 0.97f, 1.00f));

    ImGui::Begin("SidebarWindow", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    float width = ImGui::GetWindowWidth();

    auto drawIconBtn = [&](ActivePanel panel, int iconType, const char* tooltip) {
        bool active = (_activePanel == panel);
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        char btnId[32];
        std::snprintf(btnId, sizeof(btnId), "##sidebar_btn_%d", panel);
        if (ImGui::Button(btnId, ImVec2(width, 40.0f)))
        {
            _activePanel = panel;
        }
        ImGui::PopStyleVar();

        ImVec2 pMin = ImGui::GetItemRectMin();
        ImVec2 pMax = ImGui::GetItemRectMax();
        ImVec2 center = ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
        ImU32 iconColor = ImGui::GetColorU32(active ? ImGuiCol_Text : ImGuiCol_TextDisabled);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (iconType == 0) DrawUserIcon(drawList, center, 18.0f, iconColor);
        else if (iconType == 1) DrawGearIcon(drawList, center, 18.0f, iconColor);
        else if (iconType == 2) DrawFolderIcon(drawList, center, 18.0f, iconColor);

        if (active)
        {
            drawList->AddRectFilled(
                ImVec2(pMin.x, pMin.y),
                ImVec2(pMin.x + 3.0f, pMax.y),
                ImColor(24, 119, 242, 255) // Premium Blue
            );
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tooltip);
        }

        ImGui::PopStyleColor();
    };

    drawIconBtn(PANEL_MODELS, 0, "Models");
    drawIconBtn(PANEL_VIEWPORT_SETTINGS, 1, "Viewport Settings");
    drawIconBtn(PANEL_FILE_BROWSER, 2, "File Browser");

    // Align Save/Load buttons at the bottom of the sidebar
    float cursorY = ImGui::GetCursorPosY();
    float winHeight = ImGui::GetWindowHeight();
    float itemHeight = 40.0f + 8.0f; // button + spacing
    float neededY = winHeight - (itemHeight * 2.0f) - 8.0f;
    if (cursorY < neededY)
    {
        ImGui::SetCursorPosY(neededY);
    }

    auto drawActionBtn = [&](int actionType, const char* tooltip) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        char btnId[32];
        std::snprintf(btnId, sizeof(btnId), "##sidebar_action_%d", actionType);
        bool clicked = ImGui::Button(btnId, ImVec2(width, 40.0f));
        ImGui::PopStyleVar();

        ImVec2 pMin = ImGui::GetItemRectMin();
        ImVec2 pMax = ImGui::GetItemRectMax();
        ImVec2 btn_center = ImVec2((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
        ImU32 iconColor = ImGui::GetColorU32(ImGuiCol_Text);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (actionType == 0) DrawSaveIcon(drawList, btn_center, 18.0f, iconColor);
        else if (actionType == 1) DrawLoadIcon(drawList, btn_center, 18.0f, iconColor);

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tooltip);
        }

        ImGui::PopStyleColor();
        return clicked;
    };

    if (drawActionBtn(0, "Save Project (Ctrl+S)"))
    {
        app.saveProject();
    }
    if (drawActionBtn(1, "Open Project (Ctrl+O)"))
    {
        app.loadProject();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void MainGui::drawModelListPanel(AppState& state, ILive2DBackend& backend, GuiApplication& app)
{
    static ImVec2 marqueeStartPos;
    static bool isDraggingMarquee = false;
    static std::vector<bool> marqueeStartSelection;

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        isDraggingMarquee = false;
    }

    ImGui::Text("Models");
    ImGui::Separator();

    struct ModelRect {
        size_t index;
        ImVec2 min;
        ImVec2 max;
    };
    std::vector<ModelRect> itemRects;

    if (ImGui::BeginTable("ModelListTable", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders, ImVec2(0, 160.0f)))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Show", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(0);
        ImGui::TableHeader("Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::TableHeader("Show");
        if (ImGui::IsItemClicked() && !state.models.empty())
        {
            bool allVisible = true;
            for (const auto& m : state.models)
            {
                if (!m.visible)
                {
                    allVisible = false;
                    break;
                }
            }
            bool nextState = !allVisible;
            for (auto& m : state.models)
            {
                m.visible = nextState;
            }
        }

        for (size_t visual_idx = 0; visual_idx < state.models.size(); ++visual_idx)
        {
            size_t i = state.models.size() - 1 - visual_idx;
            ModelEntry& model = state.models[i];
            ImGui::PushID(static_cast<int>(model.id));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            bool isSelected = model.selected;
            if (ImGui::Selectable(model.name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                _lastFocusedTable = FOCUS_MODELS;
                if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift)
                {
                    state.clearSelection();
                    model.selected = true;
                }
                else if (ImGui::GetIO().KeyCtrl)
                {
                    model.selected = !model.selected;
                }
                else if (ImGui::GetIO().KeyShift)
                {
                    size_t firstSelected = i;
                    for (size_t k = 0; k < state.models.size(); ++k)
                    {
                        if (state.models[k].selected)
                        {
                            firstSelected = k;
                            break;
                        }
                    }
                    size_t start = std::min(firstSelected, i);
                    size_t end = std::max(firstSelected, i);
                    for (size_t k = start; k <= end; ++k)
                    {
                        state.models[k].selected = true;
                    }
                }
            }

            // Drag and Drop Source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("DND_MODEL_INDEX", &i, sizeof(size_t));
                ImGui::Text("Move %s", model.name.c_str());
                ImGui::EndDragDropSource();
            }

            // Drag and Drop Target
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MODEL_INDEX", ImGuiDragDropFlags_AcceptBeforeDelivery))
                {
                    IM_ASSERT(payload->DataSize == sizeof(size_t));
                    size_t dragIdx = *(const size_t*)payload->Data;

                    // Get mouse hover position to determine top or bottom half insertion
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    ImVec2 rowMin = ImGui::GetItemRectMin();
                    ImVec2 rowMax = ImGui::GetItemRectMax();
                    float rowMidY = (rowMin.y + rowMax.y) * 0.5f;
                    bool insertBefore = (mousePos.y < rowMidY);

                    // Draw insertion line at the boundary spanning the full row
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    float lineY = insertBefore ? rowMin.y : rowMax.y;
                    float rowEndX = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().ScrollbarSize;
                    drawList->AddLine(ImVec2(rowMin.x, lineY), ImVec2(rowEndX, lineY), ImColor(18, 122, 245, 255), 2.5f);

                    if (payload->IsDelivery())
                    {
                        std::vector<size_t> draggedIndices;
                        if (state.models[dragIdx].selected)
                        {
                            for (size_t k = 0; k < state.models.size(); ++k)
                            {
                                if (state.models[k].selected) draggedIndices.push_back(k);
                            }
                        }
                        else
                        {
                            draggedIndices.push_back(dragIdx);
                        }

                        // Verify target is not in the dragged set
                        bool targetIsDragged = false;
                        for (size_t idx : draggedIndices)
                        {
                            if (idx == i)
                            {
                                targetIsDragged = true;
                                break;
                            }
                        }

                        if (!targetIsDragged)
                        {
                            size_t insertAtVectorIdx = insertBefore ? (i + 1) : i;

                            std::vector<ModelEntry> tempItems;
                            for (size_t idx : draggedIndices)
                            {
                                tempItems.push_back(std::move(state.models[idx]));
                            }

                            // Erase from original in reverse order
                            for (auto it = draggedIndices.rbegin(); it != draggedIndices.rend(); ++it)
                            {
                                state.models.erase(state.models.begin() + *it);
                            }

                            // Adjust target insertion index based on erased items before it
                            size_t adjust = 0;
                            for (size_t idx : draggedIndices)
                            {
                                if (idx < insertAtVectorIdx)
                                {
                                    adjust++;
                                }
                            }
                            size_t targetIdx = (insertAtVectorIdx >= adjust) ? (insertAtVectorIdx - adjust) : 0;
                            if (targetIdx > state.models.size()) targetIdx = state.models.size();

                            state.models.insert(state.models.begin() + targetIdx, std::make_move_iterator(tempItems.begin()), std::make_move_iterator(tempItems.end()));
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Get item rect for marquee selection
            ImVec2 itemMin = ImGui::GetItemRectMin();
            ImVec2 itemMax = ImGui::GetItemRectMax();
            float rowEndX = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().ScrollbarSize;
            itemMax.x = std::max(itemMax.x, rowEndX);
            itemRects.push_back({i, itemMin, itemMax});

            if (ImGui::BeginPopupContextItem("model_context_new"))
            {
                if (ImGui::MenuItem("Move Up"))
                {
                    if (i + 1 < state.models.size()) std::swap(state.models[i], state.models[i + 1]);
                }
                if (ImGui::MenuItem("Move Down"))
                {
                    if (i > 0) std::swap(state.models[i], state.models[i - 1]);
                }
                if (ImGui::MenuItem("Select Only"))
                {
                    state.selectOnly(model.id);
                }
                if (ImGui::MenuItem("Unload"))
                {
                    std::vector<uint64_t> toUnload;
                    if (model.selected)
                    {
                        for (const auto& m : state.models)
                        {
                            if (m.selected) toUnload.push_back(m.id);
                        }
                    }
                    else
                    {
                        toUnload.push_back(model.id);
                    }

                    for (uint64_t id : toUnload)
                    {
                        backend.unloadModel(id);
                        auto it = std::find_if(state.models.begin(), state.models.end(), [id](const ModelEntry& m) { return m.id == id; });
                        if (it != state.models.end())
                        {
                            state.models.erase(it);
                        }
                    }
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Export"))
                {
                    if (ImGui::MenuItem("Export Single Frame..."))
                    {
                        _openExportFrame = true;
                    }
                    if (ImGui::MenuItem("Export Frame Sequence..."))
                    {
                        _openExportSequence = true;
                    }
                    if (ImGui::MenuItem("Export Video / GIF..."))
                    {
                        _openExportVideo = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            ImGui::TableNextColumn();
            ImGui::Checkbox("##visible", &model.visible);

            ImGui::PopID();
        }

        // Marquee selection logic
        ImGuiIO& io = ImGui::GetIO();
        if (!isDraggingMarquee)
        {
            bool isOverScrollbar = (io.MousePos.x >= ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().ScrollbarSize - 4.0f);
            if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && !isOverScrollbar)
            {
                _lastFocusedTable = FOCUS_MODELS;
                isDraggingMarquee = true;
                marqueeStartPos = io.MousePos;
                marqueeStartSelection.clear();
                for (const auto& m : state.models)
                {
                    marqueeStartSelection.push_back(m.selected);
                }
            }
        }

        if (isDraggingMarquee)
        {
            ImVec2 mousePos = io.MousePos;
            ImVec2 rectMin = ImVec2(std::min(marqueeStartPos.x, mousePos.x), std::min(marqueeStartPos.y, mousePos.y));
            ImVec2 rectMax = ImVec2(std::max(marqueeStartPos.x, mousePos.x), std::max(marqueeStartPos.y, mousePos.y));

            // Draw selection box in table's child window draw list (clipped to table)
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(rectMin, rectMax, ImColor(18, 122, 245, 30));
            drawList->AddRect(rectMin, rectMax, ImColor(18, 122, 245, 120), 0.0f, 0, 1.0f);

            // Compute overlap and update selections
            for (const auto& r : itemRects)
            {
                bool intersects = (r.min.x <= rectMax.x && r.max.x >= rectMin.x &&
                                   r.min.y <= rectMax.y && r.max.y >= rectMin.y);

                if (io.KeyCtrl)
                {
                    state.models[r.index].selected = marqueeStartSelection[r.index] || intersects;
                }
                else
                {
                    state.models[r.index].selected = intersects;
                }
            }
        }

        ImGui::EndTable();
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete) && _lastFocusedTable == FOCUS_MODELS)
    {
        std::vector<uint64_t> toUnload;
        for (const auto& m : state.models)
        {
            if (m.selected) toUnload.push_back(m.id);
        }
        for (uint64_t id : toUnload)
        {
            backend.unloadModel(id);
            auto it = std::find_if(state.models.begin(), state.models.end(), [id](const ModelEntry& m) { return m.id == id; });
            if (it != state.models.end())
            {
                state.models.erase(it);
            }
        }
    }

    size_t selectedCount = state.selectedModels().size();
    ImGui::Text("%d items, %d selected", (int)state.models.size(), (int)selectedCount);
    ImGui::Separator();

    auto selected = state.selectedModels();
    if (!selected.empty())
    {
        ModelEntry* activeModel = selected[0];
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.96f, 0.96f, 0.98f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.90f, 0.92f, 0.95f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0.96f, 0.96f, 0.98f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));

        if (ImGui::BeginTabBar("SubTabs"))
        {
            if (ImGui::BeginTabItem("Basic"))
            {
                if (ImGui::BeginChild("##tabBasic", ImVec2(0, 0), false))
                    drawTabBasic(*activeModel, app);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Render"))
            {
                if (ImGui::BeginChild("##tabRender", ImVec2(0, 0), false))
                    drawTabRender(state, *activeModel, backend, app);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Transform"))
            {
                if (ImGui::BeginChild("##tabTransform", ImVec2(0, 0), false))
                    drawTabTransform(state, *activeModel, app);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Expression"))
            {
                if (ImGui::BeginChild("##tabExpression", ImVec2(0, 0), false))
                    drawTabExpression(*activeModel, backend, app);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Parts"))
            {
                if (ImGui::BeginChild("##tabParts", ImVec2(0, 0), false))
                    drawTabParts(*activeModel, backend, app);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Motion"))
            {
                if (ImGui::BeginChild("##tabMotion", ImVec2(0, 0), false))
                    drawTabMotion(*activeModel, backend, app);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Debug"))
            {
                if (ImGui::BeginChild("##tabDebug", ImVec2(0, 0), false))
                    drawTabDebug(state, *activeModel, backend, app);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::PopStyleColor(5);
    }
    else
    {
        ImGui::TextDisabled("Select a model to view properties.");
    }

    // Deferred export popups (must be opened at root-level ID stack, not inside nested menus)
    if (_openExportFrame)  { ImGui::OpenPopup("Export Single Frame");  _openExportFrame  = false; }
    if (_openExportSequence) { ImGui::OpenPopup("Export Frame Sequence"); _openExportSequence = false; }
    if (_openExportVideo)  { ImGui::OpenPopup("Export Video / GIF");   _openExportVideo  = false; }

    // Draw export modals
    ExportDialogs::drawExportFrameDialog(state, app);
    ExportDialogs::drawExportSequenceDialog(state, app);
    ExportDialogs::drawExportVideoDialog(state, app);
}

void MainGui::drawTabBasic(ModelEntry& model, GuiApplication& app)
{
    ImGui::Text("Model Info");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 6.0f));
    if (ImGui::BeginTable("ModelBasicTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto addRow = [](const char* label, auto drawControl) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            drawControl();
        };

        addRow("Name", [&]() {
            ImGui::TextUnformatted(model.name.c_str());
        });
        addRow("Path", [&]() {
            std::string modelPath = utf8Path(model.model3Json);
            ImGui::TextWrapped("%s", modelPath.c_str());
        });
        addRow("Base Dir", [&]() {
            std::string baseDir = utf8Path(model.baseDir);
            ImGui::TextWrapped("%s", baseDir.c_str());
        });
        addRow("Motions", [&]() {
            ImGui::Text("%d", (int)model.motions.size());
        });
        addRow("Expressions", [&]() {
            ImGui::Text("%d", (int)model.expressions.size());
        });

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void MainGui::drawTabRender(AppState& state, ModelEntry& model, ILive2DBackend&, GuiApplication& app)
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 6.0f));
    if (ImGui::BeginTable("ModelRenderTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto addRow = [](const char* label, auto drawControl) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            drawControl();
        };

        addRow("Visible", [&]() {
            if (ToggleButton("##visible", &model.visible)) app.pushUndoState("Toggle Model Visibility");
        });
        addRow("Auto Idle", [&]() {
            if (ToggleButton("##autoidle", &model.autoIdle)) app.pushUndoState("Toggle Auto Idle");
        });
        addRow("Time Scale", [&]() {
            if (ImGui::DragFloat("##timescale", &model.timeScale, 0.05f, 0.0f, 4.0f, "%.2fx"))
            {
                model.timeScale = std::clamp(model.timeScale, 0.0f, 4.0f);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Time Scale");
        });
        addRow("Hide Pure Red", [&]() {
            if (ToggleButton("##hidered", &state.view.hideRed)) app.pushUndoState("Toggle Hide Red");
        });

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void MainGui::drawTabTransform(AppState&, ModelEntry& model, GuiApplication& app)
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 6.0f));
    if (ImGui::BeginTable("ModelTransformTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto addRow = [](const char* label, auto drawControl) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            drawControl();
        };

        addRow("Scale", [&]() {
            ImGui::DragFloat("##scale", &model.scale, 0.005f, 0.01f, 20.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Scale Model");
        });
        addRow("Flip X", [&]() {
            if (ToggleButton("##flipx", &model.flipX)) app.pushUndoState("Flip X");
        });
        addRow("Flip Y", [&]() {
            if (ToggleButton("##flipy", &model.flipY)) app.pushUndoState("Flip Y");
        });
        addRow("Position X", [&]() {
            ImGui::DragFloat("##posx", &model.x, 0.005f, -10.0f, 10.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Position X");
        });
        addRow("Position Y", [&]() {
            ImGui::DragFloat("##posy", &model.y, 0.005f, -10.0f, 10.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Position Y");
        });
        addRow("Rotation", [&]() {
            ImGui::DragFloat("##rot", &model.rotationDeg, 0.1f, -360.0f, 360.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Rotation");
        });
        addRow("Alpha", [&]() {
            ImGui::SliderFloat("##alpha", &model.alpha, 0.0f, 1.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Alpha");
        });
        addRow("Loop Motion", [&]() {
            if (ToggleButton("##loop", &model.loopMotion)) app.pushUndoState("Toggle Loop Motion");
        });

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void MainGui::drawTabExpression(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app)
{
    if (model.expressions.empty())
    {
        ImGui::TextDisabled("No expressions found.");
        return;
    }
    for (int i = 0; i < (int)model.expressions.size(); ++i)
    {
        bool active = (model.currentExpression == i);
        if (ImGui::Selectable(model.expressions[i].name.c_str(), active))
        {
            model.currentExpression = i;
            backend.setExpression(model.id, model.expressions[i]);
            app.pushUndoState("Change Expression");
        }
    }
}

void MainGui::drawTabParts(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app)
{
    if (ImGui::CollapsingHeader("Parts List", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int partCount = backend.getPartCount(model.id);
        if (partCount == 0)
        {
            ImGui::TextDisabled("No parts found.");
        }
        else
        {
            for (int i = 0; i < partCount; ++i)
            {
                const char* partId = backend.getPartId(model.id, i);
                float opacity = backend.getPartOpacity(model.id, i);
                bool visible = opacity > 0.5f;

                ImGui::PushID(i);
                if (ImGui::Checkbox(partId, &visible))
                {
                    backend.setPartOpacity(model.id, i, visible ? 1.0f : 0.0f);
                    app.pushUndoState("Toggle Part Visibility");
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f);
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::SliderFloat("##opacity", &opacity, 0.0f, 1.0f, ""))
                {
                    backend.setPartOpacity(model.id, i, opacity);
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    app.pushUndoState("Model Part Opacity");
                }
                ImGui::PopID();
            }
        }
    }

    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int paramCount = backend.getParameterCount(model.id);
        if (paramCount == 0)
        {
            ImGui::TextDisabled("No parameters found.");
        }
        else
        {
            for (int i = 0; i < paramCount; ++i)
            {
                const char* paramId = backend.getParameterId(model.id, i);
                float val = backend.getParameterValue(model.id, i);
                float minVal = backend.getParameterMin(model.id, i);
                float maxVal = backend.getParameterMax(model.id, i);
                float defVal = backend.getParameterDefault(model.id, i);

                ImGui::PushID(1000 + i);
                ImGui::TextUnformatted(paramId);
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 90.0f);
                if (ImGui::SliderFloat("##val", &val, minVal, maxVal, "%.2f"))
                {
                    backend.setParameterValue(model.id, i, val);
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    app.pushUndoState("Change Parameter Value");
                }
                ImGui::SameLine();
                if (ImGui::Button("R"))
                {
                    backend.setParameterValue(model.id, i, defVal);
                    app.pushUndoState("Reset Parameter Value");
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Default: %.2f", defVal);
                }
                ImGui::PopID();
            }
        }
    }
}

void MainGui::drawTabMotion(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app)
{
    static ImVec2 marqueeStartPos;
    static bool isDraggingMarquee = false;
    static std::vector<bool> marqueeStartSelection;
    static std::vector<PlaylistItem> g_playlistClipboard;

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        isDraggingMarquee = false;
    }

    if (model.motions.empty())
    {
        ImGui::TextDisabled("No motions found.");
        return;
    }

    // --- Clipboard Lambdas ---
    auto copySelected = [&](int fallbackIdx) {
        g_playlistClipboard.clear();
        for (const auto& item : model.playlist)
        {
            if (item.selected)
            {
                g_playlistClipboard.push_back(item);
            }
        }
        if (g_playlistClipboard.empty() && fallbackIdx >= 0 && fallbackIdx < (int)model.playlist.size())
        {
            g_playlistClipboard.push_back(model.playlist[fallbackIdx]);
        }
    };

    auto cutSelected = [&](int fallbackIdx) {
        copySelected(fallbackIdx);
        if (g_playlistClipboard.empty()) return;

        std::vector<int> toDelete;
        if (fallbackIdx >= 0 && fallbackIdx < (int)model.playlist.size() && !model.playlist[fallbackIdx].selected)
        {
            toDelete.push_back(fallbackIdx);
        }
        else
        {
            for (int k = 0; k < (int)model.playlist.size(); ++k)
            {
                if (model.playlist[k].selected) toDelete.push_back(k);
            }
        }

        std::sort(toDelete.rbegin(), toDelete.rend());
        for (int idx : toDelete)
        {
            model.playlist.erase(model.playlist.begin() + idx);
        }

        if (model.playlist.empty())
        {
            model.playlistActive = false;
            backend.stopPlaylist(model.id);
        }
        else
        {
            model.playlistActive = true;
            backend.startPlaylist(model.id, model.playlist, model.loopMotion);
        }
        app.pushUndoState("Cut Playlist Items");
    };

    auto pasteAt = [&](int insertAt) {
        if (g_playlistClipboard.empty()) return;
        if (insertAt < 0) insertAt = 0;
        if (insertAt > (int)model.playlist.size()) insertAt = (int)model.playlist.size();

        std::vector<PlaylistItem> tempItems;
        for (const auto& clipItem : g_playlistClipboard)
        {
            PlaylistItem newItem = clipItem;
            newItem.selected = false; // Paste된 항목은 선택 해제된 상태로 삽입
            tempItems.push_back(newItem);
        }

        model.playlist.insert(model.playlist.begin() + insertAt, tempItems.begin(), tempItems.end());
        model.playlistActive = true;
        backend.startPlaylist(model.id, model.playlist, model.loopMotion);
        app.pushUndoState("Paste Playlist Items");
    };
    // -------------------------

    struct PlaylistItemRect {
        size_t index;
        ImVec2 min;
        ImVec2 max;
    };
    std::vector<PlaylistItemRect> itemRects;

    // 탭 전체 가용 크기 획득
    ImVec2 avail = ImGui::GetContentRegionAvail();
    // 하단에 고정할 "Motion Transition" 영역의 필요 높이 계산 (약 60px)
    float fixedHeight = ImGui::GetFrameHeightWithSpacing() * 2.2f;
    float scrollHeight = avail.y - fixedHeight - ImGui::GetStyle().ItemSpacing.y * 3.0f;
    if (scrollHeight < 50.0f) scrollHeight = 50.0f; // 최소 높이 보장

    // 스크롤 가능한 상단 리스트 영역
    if (ImGui::BeginChild("##MotionPlaylistScroll", ImVec2(0, scrollHeight), false, ImGuiWindowFlags_NoScrollbar))
    {
        if (ImGui::BeginTable("PlaylistTable", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders, ImVec2(0, 0)))
    {
        ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Interp (s)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Motion Name", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(0);
        ImGui::TableHeader("Slot");
        ImGui::TableSetColumnIndex(1);
        ImGui::TableHeader("Interp (s)");
        ImGui::TableSetColumnIndex(2);
        ImGui::TableHeader("Motion Name");

        for (int i = 0; i < (int)model.playlist.size(); ++i)
        {
            auto& item = model.playlist[i];
            ImGui::PushID(i);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Slot column (selectable and drag source/target)
            std::string slotLabel = std::to_string(i);
            bool isSelected = item.selected;
            if (ImGui::Selectable(slotLabel.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                _lastFocusedTable = FOCUS_PLAYLIST;
                if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift)
                {
                    for (auto& it2 : model.playlist) it2.selected = false;
                    item.selected = true;
                }
                else if (ImGui::GetIO().KeyCtrl)
                {
                    item.selected = !item.selected;
                }
                else if (ImGui::GetIO().KeyShift)
                {
                    int firstSelected = i;
                    for (int k = 0; k < (int)model.playlist.size(); ++k)
                    {
                        if (model.playlist[k].selected)
                        {
                            firstSelected = k;
                            break;
                        }
                    }
                    int start = std::min(firstSelected, i);
                    int end = std::max(firstSelected, i);
                    for (int k = start; k <= end; ++k)
                    {
                        model.playlist[k].selected = true;
                    }
                }

                // Double click starts playlist execution immediately
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    model.playlistActive = true;
                    backend.startPlaylist(model.id, model.playlist, model.loopMotion);
                }
            }

            // Drag and drop source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("DND_PLAYLIST_INDEX", &i, sizeof(int));
                ImGui::Text("Move Slot %d", i);
                ImGui::EndDragDropSource();
            }

            // Drag and drop target
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PLAYLIST_INDEX", ImGuiDragDropFlags_AcceptBeforeDelivery))
                {
                    IM_ASSERT(payload->DataSize == sizeof(int));
                    int dragIdx = *(const int*)payload->Data;

                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    ImVec2 rowMin = ImGui::GetItemRectMin();
                    ImVec2 rowMax = ImGui::GetItemRectMax();
                    float rowMidY = (rowMin.y + rowMax.y) * 0.5f;
                    bool insertBefore = (mousePos.y < rowMidY);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    float lineY = insertBefore ? rowMin.y : rowMax.y;
                    float rowEndX = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().ScrollbarSize;
                    drawList->AddLine(ImVec2(rowMin.x, lineY), ImVec2(rowEndX, lineY), ImColor(18, 122, 245, 255), 2.5f);

                    if (payload->IsDelivery())
                    {
                        std::vector<int> draggedIndices;
                        if (model.playlist[dragIdx].selected)
                        {
                            for (int k = 0; k < (int)model.playlist.size(); ++k)
                            {
                                if (model.playlist[k].selected) draggedIndices.push_back(k);
                            }
                        }
                        else
                        {
                            draggedIndices.push_back(dragIdx);
                        }

                        bool targetIsDragged = false;
                        for (int idx : draggedIndices)
                        {
                            if (idx == i)
                            {
                                targetIsDragged = true;
                                break;
                            }
                        }

                        if (!targetIsDragged)
                        {
                            int insertAtIdx = insertBefore ? i : (i + 1);
                            std::vector<PlaylistItem> tempItems;
                            for (int idx : draggedIndices)
                            {
                                tempItems.push_back(model.playlist[idx]);
                            }

                            std::vector<int> sortedDragged = draggedIndices;
                            std::sort(sortedDragged.rbegin(), sortedDragged.rend());
                            for (int idx : sortedDragged)
                            {
                                model.playlist.erase(model.playlist.begin() + idx);
                            }

                            int adjust = 0;
                            for (int idx : draggedIndices)
                            {
                                if (idx < insertAtIdx) adjust++;
                            }
                            int targetIdx = insertAtIdx - adjust;
                            if (targetIdx < 0) targetIdx = 0;
                            if (targetIdx > (int)model.playlist.size()) targetIdx = (int)model.playlist.size();

                            model.playlist.insert(model.playlist.begin() + targetIdx, tempItems.begin(), tempItems.end());

                            model.playlistActive = true;
                            backend.startPlaylist(model.id, model.playlist, model.loopMotion);
                            app.pushUndoState("Reorder Playlist");
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImVec2 itemMin = ImGui::GetItemRectMin();
            ImVec2 itemMax = ImGui::GetItemRectMax();
            float rowEndX = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().ScrollbarSize;
            itemMax.x = std::max(itemMax.x, rowEndX);
            itemRects.push_back({(size_t)i, itemMin, itemMax});

            if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
            {
                if (ImGui::MenuItem("Copy", "Ctrl+C"))
                {
                    copySelected(i);
                }
                if (ImGui::MenuItem("Cut", "Ctrl+X"))
                {
                    cutSelected(i);
                }
                if (ImGui::MenuItem("Paste", "Ctrl+V", false, !g_playlistClipboard.empty()))
                {
                    pasteAt(i);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Delete"))
                {
                    std::vector<int> toDelete;
                    if (item.selected)
                    {
                        for (int k = 0; k < (int)model.playlist.size(); ++k)
                        {
                            if (model.playlist[k].selected) toDelete.push_back(k);
                        }
                    }
                    else
                    {
                        toDelete.push_back(i);
                    }
                    std::sort(toDelete.rbegin(), toDelete.rend());
                    for (int idx : toDelete)
                    {
                        model.playlist.erase(model.playlist.begin() + idx);
                    }
                    if (model.playlist.empty())
                    {
                        model.playlistActive = false;
                        backend.stopPlaylist(model.id);
                    }
                    else
                    {
                        model.playlistActive = true;
                        backend.startPlaylist(model.id, model.playlist, model.loopMotion);
                    }
                    app.pushUndoState("Delete Playlist Item");
                }
                ImGui::EndPopup();
            }

            // Fade-in time column
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat("##fade", &item.fadeTime, 0.05f, 0.0f, 10.0f, "%.3f s"))
            {
                _lastFocusedTable = FOCUS_PLAYLIST;
                item.fadeTime = std::max(0.0f, item.fadeTime);
                if (model.playlistActive)
                {
                    backend.startPlaylist(model.id, model.playlist, model.loopMotion);
                }
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                app.pushUndoState("Playlist Item Fade-in Time");
            }

            // Motion dropdown column
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            std::string currentLabel = "Select Motion...";
            int selectedMotionIdx = -1;
            for (int m_idx = 0; m_idx < (int)model.motions.size(); ++m_idx)
            {
                if (model.motions[m_idx].group == item.group && model.motions[m_idx].index == item.index)
                {
                    selectedMotionIdx = m_idx;
                    currentLabel = model.motions[m_idx].label();
                    break;
                }
            }

            std::string comboId = "##motion_combo_" + std::to_string(i);
            if (ImGui::BeginCombo(comboId.c_str(), currentLabel.c_str()))
            {
                _lastFocusedTable = FOCUS_PLAYLIST;
                for (int m_idx = 0; m_idx < (int)model.motions.size(); ++m_idx)
                {
                    const auto& mot = model.motions[m_idx];
                    bool isSel = (selectedMotionIdx == m_idx);
                    if (ImGui::Selectable(mot.label().c_str(), isSel))
                    {
                        item.group = mot.group;
                        item.index = mot.index;
                        float fi = backend.getMotionFadeInTime(model.id, mot.group, mot.index);
                        item.fadeTime = (fi >= 0.0f) ? fi : 0.5f;
                        model.playlistActive = true;
                        backend.startPlaylist(model.id, model.playlist, model.loopMotion);
                        app.pushUndoState("Select Playlist Motion");
                    }
                    if (isSel)
                    {
                        ImGui::SetItemDefaultFocus();
                        if (ImGui::IsWindowAppearing())
                        {
                            ImGui::SetScrollHereY(0.5f);
                        }
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::PopID();
        }

        // Marquee selection logic
        ImGuiIO& io = ImGui::GetIO();
        if (!isDraggingMarquee)
        {
            bool isOverScrollbar = (io.MousePos.x >= ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().ScrollbarSize - 4.0f);
            if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && !isOverScrollbar)
            {
                _lastFocusedTable = FOCUS_PLAYLIST;
                isDraggingMarquee = true;
                marqueeStartPos = io.MousePos;
                marqueeStartSelection.clear();
                for (const auto& item : model.playlist)
                {
                    marqueeStartSelection.push_back(item.selected);
                }
            }
        }

        if (isDraggingMarquee)
        {
            ImVec2 mousePos = io.MousePos;
            ImVec2 rectMin = ImVec2(std::min(marqueeStartPos.x, mousePos.x), std::min(marqueeStartPos.y, mousePos.y));
            ImVec2 rectMax = ImVec2(std::max(marqueeStartPos.x, mousePos.x), std::max(marqueeStartPos.y, mousePos.y));

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(rectMin, rectMax, ImColor(18, 122, 245, 30));
            drawList->AddRect(rectMin, rectMax, ImColor(18, 122, 245, 120), 0.0f, 0, 1.0f);

            for (const auto& r : itemRects)
            {
                bool intersects = (r.min.x <= rectMax.x && r.max.x >= rectMin.x &&
                                   r.min.y <= rectMax.y && r.max.y >= rectMin.y);

                if (io.KeyCtrl)
                {
                    model.playlist[r.index].selected = marqueeStartSelection[r.index] || intersects;
                }
                else
                {
                    model.playlist[r.index].selected = intersects;
                }
            }
        }

        ImGui::EndTable();
    }
    ImGui::EndChild();
}

    // Context menu trigger on empty area
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
    {
        ImGui::OpenPopup("playlist_empty_context");
    }
 
    if (ImGui::BeginPopup("playlist_empty_context"))
    {
        if (ImGui::MenuItem("Add"))
        {
            PlaylistItem newItem;
            if (!model.motions.empty())
            {
                int nextIdx = 0;
                if (!model.playlist.empty())
                {
                    const auto& lastItem = model.playlist.back();
                    int lastMotionIdx = -1;
                    for (int m_idx = 0; m_idx < (int)model.motions.size(); ++m_idx)
                    {
                        if (model.motions[m_idx].group == lastItem.group && model.motions[m_idx].index == lastItem.index)
                        {
                            lastMotionIdx = m_idx;
                            break;
                        }
                    }
                    if (lastMotionIdx != -1)
                    {
                        nextIdx = (lastMotionIdx + 1) % (int)model.motions.size();
                    }
                }
                const auto& nextMot = model.motions[nextIdx];
                newItem.group = nextMot.group;
                newItem.index = nextMot.index;
                float fi = backend.getMotionFadeInTime(model.id, nextMot.group, nextMot.index);
                newItem.fadeTime = (fi >= 0.0f) ? fi : 0.5f;
            }
            model.playlist.push_back(newItem);
            model.playlistActive = true;
            backend.startPlaylist(model.id, model.playlist, model.loopMotion);
            app.pushUndoState("Add Playlist Item");
        }

        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !g_playlistClipboard.empty()))
        {
            pasteAt((int)model.playlist.size());
        }

        if (ImGui::MenuItem("Clear"))
        {
            model.playlist.clear();
            model.playlistActive = false;
            backend.stopPlaylist(model.id);
            app.pushUndoState("Clear Playlist");
        }
        ImGui::EndPopup();
    }

    // Keyboard and Shortcut handling
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && _lastFocusedTable == FOCUS_PLAYLIST)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
        {
            copySelected(-1);
        }
        else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X))
        {
            cutSelected(-1);
        }
        else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
        {
            int insertAt = (int)model.playlist.size();
            for (int k = 0; k < (int)model.playlist.size(); ++k)
            {
                if (model.playlist[k].selected)
                {
                    insertAt = k;
                    break;
                }
            }
            pasteAt(insertAt);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            std::vector<int> toDelete;
            for (int k = 0; k < (int)model.playlist.size(); ++k)
            {
                if (model.playlist[k].selected) toDelete.push_back(k);
            }
            std::sort(toDelete.rbegin(), toDelete.rend());
            for (int idx : toDelete)
            {
                model.playlist.erase(model.playlist.begin() + idx);
            }
            if (model.playlist.empty())
            {
                model.playlistActive = false;
                backend.stopPlaylist(model.id);
            }
            else
            {
                model.playlistActive = true;
                backend.startPlaylist(model.id, model.playlist, model.loopMotion);
            }
            app.pushUndoState("Delete Selected Playlist Items");
        }
    }

    // 하단 고정 영역
    if (ImGui::BeginChild("##MotionTransitionFixed", ImVec2(0, fixedHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        // Play Sequence / Stop Sequence 버튼 제거 후, 대신:
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Motion Transition");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##blendDelta", &model.motionBlendDeltaThreshold,
                                0.0f, 1.0f, "Skip threshold: %.2f"))
        {
            // 실시간으로 bridge에 동기화 (백엔드 호출)
            backend.setMotionBlendThreshold(model.id, model.motionBlendDeltaThreshold);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "포즈 차이가 이 값보다 작으면 보간을 건너뜁니다.\n"
                "0 = 항상 보간 / 1 = 거의 항상 스킵");
        ImGui::EndChild();
    }
}

void MainGui::drawTabDebug(AppState& state, ModelEntry&, ILive2DBackend&, GuiApplication& app)
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 6.0f));
    if (ImGui::BeginTable("ModelDebugTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto addRow = [](const char* label, auto drawControl) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            drawControl();
        };

        addRow("Show Wireframe", [&]() {
            if (ToggleButton("##wireframe", &state.view.showWireframe)) app.pushUndoState("Toggle Wireframe");
        });
        addRow("Show Hit Areas", [&]() {
            if (ToggleButton("##hitareas", &state.view.showHitAreas)) app.pushUndoState("Toggle Hit Areas");
        });
        addRow("Show Bounds", [&]() {
            if (ToggleButton("##bounds", &state.view.debugBounds)) app.pushUndoState("Toggle Bounds");
        });
        addRow("Only Selected", [&]() {
            if (ToggleButton("##onlysel", &state.view.onlySelected)) app.pushUndoState("Toggle Only Selected");
        });

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void MainGui::drawViewportSettingsPanel(AppState& state, GuiApplication& app)
{
    ImGui::Text("Viewport Settings");
    ImGui::Separator();
    ImGui::Spacing();

    auto& camera = app.getCamera();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 6.0f));
    if (ImGui::BeginTable("ViewportSettingsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto addRow = [](const char* label, auto drawControl) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            drawControl();
        };

        addRow("Width (px)", [&]() {
            ImGui::InputInt("##width", &state.view.width, 0, 0);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Viewport Width");
        });
        addRow("Height (px)", [&]() {
            ImGui::InputInt("##height", &state.view.height, 0, 0);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Viewport Height");
        });
        addRow("Center X", [&]() {
            ImGui::DragFloat("##panx", &camera.panX, 0.01f);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Viewport Pan X");
        });
        addRow("Center Y", [&]() {
            ImGui::DragFloat("##pany", &camera.panY, 0.01f);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Viewport Pan Y");
        });
        addRow("Zoom", [&]() {
            ImGui::DragFloat("##zoom", &camera.zoom, 0.01f, 0.05f, 50.0f, "%.3f x");
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Viewport Zoom");
        });
        addRow("Rotation (Degrees)", [&]() {
            ImGui::DragFloat("##rot", &state.view.rotationDeg, 0.5f, -360.0f, 360.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Viewport Rotation");
        });
        addRow("Flip X", [&]() {
            if (ToggleButton("##flipx", &state.view.flipX)) app.pushUndoState("Toggle Viewport Flip X");
        });
        addRow("Flip Y", [&]() {
            if (ToggleButton("##flipy", &state.view.flipY)) app.pushUndoState("Toggle Viewport Flip Y");
        });
        addRow("Target FPS", [&]() {
            if (ImGui::DragInt("##fps", &state.view.targetFps, 1.0f, 10, 240, "%d FPS"))
            {
                state.view.targetFps = std::clamp(state.view.targetFps, 10, 240);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Viewport FPS");
        });
        addRow("Show Axis", [&]() {
            if (ToggleButton("##axis", &state.view.showAxis)) app.pushUndoState("Toggle Show Axis");
        });
        addRow("Background Transparent", [&]() {
            if (ToggleButton("##bg_trans", &state.view.transparentBackground)) app.pushUndoState("Toggle Transparent Background");
        });
        addRow("Background Color", [&]() {
            ImGui::ColorEdit4("##bg_color", state.view.bg, ImGuiColorEditFlags_NoLabel);
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Background Color");
        });
        addRow("Background Image Path", [&]() {
            char bgBuf[1024];
            std::snprintf(bgBuf, sizeof(bgBuf), "%s", state.view.bgImagePath.c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 35.0f);
            if (ImGui::InputText("##bgimage", bgBuf, sizeof(bgBuf)))
            {
                state.view.bgImagePath = bgBuf;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Change Background Image Path");
            ImGui::SameLine();
            if (ImGui::Button("...##bg", ImVec2(30.0f, 0)))
            {
                auto picked = NativeDialogs::pickFile(L"Image Files (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0");
                if (picked)
                {
                    state.view.bgImagePath = utf8Path(*picked);
                    app.pushUndoState("Change Background Image Path");
                }
            }
        });
        addRow("Background Image Mode", [&]() {
            const char* imgModes[] = { "None", "Fill", "Uniform", "UniformToFill" };
            if (ImGui::Combo("##bgmode", &state.view.bgImageMode, imgModes, IM_ARRAYSIZE(imgModes)))
            {
                app.pushUndoState("Change Background Image Mode");
            }
        });

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void MainGui::drawFileBrowserPanel(AppState& state, GuiApplication& app)
{
    static ImVec2 browserMarqueeStartPos;
    static bool isDraggingBrowserMarquee = false;
    static std::vector<std::filesystem::path> browserMarqueeStartSelection;
    static std::vector<std::filesystem::path> selectedPaths;
    static int lastClickedIndex = -1;
    static int sortDirection = 1; // 1 = Ascending, -1 = Descending

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        isDraggingBrowserMarquee = false;
    }

    ImGui::Text("File Browser");
    ImGui::Separator();

    static char filterBuf[256] = "";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
    ImGui::InputText("##filter", filterBuf, sizeof(filterBuf));
    ImGui::SameLine();

    if (ImGui::Button("Browse"))
    {
        auto folder = NativeDialogs::pickFolder();
        if (folder)
        {
            state.browserDir = *folder;
            state.browserModels = ModelScanner::findModels(state.browserDir, 5);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
    {
        state.browserModels = ModelScanner::findModels(state.browserDir, 5);
    }

    ImGui::Separator();

    std::string filterStr(filterBuf);
    std::vector<std::filesystem::path> filtered;
    for (const auto& p : state.browserModels)
    {
        std::string filename = utf8Path(p.filename());
        if (filterStr.empty() || filename.find(filterStr) != std::string::npos)
        {
            filtered.push_back(p);
        }
    }

    int currentSortDir = sortDirection;
    std::sort(filtered.begin(), filtered.end(), [currentSortDir](const std::filesystem::path& a, const std::filesystem::path& b) {
        std::string sa = utf8Path(a.filename());
        std::string sb = utf8Path(b.filename());
        if (currentSortDir == 1)
        {
            return sa < sb;
        }
        else
        {
            return sa > sb;
        }
    });

    struct BrowserItemRect {
        std::filesystem::path path;
        int index;
        ImVec2 rowMin;
        ImVec2 rowMax;
        ImVec2 textMin;
        ImVec2 textMax;
    };
    std::vector<BrowserItemRect> browserItemRects;

    std::string sortArrow = (sortDirection == 1) ? " [Asc]" : " [Desc]";
    if (ImGui::Selectable(("Name" + sortArrow).c_str(), false))
    {
        sortDirection = -sortDirection;
    }
    ImGui::Separator();

    if (ImGui::BeginChild("BrowserList", ImVec2(0, -60.0f), true))
    {
        for (int i = 0; i < (int)filtered.size(); ++i)
        {
            ImGui::PushID(i); // unique ID per item to avoid context menu conflicts
            const auto& p = filtered[i];
            std::string filename = utf8Path(p.filename());
            
            float textWidth = ImGui::CalcTextSize(filename.c_str()).x;
            bool isSelected = std::find(selectedPaths.begin(), selectedPaths.end(), p) != selectedPaths.end();
            
            // Draw custom full-width background highlight if selected
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float rowEndX = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().ScrollbarSize;
            float rowHeight = ImGui::GetFrameHeight();
            if (isSelected)
            {
                ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(rowEndX, pos.y + rowHeight), ImGui::GetColorU32(ImGuiCol_Header));
            }

            // Draw selectable with limited width (only covering the text)
            bool selectableClicked = ImGui::Selectable(filename.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(textWidth + 10.0f, 0.0f));
            
            ImVec2 textMin = ImGui::GetItemRectMin();
            ImVec2 textMax = ImGui::GetItemRectMax();
            ImVec2 rowMin = pos;
            ImVec2 rowMax = ImVec2(rowEndX, pos.y + rowHeight);
            browserItemRects.push_back({p, i, rowMin, rowMax, textMin, textMax});

            if (selectableClicked)
            {
                ImGuiIO& io = ImGui::GetIO();
                if (!io.KeyCtrl && !io.KeyShift)
                {
                    selectedPaths.clear();
                    selectedPaths.push_back(p);
                    lastClickedIndex = i;
                }
                else if (io.KeyCtrl)
                {
                    auto it = std::find(selectedPaths.begin(), selectedPaths.end(), p);
                    if (it != selectedPaths.end()) selectedPaths.erase(it);
                    else selectedPaths.push_back(p);
                    lastClickedIndex = i;
                }
                else if (io.KeyShift && lastClickedIndex >= 0 && lastClickedIndex < (int)filtered.size())
                {
                    int start = std::min(lastClickedIndex, i);
                    int end = std::max(lastClickedIndex, i);
                    selectedPaths.clear();
                    for (int k = start; k <= end; ++k)
                    {
                        selectedPaths.push_back(filtered[k]);
                    }
                }

                if (ImGui::IsMouseDoubleClicked(0))
                {
                    for (const auto& path : selectedPaths)
                    {
                        app.requestImportPath(path);
                    }
                }
            }

            if (ImGui::BeginPopupContextItem("file_ctx"))
            {
                if (ImGui::MenuItem("Import"))
                {
                    bool rightClickedSelected = std::find(selectedPaths.begin(), selectedPaths.end(), p) != selectedPaths.end();
                    if (!rightClickedSelected)
                    {
                        selectedPaths.clear();
                        selectedPaths.push_back(p);
                    }
                    for (const auto& path : selectedPaths)
                    {
                        app.requestImportPath(path);
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        // Marquee selection logic in File Browser
        ImGuiIO& io = ImGui::GetIO();
        bool isOverScrollbar = (io.MousePos.x >= ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().ScrollbarSize - 4.0f);
        
        if (!isDraggingBrowserMarquee)
        {
            if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !isOverScrollbar)
            {
                // Check if they clicked on the text of any item
                bool clickedOnText = false;
                ImVec2 clickPos = io.MouseClickedPos[0];
                for (const auto& r : browserItemRects)
                {
                    if (clickPos.x >= r.textMin.x && clickPos.x <= r.textMax.x &&
                        clickPos.y >= r.textMin.y && clickPos.y <= r.textMax.y)
                    {
                        clickedOnText = true;
                        break;
                    }
                }

                if (!clickedOnText)
                {
                    isDraggingBrowserMarquee = true;
                    browserMarqueeStartPos = clickPos;
                    browserMarqueeStartSelection = selectedPaths;
                }
            }
        }

        if (isDraggingBrowserMarquee)
        {
            ImVec2 mousePos = io.MousePos;
            ImVec2 rectMin = ImVec2(std::min(browserMarqueeStartPos.x, mousePos.x), std::min(browserMarqueeStartPos.y, mousePos.y));
            ImVec2 rectMax = ImVec2(std::max(browserMarqueeStartPos.x, mousePos.x), std::max(browserMarqueeStartPos.y, mousePos.y));

            // Draw selection box in child window draw list (clipped to child window)
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(rectMin, rectMax, ImColor(18, 122, 245, 30));
            drawList->AddRect(rectMin, rectMax, ImColor(18, 122, 245, 120), 0.0f, 0, 1.0f);

            // Compute overlap and update selections
            selectedPaths = browserMarqueeStartSelection;
            for (const auto& r : browserItemRects)
            {
                bool intersects = (r.rowMin.x <= rectMax.x && r.rowMax.x >= rectMin.x &&
                                   r.rowMin.y <= rectMax.y && r.rowMax.y >= rectMin.y);

                auto it = std::find(selectedPaths.begin(), selectedPaths.end(), r.path);
                if (intersects)
                {
                    if (it == selectedPaths.end()) selectedPaths.push_back(r.path);
                }
                else
                {
                    if (!io.KeyCtrl && it != selectedPaths.end()) selectedPaths.erase(it);
                }
            }
        }

        // Handle clicking on empty row space (select on release if no drag occurred)
        if (ImGui::IsMouseReleased(0) && ImGui::IsWindowHovered() && !isOverScrollbar)
        {
            if (!isDraggingBrowserMarquee)
            {
                ImVec2 clickPos = io.MouseClickedPos[0];
                ImVec2 releasePos = io.MousePos;
                float dragDist = std::sqrt((clickPos.x - releasePos.x)*(clickPos.x - releasePos.x) + (clickPos.y - releasePos.y)*(clickPos.y - releasePos.y));
                
                if (dragDist < 5.0f)
                {
                    bool clickedOnRow = false;
                    for (const auto& r : browserItemRects)
                    {
                        bool insideRow = (clickPos.x >= r.rowMin.x && clickPos.x <= r.rowMax.x &&
                                          clickPos.y >= r.rowMin.y && clickPos.y <= r.rowMax.y);
                        if (insideRow)
                        {
                            clickedOnRow = true;
                            bool insideText = (clickPos.x >= r.textMin.x && clickPos.x <= r.textMax.x &&
                                               clickPos.y >= r.textMin.y && clickPos.y <= r.textMax.y);
                            if (!insideText)
                            {
                                if (!io.KeyCtrl && !io.KeyShift)
                                {
                                    selectedPaths.clear();
                                    selectedPaths.push_back(r.path);
                                    lastClickedIndex = r.index;
                                }
                                else if (io.KeyCtrl)
                                {
                                    auto it = std::find(selectedPaths.begin(), selectedPaths.end(), r.path);
                                    if (it != selectedPaths.end()) selectedPaths.erase(it);
                                    else selectedPaths.push_back(r.path);
                                    lastClickedIndex = r.index;
                                }
                                else if (io.KeyShift && lastClickedIndex >= 0 && lastClickedIndex < (int)filtered.size())
                                {
                                    int start = std::min(lastClickedIndex, r.index);
                                    int end = std::max(lastClickedIndex, r.index);
                                    selectedPaths.clear();
                                    for (int k = start; k <= end; ++k)
                                    {
                                        selectedPaths.push_back(filtered[k]);
                                    }
                                }
                            }
                            break;
                        }
                    }

                    if (!clickedOnRow)
                    {
                        if (!io.KeyCtrl && !io.KeyShift)
                        {
                            selectedPaths.clear();
                        }
                    }
                }
            }
        }

        ImGui::EndChild();
    }

    if (selectedPaths.size() == 1)
    {
        ImGui::TextWrapped("File: %s", utf8Path(selectedPaths[0].filename()).c_str());
        ImGui::TextWrapped("Dir: %s", utf8Path(selectedPaths[0].parent_path()).c_str());
    }
    else if (selectedPaths.size() > 1)
    {
        ImGui::Text("%d items selected.", (int)selectedPaths.size());
    }
    else
    {
        ImGui::Text("%d models found.", (int)filtered.size());
    }
}

void MainGui::draw(AppState& state, ILive2DBackend& backend, GuiApplication& app)
{
    // Draw vertical icon sidebar (fixed 40px)
    drawIconSidebar(app);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float sidebarW = 40.0f;
    const float panelW   = state.view.leftPanelWidth - sidebarW; // active panel width

    // Draw active panel next to the sidebar
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + sidebarW, vp->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(panelW, vp->WorkSize.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("ActivePanelWindow", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    switch (_activePanel)
    {
    case PANEL_MODELS:
        drawModelListPanel(state, backend, app);
        break;
    case PANEL_VIEWPORT_SETTINGS:
        drawViewportSettingsPanel(state, app);
        break;
    case PANEL_FILE_BROWSER:
        drawFileBrowserPanel(state, app);
        break;
    }

    ImGui::End();
    ImGui::PopStyleVar();

    // Resizable splitter: thin drag zone between the left panel and viewport
    const float splitterW = 6.0f;
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + state.view.leftPanelWidth - splitterW * 0.5f, vp->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(splitterW, vp->WorkSize.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##splitter", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground);
    ImGui::InvisibleButton("##splitter_btn", ImVec2(splitterW, vp->WorkSize.y));
    if (ImGui::IsItemActive())
    {
        state.view.leftPanelWidth += ImGui::GetIO().MouseDelta.x;
        state.view.leftPanelWidth = std::clamp(state.view.leftPanelWidth, 200.0f, 700.0f);
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // Draw horizontal splitter between viewport and console log
    const float splitterH = 6.0f;
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + state.view.leftPanelWidth, vp->WorkPos.y + vp->WorkSize.y - state.view.consoleHeight - splitterH * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - state.view.leftPanelWidth, splitterH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##console_splitter", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground);
    ImGui::InvisibleButton("##console_splitter_btn", ImVec2(vp->WorkSize.x - state.view.leftPanelWidth, splitterH));
    if (ImGui::IsItemActive())
    {
        state.view.consoleHeight -= ImGui::GetIO().MouseDelta.y;
        state.view.consoleHeight = std::clamp(state.view.consoleHeight, 60.0f, 400.0f);
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // Draw console log panel below viewport
    const float leftW = state.view.leftPanelWidth;
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + leftW, vp->WorkPos.y + vp->WorkSize.y - state.view.consoleHeight));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - leftW, state.view.consoleHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("Console Log", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);
    ConsoleLog::getInstance().drawConsolePanel();
    ImGui::End();
    ImGui::PopStyleVar();

    drawPlaybackBar(state, backend);
}

void MainGui::drawModelPanel(AppState& state, ILive2DBackend& backend, GuiApplication& app)
{
    ImGui::TextWrapped("Drag & drop .model3.json or a folder into the window.");
    ImGui::InputText("Import path", _pathBuffer, sizeof(_pathBuffer));
    if (ImGui::Button("Import"))
    {
        if (_pathBuffer[0] != '\0')
        {
            app.requestImportPath(pathFromUtf8(_pathBuffer));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear selection")) state.clearSelection();

    ImGui::Separator();
    if (state.models.empty())
    {
        ImGui::TextDisabled("No model loaded.");
        return;
    }

    for (size_t i = 0; i < state.models.size(); ++i)
    {
        ModelEntry& model = state.models[i];
        ImGui::PushID(static_cast<int>(model.id));
        if (ImGui::Checkbox("##visible", &model.visible))
        {
            app.pushUndoState("Toggle Model Visibility");
        }
        ImGui::SameLine();
        if (ImGui::Button("^"))
        {
            if (i > 0)
            {
                std::swap(state.models[i], state.models[i - 1]);
                app.pushUndoState("Reorder Model");
                ImGui::PopID();
                break;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("v"))
        {
            if (i + 1 < state.models.size())
            {
                std::swap(state.models[i], state.models[i + 1]);
                app.pushUndoState("Reorder Model");
                ImGui::PopID();
                break;
            }
        }
        ImGui::SameLine();
        if (ImGui::Selectable(model.name.c_str(), model.selected, ImGuiSelectableFlags_AllowDoubleClick))
        {
            _lastFocusedTable = FOCUS_MODELS;
            if (!ImGui::GetIO().KeyCtrl) state.clearSelection();
            model.selected = !model.selected;
        }
        if (ImGui::BeginPopupContextItem("model_context"))
        {
            if (ImGui::MenuItem("Move Up"))
            {
                if (i > 0)
                {
                    std::swap(state.models[i], state.models[i - 1]);
                    app.pushUndoState("Reorder Model");
                }
            }
            if (ImGui::MenuItem("Move Down"))
            {
                if (i + 1 < state.models.size())
                {
                    std::swap(state.models[i], state.models[i + 1]);
                    app.pushUndoState("Reorder Model");
                }
            }
            if (ImGui::MenuItem("Select only"))
            {
                state.selectOnly(model.id);
                app.pushUndoState("Select Model Only");
            }
            if (ImGui::MenuItem("Unload"))
            {
                std::vector<uint64_t> toUnload;
                if (model.selected)
                {
                    for (const auto& m : state.models)
                    {
                        if (m.selected) toUnload.push_back(m.id);
                    }
                }
                else
                {
                    toUnload.push_back(model.id);
                }

                for (uint64_t id : toUnload)
                {
                    backend.unloadModel(id);
                    auto it = std::find_if(state.models.begin(), state.models.end(), [id](const ModelEntry& m) { return m.id == id; });
                    if (it != state.models.end())
                    {
                        state.models.erase(it);
                    }
                }
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }
        if (model.selected)
        {
            drawModelTransform(model, app);
            drawMotionControls(model, backend, app);
            drawPartsAndParameters(model, backend, app);
        }
        ImGui::PopID();
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete) && _lastFocusedTable == FOCUS_MODELS)
    {
        std::vector<uint64_t> toUnload;
        for (const auto& m : state.models)
        {
            if (m.selected) toUnload.push_back(m.id);
        }
        for (uint64_t id : toUnload)
        {
            backend.unloadModel(id);
            auto it = std::find_if(state.models.begin(), state.models.end(), [id](const ModelEntry& m) { return m.id == id; });
            if (it != state.models.end())
            {
                state.models.erase(it);
            }
        }
    }
}

void MainGui::drawModelTransform(ModelEntry& model, GuiApplication& app)
{
    if (ImGui::TreeNode("Transform"))
    {
        ImGui::DragFloat2("Position", &model.x, 0.005f, -10.0f, 10.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Position");
        ImGui::DragFloat("Scale", &model.scale, 0.005f, 0.01f, 20.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Scale Model");
        ImGui::DragFloat("Rotation", &model.rotationDeg, 0.1f, -360.0f, 360.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Rotation");
        ImGui::SliderFloat("Alpha", &model.alpha, 0.0f, 1.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Alpha");
        ImGui::SliderFloat("Time scale", &model.timeScale, 0.0f, 4.0f);
        if (ImGui::IsItemDeactivatedAfterEdit()) app.pushUndoState("Model Time Scale");
        if (ImGui::Checkbox("Loop motion", &model.loopMotion)) app.pushUndoState("Toggle Loop Motion");
        ImGui::SameLine();
        if (ImGui::Checkbox("Auto idle", &model.autoIdle)) app.pushUndoState("Toggle Auto Idle");
        ImGui::TreePop();
    }
}

void MainGui::drawMotionControls(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app)
{
    if (ImGui::TreeNode("Motions / Expressions"))
    {
        if (model.motions.empty())
        {
            ImGui::TextDisabled("No motions found in model3.json.");
        }
        else
        {
            std::string preview = model.currentMotion >= 0 && model.currentMotion < static_cast<int>(model.motions.size())
                ? model.motions[model.currentMotion].label() : "<select motion>";
            if (ImGui::BeginCombo("Motion", preview.c_str()))
            {
                for (int i = 0; i < static_cast<int>(model.motions.size()); ++i)
                {
                    bool selected = model.currentMotion == i;
                    if (ImGui::Selectable(model.motions[i].label().c_str(), selected))
                    {
                        model.currentMotion = i;
                        backend.playMotion(model.id, model.motions[i], model.loopMotion);
                        app.pushUndoState("Select Motion");
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Play"))
            {
                if (model.currentMotion >= 0 && model.currentMotion < static_cast<int>(model.motions.size()))
                    backend.playMotion(model.id, model.motions[model.currentMotion], model.loopMotion);
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop")) backend.stopMotion(model.id);
        }

        if (!model.expressions.empty())
        {
            std::string preview = model.currentExpression >= 0 && model.currentExpression < static_cast<int>(model.expressions.size())
                ? model.expressions[model.currentExpression].name : "<select expression>";
            if (ImGui::BeginCombo("Expression", preview.c_str()))
            {
                for (int i = 0; i < static_cast<int>(model.expressions.size()); ++i)
                {
                    bool selected = model.currentExpression == i;
                    if (ImGui::Selectable(model.expressions[i].name.c_str(), selected))
                    {
                        model.currentExpression = i;
                        backend.setExpression(model.id, model.expressions[i]);
                        app.pushUndoState("Select Expression");
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        ImGui::TreePop();
    }
}

void MainGui::drawBrowserPanel(AppState& state, GuiApplication& app)
{
    if (_browserBuffer[0] == '\0') copyPathToBuffer(_browserBuffer, sizeof(_browserBuffer), state.browserDir);
    ImGui::InputText("Folder", _browserBuffer, sizeof(_browserBuffer));
    if (ImGui::Button("Scan"))
    {
        state.browserDir = pathFromUtf8(_browserBuffer);
        state.browserModels = ModelScanner::findModels(state.browserDir, 5);
    }
    ImGui::SameLine();
    if (ImGui::Button("Import all"))
    {
        for (const auto& p : state.browserModels) app.requestImportPath(p);
    }
    ImGui::Separator();
    for (const auto& p : state.browserModels)
    {
        const auto id = utf8Path(p);
        ImGui::PushID(id.c_str());
        const auto filename = utf8Path(p.filename());
        const auto parent = utf8Path(p.parent_path());
        ImGui::TextWrapped("%s", filename.c_str());
        ImGui::TextDisabled("%s", parent.c_str());
        if (ImGui::Button("Import")) app.requestImportPath(p);
        ImGui::Separator();
        ImGui::PopID();
    }
}

void MainGui::drawStagePanel(AppState& state)
{
    ImGui::Checkbox("Transparent background", &state.view.transparentBackground);
    if (!state.view.transparentBackground)
    {
        ImGui::ColorEdit4("Background", state.view.bg);
    }
    ImGui::Checkbox("Show grid", &state.view.showGrid);
    ImGui::Checkbox("Only selected", &state.view.onlySelected);
    ImGui::Checkbox("Debug bounds", &state.view.debugBounds);
    ImGui::Checkbox("Hide pure red color", &state.view.hideRed);
    ImGui::SameLine();
    ImGui::Checkbox("Show wireframe", &state.view.showWireframe);
    ImGui::SameLine();
    ImGui::Checkbox("Show hit areas", &state.view.showHitAreas);
    ImGui::InputInt("Viewport width", &state.view.width);
    ImGui::InputInt("Viewport height", &state.view.height);
    ImGui::TextWrapped("Viewport controls intentionally follow the SpineViewer-style workflow: left panel for model state, right side for visual preview, mouse wheel zoom, right drag pan.");
}

void MainGui::drawExportPanel(AppState& state, GuiApplication& app, ILive2DBackend&)
{
    if (_outBuffer[0] == '\0') copyPathToBuffer(_outBuffer, sizeof(_outBuffer), state.exportSettings.outputPath);
    if (_ffmpegBuffer[0] == '\0') copyPathToBuffer(_ffmpegBuffer, sizeof(_ffmpegBuffer), state.exportSettings.ffmpegPath);

    ImGui::InputText("Output WebM", _outBuffer, sizeof(_outBuffer));
    ImGui::InputText("FFmpeg", _ffmpegBuffer, sizeof(_ffmpegBuffer));
    ImGui::InputInt("Width", &state.exportSettings.width);
    ImGui::InputInt("Height", &state.exportSettings.height);
    ImGui::InputInt("FPS", &state.exportSettings.fps);
    ImGui::DragFloat("Seconds", &state.exportSettings.seconds, 0.1f, 0.1f, 600.0f);
    ImGui::InputInt("Padding", &state.exportSettings.padding);
    ImGui::SliderInt("VP9 CRF", &state.exportSettings.crf, 4, 63);
    ImGui::Checkbox("Transparent alpha", &state.exportSettings.transparent);
    ImGui::Checkbox("Auto crop by alpha", &state.exportSettings.autoCrop);
    ImGui::Checkbox("Export selected only", &state.exportSettings.selectedOnly);
    ImGui::InputText("Extra ffmpeg args", _extraFfmpeg, sizeof(_extraFfmpeg));

    if (ImGui::Button("Export WebM Alpha"))
    {
        state.exportSettings.outputPath = pathFromUtf8(_outBuffer);
        state.exportSettings.ffmpegPath = pathFromUtf8(_ffmpegBuffer);
        state.exportSettings.extraFfmpegArgs = _extraFfmpeg;
        app.requestExport();
    }
    ImGui::ProgressBar(state.exportStatus.progress, ImVec2(-1.0f, 0.0f));
    ImGui::TextWrapped("%s", state.exportStatus.message.c_str());
    if (!state.exportStatus.lastCommand.empty())
    {
        if (ImGui::TreeNode("Last ffmpeg command"))
        {
            ImGui::TextWrapped("%s", state.exportStatus.lastCommand.c_str());
            ImGui::TreePop();
        }
    }
}

void MainGui::drawPlaybackBar(AppState& state, ILive2DBackend& backend)
{
    ModelEntry* target = nullptr;
    
    // 1. Find first model that is both selected and visible
    for (auto& m : state.models)
    {
        if (m.selected && m.visible)
        {
            target = &m;
            break;
        }
    }
    
    // 2. If none, find first visible model
    if (!target)
    {
        for (auto& m : state.models)
        {
            if (m.visible)
            {
                target = &m;
                break;
            }
        }
    }
    
    // 3. If none, find first selected model
    if (!target)
    {
        for (auto& m : state.models)
        {
            if (m.selected)
            {
                target = &m;
                break;
            }
        }
    }
    
    if (!target) return;

    std::vector<ModelEntry*> controlledModels;
    auto selected = state.selectedModels();
    if (!selected.empty())
    {
        controlledModels = selected;
    }
    else
    {
        controlledModels.push_back(target);
    }

    float curTime = backend.getMotionTime(target->id);
    float duration = backend.getMotionDuration(target->id);
    bool paused = backend.isMotionPaused(target->id);

    if (duration > 0.0f)
    {
        if (target->loopMotion)
        {
            curTime = std::fmod(curTime, duration);
            if (curTime < 0.0f) curTime = 0.0f;
        }
        else
        {
            curTime = std::min(curTime, duration);
        }
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float barHeight = 45.0f;
    const float leftW = state.view.leftPanelWidth;
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + leftW + 10.0f, vp->WorkPos.y + vp->WorkSize.y - state.view.consoleHeight - barHeight - 10.0f));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - leftW - 20.0f, barHeight));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.00f, 1.00f, 1.00f, 0.95f)); // Semi-transparent pure white
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));   // Transparent buttons
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.92f, 0.96f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.80f, 0.85f, 0.92f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::Begin("Playback Bar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    bool playClicked = false;
    if (paused)
    {
        playClicked = ImGui::Button("##play_btn", ImVec2(32.0f, 32.0f));
        ImVec2 pMin = ImGui::GetItemRectMin();
        ImVec2 pMax = ImGui::GetItemRectMax();
        DrawPlayIcon(ImGui::GetWindowDrawList(), ImVec2((pMin.x + pMax.x)*0.5f, (pMin.y + pMax.y)*0.5f), 14.0f, ImGui::GetColorU32(ImGuiCol_Text));
        if (playClicked)
        {
            for (auto* m : controlledModels)
            {
                backend.setMotionPaused(m->id, false);
                float ct = backend.getMotionTime(m->id);
                float dur = backend.getMotionDuration(m->id);
                if (m->playlistActive)
                {
                    if (dur > 0.0f && ct >= dur - 0.01f)
                    {
                        backend.startPlaylist(m->id, m->playlist, m->loopMotion);
                    }
                }
                else
                {
                    if (dur > 0.0f && ct >= dur - 0.01f)
                    {
                        backend.setMotionTime(m->id, 0.0f);
                    }
                }
            }
        }
    }
    else
    {
        playClicked = ImGui::Button("##pause_btn", ImVec2(32.0f, 32.0f));
        ImVec2 pMin = ImGui::GetItemRectMin();
        ImVec2 pMax = ImGui::GetItemRectMax();
        DrawPauseIcon(ImGui::GetWindowDrawList(), ImVec2((pMin.x + pMax.x)*0.5f, (pMin.y + pMax.y)*0.5f), 14.0f, ImGui::GetColorU32(ImGuiCol_Text));
        if (playClicked)
        {
            for (auto* m : controlledModels) backend.setMotionPaused(m->id, true);
        }
    }

    ImGui::SameLine();
    bool stopClicked = ImGui::Button("##stop_btn", ImVec2(32.0f, 32.0f));
    ImVec2 stopMin = ImGui::GetItemRectMin();
    ImVec2 stopMax = ImGui::GetItemRectMax();
    DrawStopIcon(ImGui::GetWindowDrawList(), ImVec2((stopMin.x + stopMax.x)*0.5f, (stopMin.y + stopMax.y)*0.5f), 14.0f, ImGui::GetColorU32(ImGuiCol_Text));
    if (stopClicked)
    {
        for (auto* m : controlledModels)
        {
            if (m->playlistActive)
            {
                backend.stopPlaylist(m->id);
            }
            else
            {
                backend.stopMotion(m->id);
            }
            backend.setMotionTime(m->id, 0.0f);
            backend.setMotionPaused(m->id, true);
        }
    }

    ImGui::SameLine();
    bool resetClicked = ImGui::Button("##reset_btn", ImVec2(32.0f, 32.0f));
    ImVec2 resetMin = ImGui::GetItemRectMin();
    ImVec2 resetMax = ImGui::GetItemRectMax();
    DrawResetIcon(ImGui::GetWindowDrawList(), ImVec2((resetMin.x + resetMax.x)*0.5f, (resetMin.y + resetMax.y)*0.5f), 14.0f, ImGui::GetColorU32(ImGuiCol_Text));
    if (resetClicked)
    {
        for (auto* m : controlledModels)
        {
            backend.setMotionTime(m->id, 0.0f);
            backend.setMotionPaused(m->id, false);
        }
    }

    ImGui::SameLine();
    float timeVal = curTime;
    float maxTime = duration > 0.0f ? duration : 1.0f;
    char timeStr[64];
    std::snprintf(timeStr, sizeof(timeStr), "%.2f / %.2f s", curTime, duration);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 170.0f);
    if (ImGui::SliderFloat("##timeline", &timeVal, 0.0f, maxTime, timeStr))
    {
        for (auto* m : controlledModels)
        {
            backend.setMotionTime(m->id, timeVal);
        }
    }

    ImGui::SameLine();
    ImGui::Text("Speed:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::DragFloat("##speed", &target->timeScale, 0.05f, 0.0f, 4.0f, "%.2fx"))
    {
        target->timeScale = std::clamp(target->timeScale, 0.0f, 4.0f);
        for (auto* m : controlledModels)
        {
            m->timeScale = target->timeScale;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);
}

void MainGui::drawPartsAndParameters(ModelEntry& model, ILive2DBackend& backend, GuiApplication& app)
{
    // Parts Tree
    if (ImGui::TreeNode("Parts"))
    {
        int partCount = backend.getPartCount(model.id);
        if (partCount == 0)
        {
            ImGui::TextDisabled("No parts found.");
        }
        else
        {
            for (int i = 0; i < partCount; ++i)
            {
                const char* partId = backend.getPartId(model.id, i);
                float opacity = backend.getPartOpacity(model.id, i);
                bool visible = opacity > 0.5f;

                ImGui::PushID(i);
                if (ImGui::Checkbox(partId, &visible))
                {
                    backend.setPartOpacity(model.id, i, visible ? 1.0f : 0.0f);
                    app.pushUndoState("Toggle Part Visibility");
                }
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }

    // Parameters Tree
    if (ImGui::TreeNode("Parameters"))
    {
        int paramCount = backend.getParameterCount(model.id);
        if (paramCount == 0)
        {
            ImGui::TextDisabled("No parameters found.");
        }
        else
        {
            for (int i = 0; i < paramCount; ++i)
            {
                const char* paramId = backend.getParameterId(model.id, i);
                float val = backend.getParameterValue(model.id, i);
                float minVal = backend.getParameterMin(model.id, i);
                float maxVal = backend.getParameterMax(model.id, i);
                float defVal = backend.getParameterDefault(model.id, i);

                ImGui::PushID(i);
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::SliderFloat(paramId, &val, minVal, maxVal, "%.2f"))
                {
                    backend.setParameterValue(model.id, i, val);
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    app.pushUndoState("Change Parameter Value");
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Default: %.2f", defVal);
                }
                ImGui::SameLine();
                if (ImGui::Button("R"))
                {
                    backend.setParameterValue(model.id, i, defVal);
                    app.pushUndoState("Reset Parameter Value");
                }
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }
}

} // namespace l2dgui
