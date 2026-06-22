#pragma once

#include "app/AppState.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <utility>

namespace l2dgui
{
class ILive2DBackend;

struct StateSnapshot
{
    std::vector<ModelEntry> models;
    ViewSettings view;
    ExportSettings exportSettings;
    std::filesystem::path browserDir;
};

class UndoManager
{
public:
    void setMaxDepth(int depth); // default 50
    void pushState(const AppState& state, const std::string& description = "");
    bool canUndo() const;
    bool canRedo() const;
    void undo(AppState& state, ILive2DBackend& backend);
    void redo(AppState& state, ILive2DBackend& backend);
    std::string undoDescription() const;
    std::string redoDescription() const;
    void clear();

private:
    std::vector<std::pair<StateSnapshot, std::string>> _history;
    int _currentIndex = -1;
    int _maxDepth = 50;

    StateSnapshot capture(const AppState& state) const;
    void restore(AppState& state, ILive2DBackend& backend, const StateSnapshot& snapshot);
};

} // namespace l2dgui
