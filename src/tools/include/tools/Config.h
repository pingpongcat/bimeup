#pragma once

#include <string>
#include <unordered_map>

namespace bimeup::tools {

class Config {
public:
    bool Load(const std::string& path);
    bool LoadFromString(const std::string& content);

    std::string GetString(const std::string& key, const std::string& defaultVal = "") const;
    int GetInt(const std::string& key, int defaultVal = 0) const;
    float GetFloat(const std::string& key, float defaultVal = 0.0f) const;
    bool GetBool(const std::string& key, bool defaultVal = false) const;

private:
    // Keys stored as "section.key" or just "key" for global section
    std::unordered_map<std::string, std::string> m_values;

    static std::string Trim(const std::string& str);
};

} // namespace bimeup::tools
