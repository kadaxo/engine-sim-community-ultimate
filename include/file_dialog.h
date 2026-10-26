#ifndef ATG_ENGINE_SIM_FILE_DIALOG_H
#define ATG_ENGINE_SIM_FILE_DIALOG_H

#include <string>

// Shows the platform's "open file" dialog for engine-sim scripts (*.mr),
// starting in initialDirectory. Returns false if the user cancelled or no
// dialog is available. ownerWindow is the native window handle to make the
// dialog modal to (HWND on Windows), or nullptr.
bool openScriptFileDialog(
    const std::string &title,
    const std::string &initialDirectory,
    void *ownerWindow,
    std::string *path);

#endif /* ATG_ENGINE_SIM_FILE_DIALOG_H */
