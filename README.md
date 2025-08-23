# Qt Web Platform Plugin

This is an experimental QPA designed to allow Qt applications to be seen inside a web browser.

Combined with my upcoming Qt Devcontainer Feature, this will allow Qt GUI apps to be developed entirely in the web browser using any IDE that supports DevContainers, like GitHub CodeSpaces.

To use, compile and run a Qt application using the QPA environment variables:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    QT_QPA_PLATFORM=web QT_QPA_PLATFORM_PLUGIN_PATH=$PWD/build/plugins/platforms dolphin

**Everything belongs to Qt. I hope this will one day be accepted as an open-source contribution.**
