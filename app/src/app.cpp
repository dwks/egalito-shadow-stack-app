#include <iostream>
#include <functional>
#include <cstring>  // for std::strcmp
#include "app.h"
#include "shadowstack.h"

#include "conductor/conductor.h"
#include "chunk/dump.h"
#include "pass/chunkpass.h"
#include "log/registry.h"
#include "log/temp.h"

void App::parse(const std::string &filename) {
    std::cout << "Parsing file [" << filename << "]\n";

    // Set logging levels according to quiet and EGALITO_DEBUG env var.
    egalito = new EgalitoInterface(/*verboseLogging=*/ options.getDebugMessages(),
        /*useLoggingEnvVar=*/ true);

    // Parsing ELF files can throw exceptions.
    try {
        egalito->initializeParsing();  // Creates Conductor and Program

        // Parse a filename; if second arg is true, parse shared libraries
        // recursively. This parse() can be called repeatedly to inject other
        // dependencies, and the recursive closure can be parsed with
        // parseRecursiveDependencies() at any later stage.
        egalito->parse(filename, options.getRecursive());
    }
    catch(const char *message) {
        std::cout << "Exception: " << message << std::endl;
    }
}

void App::processProgram() {
    // ... analyze or transform the program

    egalito->parse("libinject.so");
    auto program = egalito->getProgram();

    std::cout << "Adding shadow stack...\n";
    ShadowStackPass shadowStack(ShadowStackPass::MODE_CONST);
    program->accept(&shadowStack);

    // example:
    std::cout << "Final parsing results:\n";
    for(auto module : CIter::children(program)) {
        std::cout << "    parsed Module " << module->getName() << std::endl;
    }
    for(auto library : CIter::children(egalito->getLibraryList())) {
        std::cout << "    depends on Library " << library->getName()
            << " [" << library->getResolvedPath() << "]" << std::endl;
    }

    if(program->getEntryPoint()) {
        std::cout << "Dump of entry point:\n";
        egalito->dump(program->getEntryPoint());
    }
}

void App::generate(const std::string &output) {
    // Generate output, mirrorgen or uniongen. If only one argument is
    // given to generate(), automatically guess based on whether multiple
    // Modules are present.
    std::cout << "Performing code generation into [" << output << "]...\n";
    egalito->generate(output);
}

void printUsage(const char *program) {
    std::cout << "Egalito example app" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << program << " [-vq] input [output]" << std::endl;
    std::cout << "\t-v: be more verbose" << std::endl;
    std::cout << "\t-q: be less verbose" << std::endl;
    std::cout << "\t-r: parse recursively" << std::endl;
    std::cout << "\t-1: parse non-recursively" << std::endl;
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
        {"-v", [] (AppOptions &options) { options.setDebugMessages(true); }},
        {"-q", [] (AppOptions &options) { options.setDebugMessages(false); }},
        {"-r", [] (AppOptions &options) { options.setRecursive(true); }},
        {"-1", [] (AppOptions &options) { options.setRecursive(false); }},
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
            // 1. Parse input.
            app.parse(arg);

            // 2. Analyze or transform the program.
            app.processProgram();

            // 3. (optional) Generate a new output ELF.
            if(a+1 < argc) {
                app.generate(argv[++a]);
            }
        }
    }
    
    return 0;
}
