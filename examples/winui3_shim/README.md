# WinUI3 shim: moved to eb-winui

This example was the original prototype proving eBasic could drive a real,
live WinUI3 (Windows App SDK / Fluent Design) window - it has grown into a
real package, [eb-winui](https://github.com/yann64/eb-winui), and its
source has moved there.

The short version of what it found: an in-process approach (compiling the
WinUI3 code as a DLL, loaded directly by eBasic's own `.exe`) crashed with
a `STATUS_STOWED_EXCEPTION` from inside the system `Microsoft.UI.Xaml.dll`
- the Windows App SDK's `MddBootstrapInitialize` bootstrap doesn't tolerate
being loaded as a plugin into a foreign host process, only a process built
from the ground up as a Windows App SDK app. eb-winui crosses that boundary
as a **process** boundary instead: a real, independent, self-contained
WinUI3 host application, launched by eBasic via `CreateProcessA` with its
stdin/stdout redirected through anonymous pipes - a live, bidirectional
command/acknowledgement channel to a real native window, not just a
one-shot launch.

See eb-winui's own README for the full reasoning, the host process's
protocol, and a working `hello_winui` example.
