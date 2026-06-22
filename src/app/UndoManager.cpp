#include "app/UndoManager.hpp"
#include "backends/ILive2DBackend.hpp"
#include "render/ModelScanner.hpp"
#include <algorithm>
#include <iostream>

namespace l2dgui
{

void UndoManager::setMaxDepth(int depth)
{
    _maxDepth = depth;
}

void UndoManager::pushState(const AppState& state, const std::string& description)
{
    // Clear redo history
    if (_currentIndex < (int)_history.size() - 1)
    {
        _history.erase(_history.begin() + _currentIndex + 1, _history.end());
    }

    _history.push_back({capture(state), description});
    _currentIndex = (int)_history.size() - 1;

    // Enforce max depth
    if ((int)_history.size() > _maxDepth)
    {
        _history.erase(_history.begin());
        _currentIndex = (int)_history.size() - 1;
    }
}

bool UndoManager::canUndo() const
{
    return _currentIndex > 0;
}

bool UndoManager::canRedo() const
{
    return _currentIndex < (int)_history.size() - 1;
}

void UndoManager::undo(AppState& state, ILive2DBackend& backend)
{
    if (!canUndo()) return;
    _currentIndex--;
    restore(state, backend, _history[_currentIndex].first);
}

void UndoManager::redo(AppState& state, ILive2DBackend& backend)
{
    if (!canRedo()) return;
    _currentIndex++;
    restore(state, backend, _history[_currentIndex].first);
}

std::string UndoManager::undoDescription() const
{
    if (!canUndo()) return "";
    return _history[_currentIndex].second;
}

std::string UndoManager::redoDescription() const
{
    if (!canRedo()) return "";
    return _history[_currentIndex + 1].second;
}

void UndoManager::clear()
{
    _history.clear();
    _currentIndex = -1;
}

StateSnapshot UndoManager::capture(const AppState& state) const
{
    return StateSnapshot{state.models, state.view, state.exportSettings, state.browserDir};
}

void UndoManager::restore(AppState& state, ILive2DBackend& backend, const StateSnapshot& snapshot)
{
    // 1. Unload models that are in state.models but not in snapshot.models
    for (const auto& m : state.models)
    {
        bool found = false;
        for (const auto& sm : snapshot.models)
        {
            if (sm.id == m.id) { found = true; break; }
        }
        if (!found)
        {
            backend.unloadModel(m.id);
        }
    }

    // 2. Load models that are in snapshot.models but not in state.models, and update existing ones
    std::vector<ModelEntry> restoredModels;
    for (const auto& sm : snapshot.models)
    {
        bool found = false;
        ModelEntry* currentModel = nullptr;
        for (auto& m : state.models)
        {
            if (m.id == sm.id)
            {
                found = true;
                currentModel = &m;
                break;
            }
        }

        if (!found)
        {
            // Newly loaded model
            ModelEntry entry = sm;
            
            // Query metadata first
            ModelScanner::fillMetadataFromModel3Json(entry);
            
            auto loadRes = backend.loadModel(entry);
            if (loadRes.ok)
            {
                // Play motion / expressions if set in sm
                if (sm.playlistActive)
                {
                    backend.startPlaylist(entry.id, sm.playlist, sm.loopMotion);
                }
                else if (sm.currentMotion >= 0 && sm.currentMotion < (int)entry.motions.size())
                {
                    backend.playMotion(entry.id, entry.motions[sm.currentMotion], sm.loopMotion);
                }
                if (sm.currentExpression >= 0 && sm.currentExpression < (int)entry.expressions.size())
                {
                    backend.setExpression(entry.id, entry.expressions[sm.currentExpression]);
                }
                
                restoredModels.push_back(std::move(entry));
            }
        }
        else
        {
            // Sync backend settings if changed
            if (currentModel->currentMotion != sm.currentMotion || currentModel->loopMotion != sm.loopMotion)
            {
                if (sm.currentMotion >= 0 && sm.currentMotion < (int)currentModel->motions.size())
                {
                    backend.playMotion(sm.id, currentModel->motions[sm.currentMotion], sm.loopMotion);
                }
                else
                {
                    backend.stopMotion(sm.id);
                }
            }

            if (currentModel->currentExpression != sm.currentExpression)
            {
                if (sm.currentExpression >= 0 && sm.currentExpression < (int)currentModel->expressions.size())
                {
                    backend.setExpression(sm.id, currentModel->expressions[sm.currentExpression]);
                }
            }

            if (currentModel->playlistActive != sm.playlistActive || currentModel->playlist.size() != sm.playlist.size() || currentModel->loopMotion != sm.loopMotion)
            {
                if (sm.playlistActive)
                {
                    backend.startPlaylist(sm.id, sm.playlist, sm.loopMotion);
                }
                else if (currentModel->playlistActive)
                {
                    backend.stopPlaylist(sm.id);
                }
            }

            // Copy fields from snapshot
            *currentModel = sm;
            restoredModels.push_back(*currentModel);
        }
    }

    state.models = std::move(restoredModels);
    state.view = snapshot.view;
    state.exportSettings = snapshot.exportSettings;
    state.browserDir = snapshot.browserDir;
}

} // namespace l2dgui
