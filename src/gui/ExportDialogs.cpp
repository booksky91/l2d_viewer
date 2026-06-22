#include "gui/ExportDialogs.hpp"
#include "app/AppState.hpp"
#include "app/GuiApplication.hpp"
#include "util/NativeDialogs.hpp"
#include "util/PathUtil.hpp"
#include "util/ProjectSerializer.hpp"
#include <imgui.h>
#include <cstdio>
#include <functional>

namespace l2dgui
{
namespace ExportDialogs
{

// Sync export dimensions from viewport on first call per popup opening.
static void syncViewportDimensions(ExportSettings& s, const AppState& state)
{
    if (s.width <= 0)  s.width  = state.view.width;
    if (s.height <= 0) s.height = state.view.height;
    if (s.width <= 0)  s.width  = 1920;
    if (s.height <= 0) s.height = 1080;
}

static void drawPropertyRow(const char* label, const std::function<void()>& drawControl)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1.0f);
    drawControl();
}

void drawExportFrameDialog(AppState& state, GuiApplication& app)
{
    if (ImGui::BeginPopupModal("Export Single Frame", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ExportSettings& s = state.exportSettings;
        syncViewportDimensions(s, state);

        static std::vector<std::string> presets;
        static std::string currentPresetName = "Select Preset...";
        if (ImGui::IsWindowAppearing())
        {
            presets = ProjectSerializer::listPresets();
            currentPresetName = "Select Preset...";
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Preset:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("##PresetCombo", currentPresetName.c_str()))
        {
            for (const auto& p : presets)
            {
                if (ImGui::Selectable(p.c_str(), currentPresetName == p))
                {
                    if (ProjectSerializer::loadPreset(s, p))
                    {
                        currentPresetName = p;
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Preset"))
        {
            ImGui::OpenPopup("Save Preset As");
        }

        if (ImGui::BeginPopupModal("Save Preset As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char presetNameBuf[128] = "";
            ImGui::InputText("Preset Name", presetNameBuf, sizeof(presetNameBuf));
            ImGui::Spacing();
            if (ImGui::Button("Save", ImVec2(100, 0)))
            {
                if (presetNameBuf[0] != '\0')
                {
                    if (ProjectSerializer::savePreset(s, presetNameBuf))
                    {
                        presets = ProjectSerializer::listPresets(); // refresh
                        currentPresetName = presetNameBuf;
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::Text("Base Settings");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTable("FrameParamsTable", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

            drawPropertyRow("Width", [&]() { ImGui::InputInt("##Width", &s.width); });
            drawPropertyRow("Height", [&]() { ImGui::InputInt("##Height", &s.height); });
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Export Single File (Composite)", &s.exportSingle);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Output Folder");
            ImGui::TableNextColumn();
            char folderBuf[1024];
            std::snprintf(folderBuf, sizeof(folderBuf), "%s", utf8Path(s.outputFolder).c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 45.0f);
            if (ImGui::InputText("##outfolder", folderBuf, sizeof(folderBuf)))
            {
                s.outputFolder = pathFromUtf8(folderBuf);
            }
            ImGui::SameLine();
            if (ImGui::Button("...##folder"))
            {
                auto picked = NativeDialogs::pickFolder();
                if (picked)
                {
                    s.outputFolder = *picked;
                }
            }

            drawPropertyRow("Background Color", [&]() { ImGui::ColorEdit4("##BgColor", s.bgColor); });
            drawPropertyRow("Margin", [&]() { ImGui::InputInt("##Margin", &s.margin); });

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Auto Resolution", &s.autoResolution);

            if (s.autoResolution)
            {
                drawPropertyRow("Max Resolution", [&]() { ImGui::InputInt("##MaxRes", &s.maxResolution); });
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Separator();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Image Format");
            ImGui::TableNextColumn();
            const char* formats[] = { "JPEG", "PNG", "WEBP" };
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::Combo("##Format", &s.imageFormat, formats, IM_ARRAYSIZE(formats));

            if (s.imageFormat == 0 || s.imageFormat == 2)
            {
                drawPropertyRow("Quality", [&]() { ImGui::SliderInt("##Quality", &s.imageQuality, 1, 100); });
            }

            drawPropertyRow("Render Scale", [&]() {
                const char* scales[] = { "1x", "2x", "4x" };
                int scaleIdx = (s.renderScale == 4) ? 2 : (s.renderScale == 2) ? 1 : 0;
                if (ImGui::Combo("##RenderScale", &scaleIdx, scales, IM_ARRAYSIZE(scales)))
                {
                    s.renderScale = (scaleIdx == 2) ? 4 : (scaleIdx == 1) ? 2 : 1;
                }
            });

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Export", ImVec2(120, 0)))
        {
            s.exportType = EXPORT_SINGLE_FRAME;
            app.requestExport();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void drawExportSequenceDialog(AppState& state, GuiApplication& app)
{
    if (ImGui::BeginPopupModal("Export Frame Sequence", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ExportSettings& s = state.exportSettings;
        syncViewportDimensions(s, state);

        static std::vector<std::string> presets;
        static std::string currentPresetName = "Select Preset...";
        if (ImGui::IsWindowAppearing())
        {
            presets = ProjectSerializer::listPresets();
            currentPresetName = "Select Preset...";
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Preset:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("##PresetCombo", currentPresetName.c_str()))
        {
            for (const auto& p : presets)
            {
                if (ImGui::Selectable(p.c_str(), currentPresetName == p))
                {
                    if (ProjectSerializer::loadPreset(s, p))
                    {
                        currentPresetName = p;
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Preset"))
        {
            ImGui::OpenPopup("Save Preset As");
        }

        if (ImGui::BeginPopupModal("Save Preset As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char presetNameBuf[128] = "";
            ImGui::InputText("Preset Name", presetNameBuf, sizeof(presetNameBuf));
            ImGui::Spacing();
            if (ImGui::Button("Save", ImVec2(100, 0)))
            {
                if (presetNameBuf[0] != '\0')
                {
                    if (ProjectSerializer::savePreset(s, presetNameBuf))
                    {
                        presets = ProjectSerializer::listPresets(); // refresh
                        currentPresetName = presetNameBuf;
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::Text("Base Settings");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTable("SequenceParamsTable", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

            drawPropertyRow("Width", [&]() { ImGui::InputInt("##Width", &s.width); });
            drawPropertyRow("Height", [&]() { ImGui::InputInt("##Height", &s.height); });
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Export Single File (Composite)", &s.exportSingle);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Output Folder");
            ImGui::TableNextColumn();
            char folderBuf[1024];
            std::snprintf(folderBuf, sizeof(folderBuf), "%s", utf8Path(s.outputFolder).c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 45.0f);
            if (ImGui::InputText("##outfolder", folderBuf, sizeof(folderBuf)))
            {
                s.outputFolder = pathFromUtf8(folderBuf);
            }
            ImGui::SameLine();
            if (ImGui::Button("...##folder"))
            {
                auto picked = NativeDialogs::pickFolder();
                if (picked)
                {
                    s.outputFolder = *picked;
                }
            }

            drawPropertyRow("Background Color", [&]() { ImGui::ColorEdit4("##BgColor", s.bgColor); });
            drawPropertyRow("Margin", [&]() { ImGui::InputInt("##Margin", &s.margin); });

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Auto Resolution", &s.autoResolution);

            if (s.autoResolution)
            {
                drawPropertyRow("Max Resolution", [&]() { ImGui::InputInt("##MaxRes", &s.maxResolution); });
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Separator();

            drawPropertyRow("Duration (s)", [&]() {
                ImGui::InputFloat("##Duration", &s.duration, 0.1f, 1.0f, "%.2f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("-1 = export exactly 1 full animation cycle");
            });
            drawPropertyRow("FPS", [&]() { ImGui::InputInt("##FPS", &s.fps); });
            drawPropertyRow("Export Speed", [&]() { ImGui::SliderFloat("##Speed", &s.exportSpeed, 0.01f, 10.0f, "%.2fx"); });

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Keep Last Frame", &s.keepLastFrame);

            drawPropertyRow("Render Scale", [&]() {
                const char* scales[] = { "1x", "2x", "4x" };
                int scaleIdx = (s.renderScale == 4) ? 2 : (s.renderScale == 2) ? 1 : 0;
                if (ImGui::Combo("##RenderScale", &scaleIdx, scales, IM_ARRAYSIZE(scales)))
                {
                    s.renderScale = (scaleIdx == 2) ? 4 : (scaleIdx == 1) ? 2 : 1;
                }
            });

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Export", ImVec2(120, 0)))
        {
            s.exportType = EXPORT_FRAME_SEQUENCE;
            app.requestExport();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void drawExportVideoDialog(AppState& state, GuiApplication& app)
{
    if (ImGui::BeginPopupModal("Export Video / GIF", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ExportSettings& s = state.exportSettings;
        syncViewportDimensions(s, state);

        static std::vector<std::string> presets;
        static std::string currentPresetName = "Select Preset...";
        if (ImGui::IsWindowAppearing())
        {
            presets = ProjectSerializer::listPresets();
            currentPresetName = "Select Preset...";
        }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Preset:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("##PresetCombo", currentPresetName.c_str()))
        {
            for (const auto& p : presets)
            {
                if (ImGui::Selectable(p.c_str(), currentPresetName == p))
                {
                    if (ProjectSerializer::loadPreset(s, p))
                    {
                        currentPresetName = p;
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Preset"))
        {
            ImGui::OpenPopup("Save Preset As");
        }

        if (ImGui::BeginPopupModal("Save Preset As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char presetNameBuf[128] = "";
            ImGui::InputText("Preset Name", presetNameBuf, sizeof(presetNameBuf));
            ImGui::Spacing();
            if (ImGui::Button("Save", ImVec2(100, 0)))
            {
                if (presetNameBuf[0] != '\0')
                {
                    if (ProjectSerializer::savePreset(s, presetNameBuf))
                    {
                        presets = ProjectSerializer::listPresets(); // refresh
                        currentPresetName = presetNameBuf;
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::Text("Base Settings");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTable("VideoParamsTable", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

            drawPropertyRow("Width", [&]() { ImGui::InputInt("##Width", &s.width); });
            drawPropertyRow("Height", [&]() { ImGui::InputInt("##Height", &s.height); });
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Export Single File (Composite)", &s.exportSingle);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Output Folder");
            ImGui::TableNextColumn();
            char folderBuf[1024];
            std::snprintf(folderBuf, sizeof(folderBuf), "%s", utf8Path(s.outputFolder).c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 45.0f);
            if (ImGui::InputText("##outfolder", folderBuf, sizeof(folderBuf)))
            {
                s.outputFolder = pathFromUtf8(folderBuf);
            }
            ImGui::SameLine();
            if (ImGui::Button("...##folder"))
            {
                auto picked = NativeDialogs::pickFolder();
                if (picked)
                {
                    s.outputFolder = *picked;
                }
            }

            drawPropertyRow("Background Color", [&]() { ImGui::ColorEdit4("##BgColor", s.bgColor); });
            drawPropertyRow("Margin", [&]() { ImGui::InputInt("##Margin", &s.margin); });

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Auto Resolution", &s.autoResolution);

            if (s.autoResolution)
            {
                drawPropertyRow("Max Resolution", [&]() { ImGui::InputInt("##MaxRes", &s.maxResolution); });
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Separator();

            drawPropertyRow("Duration (s)", [&]() {
                ImGui::InputFloat("##Duration", &s.duration, 0.1f, 1.0f, "%.2f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("-1 = export exactly 1 full animation cycle");
            });
            drawPropertyRow("FPS", [&]() { ImGui::InputInt("##FPS", &s.fps); });
            drawPropertyRow("Export Speed", [&]() { ImGui::SliderFloat("##Speed", &s.exportSpeed, 0.01f, 10.0f, "%.2fx"); });

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Keep Last Frame", &s.keepLastFrame);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Separator();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Video Format");
            ImGui::TableNextColumn();
            const char* videoFormats[] = { "GIF", "WEBP", "APNG", "MP4", "WEBM (VP9 Alpha)", "MKV", "MOV" };
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::Combo("##VideoFormat", &s.videoFormat, videoFormats, IM_ARRAYSIZE(videoFormats));

            if (s.videoFormat == 0 || s.videoFormat == 1 || s.videoFormat == 2)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::Checkbox("Looping Playback", &s.loopPlay);
                
                drawPropertyRow("Quality", [&]() { ImGui::SliderInt("##Quality", &s.qualityParameter, 1, 100); });
            }
            else
            {
                drawPropertyRow("CRF Quality (Lower = Better)", [&]() { ImGui::SliderInt("##CRF", &s.crfParameter, 0, 51); });
            }

            // GIF-specific dithering options
            if (s.videoFormat == 0)
            {
                drawPropertyRow("GIF Dither Mode", [&]() {
                    const char* ditherModes[] = { "None", "Bayer", "Floyd-Steinberg", "Sierra" };
                    ImGui::Combo("##GifDither", &s.gifDitherMode, ditherModes, IM_ARRAYSIZE(ditherModes));
                });
                drawPropertyRow("GIF Max Colors", [&]() {
                    ImGui::SliderInt("##GifColors", &s.gifMaxColors, 64, 256);
                });
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Checkbox("Lossless Compression", &s.losslessCompression);

            drawPropertyRow("Render Scale", [&]() {
                const char* scales[] = { "1x", "2x", "4x" };
                int scaleIdx = (s.renderScale == 4) ? 2 : (s.renderScale == 2) ? 1 : 0;
                if (ImGui::Combo("##RenderScale", &scaleIdx, scales, IM_ARRAYSIZE(scales)))
                {
                    s.renderScale = (scaleIdx == 2) ? 4 : (scaleIdx == 1) ? 2 : 1;
                }
            });

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Export", ImVec2(120, 0)))
        {
            s.exportType = EXPORT_VIDEO;
            app.requestExport();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace ExportDialogs
} // namespace l2dgui
