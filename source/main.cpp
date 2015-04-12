#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/ui.hpp>

#include <sstream>
#include <iomanip>
#include <stdio.h>

typedef enum {
    INSTALL,
    DELETE
} Mode;

int main(int argc, char **argv) {
    if(!platformInit()) {
        return 0;
    }

    bool ninjhax = platformIsNinjhax();

    std::vector<std::string> extensions;
    extensions.push_back("cia");

    MediaType destination = SD;
    Mode mode = INSTALL;
    bool exit = false;
    bool netInstall = false;
    u64 freeSpace = fsGetFreeSpace(destination);
    auto onLoop = [&]() {
        if(ninjhax && inputIsPressed(BUTTON_START)) {
            exit = true;
            return true;
        }

        bool breakLoop = false;

        if(inputIsPressed(BUTTON_L)) {
            if(destination == SD) {
                destination = NAND;
            } else {
                destination = SD;
            }

            freeSpace = fsGetFreeSpace(destination);
            if(mode == DELETE) {
                breakLoop = true;
            }
        }

        if(inputIsPressed(BUTTON_R)) {
            if(mode == INSTALL) {
                mode = DELETE;
            } else {
                mode = INSTALL;
            }

            breakLoop = true;
        }

        if(inputIsPressed(BUTTON_Y)) {
            netInstall = true;
            breakLoop = true;
        }

        std::stringstream stream;
        stream << "Free Space: " << freeSpace << " bytes (" << std::fixed << std::setprecision(2) << freeSpace / 1024.0f / 1024.0f << "MB)" << "\n";
        stream << "Destination: " << (destination == NAND ? "NAND" : "SD") << ", Mode: " << (mode == INSTALL ? "Install" : "Delete") << "\n";
        stream << "L - Switch Destination, R - Switch Mode" << "\n";
        stream << "Y - Receive an app over the network" << "\n";
        if(ninjhax) {
            stream << "START - Exit to launcher" << "\n";
        }

        std::string str = stream.str();
        screenDrawString(str, (screenGetWidth() - screenGetStrWidth(str)) / 2, screenGetHeight() - 4 - screenGetStrHeight(str), 255, 255, 255);

        return breakLoop;
    };

    auto onProgress = [&](int progress) {
        uiDisplayProgress(TOP_SCREEN, "Installing", "Press B to cancel.", true, progress);
        inputPoll();
        return !inputIsPressed(BUTTON_B);
    };

    while(platformIsRunning()) {
        std::string targetInstall;
        App targetDelete;
        if(mode == INSTALL) {
            uiSelectFile(&targetInstall, "sdmc:", extensions, [&](bool inRoot) {
                return onLoop();
            }, [&](std::string path, bool &updateList) {
                if(uiPrompt(TOP_SCREEN, "Install the selected title?", true)) {
                    AppResult ret = appInstallFile(destination, path, onProgress);
                    std::stringstream resultMsg;

                    resultMsg << "Install ";
                    if(ret == APP_SUCCESS) {
                        resultMsg << "succeeded!";
                    } else {
                        resultMsg << "failed!" << "\n";
                        resultMsg << appGetResultString(ret) << "\n";
                    }

                    uiPrompt(TOP_SCREEN, resultMsg.str(), false);

                    freeSpace = fsGetFreeSpace(destination);
                }

                return false;
            });
        } else if(mode == DELETE) {
            uiSelectApp(&targetDelete, destination, onLoop, [&](App app, bool &updateList) {
                if(uiPrompt(TOP_SCREEN, "Delete the selected title?", true)) {
                    updateList = true;
                    uiDisplayMessage(TOP_SCREEN, "Deleting title...");
                    AppResult ret = appDelete(app);

                    std::stringstream resultMsg;
                    resultMsg << "Delete ";
                    if(ret == APP_SUCCESS) {
                        resultMsg << "succeeded!";
                    } else {
                        resultMsg << "failed!" << "\n";
                        resultMsg << appGetResultString(ret) << "\n";
                    }

                    uiPrompt(TOP_SCREEN, resultMsg.str(), false);

                    freeSpace = fsGetFreeSpace(destination);
                }

                return false;
            });
        }

        if(netInstall && !exit) {
            netInstall = false;

            screenClearBuffers(BOTTOM_SCREEN, 0, 0, 0);

            RemoteFile file = uiAcceptRemoteFile(TOP_SCREEN);
            if(file.fd == NULL) {
                continue;
            }

            std::stringstream confirmStream;
            confirmStream << "Install the received application?" << "\n";
            confirmStream << "Size: " << file.fileSize << " bytes (" << std::fixed << std::setprecision(2) << file.fileSize / 1024.0f / 1024.0f << "MB)" << "\n";
            if(uiPrompt(TOP_SCREEN, confirmStream.str(), true)) {
                AppResult ret = appInstall(destination, file.fd, file.fileSize, onProgress);
                std::stringstream resultMsg;
                if(mode == INSTALL) {
                    resultMsg << "Install ";
                } else if(mode == DELETE) {
                    resultMsg << "Delete ";
                }

                if(ret == APP_SUCCESS) {
                    resultMsg << "succeeded!";
                } else {
                    resultMsg << "failed!" << "\n";
                    resultMsg << appGetResultString(ret) << "\n";
                }

                uiPrompt(TOP_SCREEN, resultMsg.str(), false);
            }

            fclose(file.fd);
            continue;
        }

        if(exit) {
            break;
        }
    }

    platformCleanup();
    return 0;
}
