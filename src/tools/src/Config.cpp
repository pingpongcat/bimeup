#include "tools/Config.h"

#include <fstream>
#include <sstream>

namespace bimeup::tools {

bool Config::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return LoadFromString(content);
}

bool Config::LoadFromString(const std::string& content) {
    if (content.empty()) {
        return false;
    }

    m_values.clear();

    std::istringstream stream(content);
    std::string line;
    std::string currentSection;

    while (std::getline(stream, line)) {
        line = Trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Section header
        if (line.front() == '[' && line.back() == ']') {
            currentSection = Trim(line.substr(1, line.size() - 2));
            continue;
        }

        // Key = value
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = Trim(line.substr(0, eqPos));
        std::string value = Trim(line.substr(eqPos + 1));

        std::string fullKey = currentSection.empty() ? key : currentSection + "." + key;
        m_values[fullKey] = value;
    }

    return !m_values.empty();
}

std::string Config::GetString(const std::string& key, const std::string& defaultVal) const {
    auto it = m_values.find(key);
    return (it != m_values.end()) ? it->second : defaultVal;
}

int Config::GetInt(const std::string& key, int defaultVal) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) {
        return defaultVal;
    }
    try {
        return std::stoi(it->second);
    } catch (...) {
        return defaultVal;
    }
}

float Config::GetFloat(const std::string& key, float defaultVal) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) {
        return defaultVal;
    }
    try {
        return std::stof(it->second);
    } catch (...) {
        return defaultVal;
    }
}

bool Config::GetBool(const std::string& key, bool defaultVal) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) {
        return defaultVal;
    }
    const std::string& val = it->second;
    if (val == "true" || val == "1" || val == "yes" || val == "on") {
        return true;
    }
    if (val == "false" || val == "0" || val == "no" || val == "off") {
        return false;
    }
    return defaultVal;
}

std::string Config::Trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace bimeup::tools
