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
    std::cout << "Initializing Egalito\n";

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
        std::cout << "Parsing file [" << filename << "]\n";
        egalito->parse(filename, options.getRecursive());

        // Add our injected code.
        std::cout << "Injecting code from our library\n";
        egalito->parse("libinject.so");
    }
    catch(const char *message) {
        std::cout << "Exception: " << message << std::endl;
    }
}

void App::transform() {
    auto program = egalito->getProgram();

    std::cout << "Adding shadow stack...\n";
    ShadowStackPass shadowStack;
    program->accept(&shadowStack);

    // example:
    std::cout << "Final parsing results:\n";
    for(auto module : CIter::children(program)) {
        std::cout << "    parsed Module " << module->getName() << std::endl;
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
    std::cout << "Egalito shadow stack app" << std::endl;
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
            app.parse(arg);

            app.transform();

            if(a+1 < argc) {
                app.generate(argv[++a]);
            }
        }
    }
    
    return 0;
}
