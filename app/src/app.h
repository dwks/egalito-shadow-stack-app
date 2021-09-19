#ifndef EGALITO_APP_H
#define EGALITO_APP_H

#include "conductor/interface.h"

class AppOptions {
private:
    bool debugMessages;
    bool recursive;
public:
    AppOptions() : debugMessages(true), recursive(true) {}
    bool getDebugMessages() const { return debugMessages; }
    bool getRecursive() const { return recursive; }

    void setDebugMessages(bool d) { debugMessages = d; }
    void setRecursive(bool r) { recursive = r; }
};

class App {
private:
    AppOptions options;
    EgalitoInterface *egalito;
public:
    void parse(const std::string &filename);
    void transform();
    void generate(const std::string &output);
    AppOptions &getOptions() { return options; }
};

#endif
