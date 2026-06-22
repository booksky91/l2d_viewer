#include "render/ModelScanner.hpp"
#include "util/PathUtil.hpp"
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace l2dgui
{

static std::string readAll(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool ModelScanner::isModel3Json(const fs::path& p)
{
    std::error_code ec;
    const auto name = utf8Path(p.filename());
    return fs::is_regular_file(p, ec) && !ec && name.size() >= 12 && name.find(".model3.json") != std::string::npos;
}

std::vector<fs::path> ModelScanner::findModels(const fs::path& fileOrDir, int maxDepth)
{
    std::vector<fs::path> out;
    if (fileOrDir.empty()) return out;

    std::error_code ec;
    if (isModel3Json(fileOrDir))
    {
        out.push_back(fs::absolute(fileOrDir, ec));
        return out;
    }
    if (!fs::is_directory(fileOrDir, ec)) return out;

    const auto root = fs::absolute(fileOrDir, ec);
    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec))
    {
        if (it.depth() > maxDepth)
        {
            it.disable_recursion_pending();
            continue;
        }
        if (isModel3Json(it->path()))
        {
            out.push_back(it->path());
        }
    }
    return out;
}

static std::string trimQuotes(std::string s)
{
    if (!s.empty() && s.front() == '"') s.erase(s.begin());
    if (!s.empty() && s.back() == '"') s.pop_back();
    return s;
}

std::vector<MotionItem> ModelScanner::parseMotions(const std::string& jsonText)
{
    std::vector<MotionItem> out;

    // Lightweight scanner for Cubism model3.json Motion groups.
    // It intentionally avoids adding a JSON dependency to the MVP.
    std::regex groupRe(R"regex("([A-Za-z0-9_\-\.]+)"\s*:\s*\[)regex");
    std::regex fileRe(R"regex("File"\s*:\s*"([^"]+\.motion3\.json)")regex", std::regex::icase);

    const auto motionsPos = jsonText.find("\"Motions\"");
    if (motionsPos == std::string::npos) return out;
    const auto start = jsonText.find('{', motionsPos);
    if (start == std::string::npos) return out;

    int brace = 0;
    size_t end = start;
    for (; end < jsonText.size(); ++end)
    {
        if (jsonText[end] == '{') ++brace;
        else if (jsonText[end] == '}')
        {
            --brace;
            if (brace == 0) { ++end; break; }
        }
    }
    const std::string block = jsonText.substr(start, end - start);

    auto groupBegin = std::sregex_iterator(block.begin(), block.end(), groupRe);
    auto groupEnd = std::sregex_iterator();
    std::vector<std::pair<std::string, size_t>> groups;
    for (auto it = groupBegin; it != groupEnd; ++it)
    {
        groups.emplace_back((*it)[1].str(), static_cast<size_t>((*it).position()));
    }

    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        const auto& [groupName, groupPos] = groups[gi];
        const size_t nextGroupPos = (gi + 1 < groups.size()) ? groups[gi + 1].second : block.size();
        const std::string groupBlock = block.substr(groupPos, nextGroupPos - groupPos);
        int index = 0;
        for (auto it = std::sregex_iterator(groupBlock.begin(), groupBlock.end(), fileRe); it != std::sregex_iterator(); ++it)
        {
            out.push_back(MotionItem{groupName, index++, (*it)[1].str()});
        }
    }
    return out;
}

std::vector<ExpressionItem> ModelScanner::parseExpressions(const std::string& jsonText)
{
    std::vector<ExpressionItem> out;
    std::regex exprRe(R"regex(\{[^\{\}]*"Name"\s*:\s*"([^"]+)"[^\{\}]*"File"\s*:\s*"([^"]+\.exp3\.json)"[^\{\}]*\})regex", std::regex::icase);
    for (auto it = std::sregex_iterator(jsonText.begin(), jsonText.end(), exprRe); it != std::sregex_iterator(); ++it)
    {
        out.push_back(ExpressionItem{(*it)[1].str(), (*it)[2].str()});
    }
    return out;
}

void ModelScanner::fillMetadataFromModel3Json(ModelEntry& entry)
{
    entry.baseDir = entry.model3Json.parent_path();
    entry.name = utf8Path(entry.model3Json.stem());
    const std::string suffix = ".model3";
    if (entry.name.size() > suffix.size() && entry.name.substr(entry.name.size() - suffix.size()) == suffix)
    {
        entry.name.erase(entry.name.size() - suffix.size());
    }

    const auto jsonText = readAll(entry.model3Json);
    entry.motions = parseMotions(jsonText);
    entry.expressions = parseExpressions(jsonText);
}

} // namespace l2dgui
