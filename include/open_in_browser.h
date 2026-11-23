#ifndef ATG_ENGINE_SIM_OPEN_IN_BROWSER_H
#define ATG_ENGINE_SIM_OPEN_IN_BROWSER_H

#include <string>

// Opens a local file (UTF-8 path) with the program the system uses for it,
// such as a web page in the default browser. Doesn't wait for it.
void openInBrowser(const std::string &path);

#endif /* ATG_ENGINE_SIM_OPEN_IN_BROWSER_H */
