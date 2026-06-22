#pragma once

#include "app/AppState.hpp"
#include <filesystem>
#include <vector>

namespace l2dgui
{

class ModelScanner
{
public:
    static bool isModel3Json(const std::filesystem::path& p);
    static std::vector<std::filesystem::path> findModels(const std::filesystem::path& fileOrDir, int maxDepth = 3);
    static void fillMetadataFromModel3Json(ModelEntry& entry);

private:
    static std::vector<MotionItem> parseMotions(const std::string& jsonText);
    static std::vector<ExpressionItem> parseExpressions(const std::string& jsonText);
};

} // namespace l2dgui
