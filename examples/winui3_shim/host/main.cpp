#include "pch.h"
#include "App.xaml.h"
#include <string>
#include <cstdio>
#include <shellapi.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::smoke::implementation {
    // Defined here, read by MainWindow.xaml.cpp - the title/message this
    // process was launched with (argv[1]/argv[2]), and a click notifier
    // that echoes to stdout so a launching process (eBasic's own compiled
    // program, via a plain CreateProcess) can observe activity without
    // needing any in-process callback machinery at all.
    std::wstring g_title;
    std::wstring g_message;

    std::wstring InitialTitle() { return g_title; }
    std::wstring InitialMessage() { return g_message; }
    void NotifyClick(int clickCount) {
        wprintf(L"CLICKED %d\n", clickCount);
        fflush(stdout);
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    winrt::smoke::implementation::g_title = argc > 1 ? argv[1] : L"eBasic + WinUI3";
    winrt::smoke::implementation::g_message = argc > 2 ? argv[2] : L"Hello from eBasic";
    if (argv) LocalFree(argv);

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    Application::Start([](auto&&) {
        make<winrt::smoke::implementation::App>();
    });
    return 0;
}
