#ifndef EGALITO_APP_H
#define EGALITO_APP_H

#include "conductor/setup.h"

class AppOptions {
private:
    bool debugMessages = false;
public:
    bool getDebugMessages() const { return debugMessages; }

    void setDebugMessages(bool d) { debugMessages = d; }
};

class App {
private:
    AppOptions options;
    ConductorSetup setup;
public:
    void parse(const char *filename);
    void extract();
    AppOptions &getOptions() { return options; }
};

#endif
