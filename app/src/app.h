#ifndef EGALITO_APP_H
#define EGALITO_APP_H

#include "conductor/interface.h"

class App {
private:
    bool verboseLogging = true;
    EgalitoInterface *egalito;
public:
    int run(int argc, char *argv[]);
    void printUsage(const char *program);
private:
    void parse(const std::string &filename);
    void transform();
    void generate(const std::string &output);
};

#endif
