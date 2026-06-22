#pragma once

#include "app/AppState.hpp"

namespace l2dgui
{
class GuiApplication;

namespace ExportDialogs
{

void drawExportFrameDialog(AppState& state, GuiApplication& app);
void drawExportSequenceDialog(AppState& state, GuiApplication& app);
void drawExportVideoDialog(AppState& state, GuiApplication& app);

} // namespace ExportDialogs
} // namespace l2dgui
