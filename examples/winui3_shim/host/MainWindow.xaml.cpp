#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include <string>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::smoke::implementation
{
    // Defined in main.cpp - the title/message this process was launched
    // with, and a click notifier that echoes the running count to stdout.
    std::wstring InitialTitle();
    std::wstring InitialMessage();
    void NotifyClick(int clickCount);

    static int s_clickCount = 0;

    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(InitialTitle());
        myText().Text(InitialMessage());
    }

    void MainWindow::myButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        NotifyClick(++s_clickCount);
        myButton().Content(box_value(L"Clicked " + winrt::to_hstring(s_clickCount) + L"x"));
    }
}
