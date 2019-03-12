#include <iostream>
#include <functional>
#include <cstring>  // for std::strcmp
#include "app.h"

#include "conductor/conductor.h"
#include "chunk/dump.h"
#include "pass/chunkpass.h"
#include "log/registry.h"
#include "log/temp.h"

void App::parse(const char *filename) {
    std::cout << "file [" << filename << "]\n";

    if(!options.getDebugMessages()) {
        // Note: once we disable messages, we never re-enable them.
        // Right now the old settings aren't saved so it's not easy to do.
        GroupRegistry::getInstance()->muteAllSettings();
    }

    try {
        if(ElfMap::isElf(filename)) {
            const bool recursive = true;
            const bool includeEgalitoLib = false;
            setup.parseElfFiles(filename, recursive, includeEgalitoLib);
        }
        else {
            setup.parseEgalitoArchive(filename);
        }
    }
    catch(const char *message) {
        std::cout << "Exception: " << message << std::endl;
    }
}

void printUsage(const char *program) {
    std::cout << "Egalito app" << std::endl;
    std::cout << "Usage: " << program << " [-vq]" << std::endl;
    std::cout << "\t-v: be more verbose" << std::endl;
    std::cout << "\t-q: be less verbose" << std::endl;
}

int main(int argc, char *argv[]) {
    if(argc <= 1) {
        printUsage(argv[0] ? argv[0] : "etapp");
        return 0;
    }

    if(!SettingsParser().parseEnvVar("EGALITO_DEBUG")) {
        printUsage(argv[0]);
        return 1;
    }

    struct {
        const char *str;
        std::function<void (AppOptions &)> action;
    } actions[] = {
        // should we show debugging log messages?
        {"-v", [] (AppOptions &options) {
            options.setDebugMessages(true);
        }},
        {"-q", [] (AppOptions &options) {
            options.setDebugMessages(false);
        }},
    };

    App app;
    for(int a = 1; a < argc; a ++) {
        const char *arg = argv[a];
        if(arg[0] == '-') {
            bool found = false;
            for(auto action : actions) {
                if(std::strcmp(arg, action.str) == 0) {
                    action.action(app.getOptions());
                    found = true;
                    break;
                }
            }
            if(!found) {
                std::cout << "Warning: unrecognized option \"" << arg << "\"\n";
            }
        }
        else {
            app.parse(arg);
        }
    }
    
    return 0;
}
