#ifndef ATG_ENGINE_SIM_ENGINE_SCRIPT_H
#define ATG_ENGINE_SIM_ENGINE_SCRIPT_H

#include <regex>
#include <string>

// The statement that runs an engine file from assets/engines when it's
// imported by another script. Newer engine scripts define main(); older ones
// only define an engine node that main.mr passed to set_engine(). Empty if
// the script defines no public node.
inline std::string engineScriptCall(const std::string &script) {
    if (std::regex_search(script, std::regex(R"(public\s+node\s+main\b)"))) {
        return "main()";
    }

    std::string lastNode;
    const std::regex node(R"(public\s+node\s+(\w+))");
    for (auto it = std::sregex_iterator(script.begin(), script.end(), node);
        it != std::sregex_iterator(); ++it)
    {
        lastNode = (*it)[1];
    }

    return lastNode.empty() ? std::string() : "set_engine(" + lastNode + "())";
}

#endif /* ATG_ENGINE_SIM_ENGINE_SCRIPT_H */
