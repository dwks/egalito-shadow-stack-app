#include <iostream>
#include <functional>
#include <cstring>  // for std::strcmp
#include <cstdlib>  // for std::exit
#include "app.h"
#include "shadowstack.h"

#include "conductor/conductor.h"
#include "chunk/dump.h"
#include "pass/chunkpass.h"
#include "log/registry.h"
#include "log/temp.h"

void App::parse(const std::string &filename) {
    // Set logging levels according to quiet and EGALITO_DEBUG env var.
    egalito = new EgalitoInterface(/*verboseLogging=*/ this->verboseLogging,
        /*useLoggingEnvVar=*/ true);

    // Parsing ELF files can throw exceptions.
    try {
        egalito->initializeParsing();  // Creates Conductor and Program

        // Parse the given input file, end all shared library dependencies,
        // recursively.
        std::cout << "Parsing file [" << filename << "]\n";
        egalito->parse(filename, /*recursiveDependencies=*/ true);

        // Add our injected code.
        std::cout << "Injecting code from our library\n";
        egalito->parse("libinject.so");
    }
    catch(const char *message) {
        std::cout << "Exception: " << message << std::endl;
        std::exit(1);
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

void App::printUsage(const char *program) {
    std::cout << "Egalito shadow stack app" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << program << " [-vq] input [output]" << std::endl;
    std::cout << "\t-v: be more verbose" << std::endl;
    std::cout << "\t-q: be less verbose" << std::endl;
}

int App::run(int argc, char *argv[]) {
    if(argc <= 1) {
        printUsage(argv[0] ? argv[0] : "etapp");
        return 0;
    }

    for(int a = 1; a < argc; a ++) {
        const char *arg = argv[a];
        if(std::strcmp(arg, "-v") == 0) {
            verboseLogging = true;
        }
        else if(std::strcmp(arg, "-q") == 0) {
            verboseLogging = false;
        }
        else {
            parse(arg);

            transform();

            if(a+1 < argc) {
                generate(argv[++a]);
            }
        }
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    App app;
    return app.run(argc, argv);
}
