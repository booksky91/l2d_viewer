#include "app/AppState.hpp"
#include <algorithm>

namespace l2dgui
{

std::string MotionItem::label() const
{
    if (!group.empty())
    {
        return group + "[" + std::to_string(index) + "]";
    }
    return file.empty() ? ("motion[" + std::to_string(index) + "]") : file;
}

uint64_t AppState::nextId()
{
    return _nextId++;
}

ModelEntry* AppState::find(uint64_t id)
{
    auto it = std::find_if(models.begin(), models.end(), [id](const ModelEntry& m) { return m.id == id; });
    return it == models.end() ? nullptr : &(*it);
}

const ModelEntry* AppState::find(uint64_t id) const
{
    auto it = std::find_if(models.begin(), models.end(), [id](const ModelEntry& m) { return m.id == id; });
    return it == models.end() ? nullptr : &(*it);
}

std::vector<ModelEntry*> AppState::selectedModels()
{
    std::vector<ModelEntry*> out;
    for (auto& m : models)
    {
        if (m.selected)
        {
            out.push_back(&m);
        }
    }
    return out;
}

void AppState::clearSelection()
{
    for (auto& m : models)
    {
        m.selected = false;
    }
}

void AppState::selectOnly(uint64_t id)
{
    for (auto& m : models)
    {
        m.selected = (m.id == id);
    }
}

} // namespace l2dgui
