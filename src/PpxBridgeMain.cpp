#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
const std::regex kName("^[A-Za-z][A-Za-z0-9_-]{0,63}$");

fs::path manifest() {
    const fs::path path = fs::current_path() / "Punpun.toml";
    if (!fs::is_regular_file(path))
        throw std::runtime_error("current directory has no Punpun.toml");
    return path;
}

std::vector<std::string> readLines(const fs::path &path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read Punpun.toml");

    std::vector<std::string> result;
    std::string line;
    while (std::getline(input, line)) result.push_back(line);
    return result;
}

void writeLines(const fs::path &path, const std::vector<std::string> &lines) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write Punpun.toml");
    for (const auto &line : lines) output << line << '\n';
}

std::pair<std::size_t, std::size_t> dependencyBounds(std::vector<std::string> &lines) {
    std::size_t start = std::string::npos;
    std::size_t end = lines.size();

    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::string trimmed = lines[i];
        const auto first = trimmed.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        trimmed = trimmed.substr(first);
        if (trimmed.empty() || trimmed.front() != '[') continue;

        const auto close = trimmed.find(']');
        if (close == std::string::npos) continue;
        const std::string section = trimmed.substr(1, close - 1);
        if (section == "dependencies") {
            start = i;
            continue;
        }
        if (start != std::string::npos && i > start) {
            end = i;
            break;
        }
    }

    if (start == std::string::npos) {
        if (!lines.empty() && !lines.back().empty()) lines.push_back("");
        lines.push_back("[dependencies]");
        start = lines.size() - 1;
        end = lines.size();
    }
    return {start, end};
}

std::string quotedPath(fs::path path) {
    path = fs::absolute(path).lexically_normal();
    const std::string value = path.generic_string();
    std::string result = "\"";
    for (const char c : value) {
        if (c == '\\' || c == '\"') result += '\\';
        result += c;
    }
    result += '\"';
    return result;
}

int addDependency(const std::string &name, const std::string &rawPath) {
    if (!std::regex_match(name, kName))
        throw std::runtime_error("invalid package name");

    const fs::path dependency = fs::absolute(rawPath);
    if (!fs::is_directory(dependency))
        throw std::runtime_error("dependency path does not exist: " + dependency.string());

    const fs::path path = manifest();
    auto lines = readLines(path);
    auto [start, end] = dependencyBounds(lines);
    const std::regex target("^\\s*" + name + "\\s*=");
    const std::string replacement = name + " = { path = " + quotedPath(dependency) + " }";

    bool found = false;
    for (std::size_t i = start + 1; i < end; ++i) {
        if (!std::regex_search(lines[i], target)) continue;
        lines[i] = replacement;
        found = true;
        break;
    }
    if (!found)
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(end), replacement);

    writeLines(path, lines);
    std::cout << "PPX graph added " << name << " -> " << dependency.generic_string() << '\n';
    return 0;
}

int removeDependency(const std::string &name) {
    if (!std::regex_match(name, kName))
        throw std::runtime_error("invalid package name");

    const fs::path path = manifest();
    auto lines = readLines(path);
    const auto [start, end] = dependencyBounds(lines);
    const std::regex target("^\\s*" + name + "\\s*=");

    for (std::size_t i = start + 1; i < end; ++i) {
        if (!std::regex_search(lines[i], target)) continue;
        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(i));
        writeLines(path, lines);
        std::cout << "PPX graph removed " << name << '\n';
        return 0;
    }
    throw std::runtime_error("dependency is not present: " + name);
}

int printTree() {
    auto lines = readLines(manifest());
    const auto [start, end] = dependencyBounds(lines);
    std::cout << fs::current_path().filename().string() << '\n';

    bool any = false;
    for (std::size_t i = start + 1; i < end; ++i) {
        const auto separator = lines[i].find('=');
        if (separator == std::string::npos) continue;
        std::string name = lines[i].substr(0, separator);
        name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char c) {
            return std::isspace(c) != 0;
        }), name.end());
        if (name.empty()) continue;
        std::cout << "+- " << name << '\n';
        any = true;
    }
    if (!any) std::cout << "(no dependencies)\n";
    return 0;
}

int validatePaths() {
    auto lines = readLines(manifest());
    const auto [start, end] = dependencyBounds(lines);
    const std::regex pathPattern("path\\s*=\\s*\"([^\"]+)\"");
    std::smatch match;
    int count = 0;

    for (std::size_t i = start + 1; i < end; ++i) {
        if (!std::regex_search(lines[i], match, pathPattern)) continue;
        fs::path path = match[1].str();
        if (path.is_relative()) path = fs::current_path() / path;
        if (!fs::exists(path)) {
            std::cerr << "missing dependency path: " << path.string() << '\n';
            return 1;
        }
        ++count;
    }
    std::cout << "PPX graph validated: " << count << " path dependency(s)\n";
    return 0;
}
}

int main(int argc, char **argv) {
    try {
        if (argc < 2) throw std::runtime_error("expected add/remove/tree/update");
        const std::string command = argv[1];
        if (command == "add" && argc == 4) return addDependency(argv[2], argv[3]);
        if (command == "remove" && argc == 3) return removeDependency(argv[2]);
        if (command == "tree" && argc == 2) return printTree();
        if (command == "update" && argc == 2) return validatePaths();
        throw std::runtime_error("unsupported command");
    } catch (const std::exception &error) {
        std::cerr << "ppide-pp-bridge: " << error.what() << '\n';
        return 2;
    }
}
