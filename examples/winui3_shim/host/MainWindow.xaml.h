#pragma once
#include "MainWindow.g.h"

namespace winrt::smoke::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        void myButton_Click(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
    };
}

namespace winrt::smoke::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
