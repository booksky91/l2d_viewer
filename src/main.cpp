#include "app/GuiApplication.hpp"
#include "util/CrashDiagnostics.hpp"
#include "util/StreamRedirector.hpp"
#include <exception>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

int main(int argc, char** argv)
{
#if defined(_WIN32)
    timeBeginPeriod(1);
#endif

    l2dgui::installCrashDiagnostics();
    l2dgui::setCrashStage("main: starting application");

    // Redirect stdout and stderr to the custom ImGui Console Log panel
    l2dgui::StreamToLogRedirector redirectCout(std::cout, l2dgui::LOG_INFO);
    l2dgui::StreamToLogRedirector redirectCerr(std::cerr, l2dgui::LOG_ERROR);

    int result = 0;
    try
    {
        l2dgui::GuiApplication app;
        l2dgui::setCrashStage("main: initialize");
        if (!app.initialize(argc, argv))
        {
#if defined(_WIN32)
            timeEndPeriod(1);
#endif
            return 1;
        }
        l2dgui::setCrashStage("main: run loop");
        result = app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal: " << e.what() << "\n";
        result = 2;
    }

#if defined(_WIN32)
    timeEndPeriod(1);
#endif
    return result;
}
