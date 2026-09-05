# WinUI3 shim: driving a real Windows App SDK window from eBasic

This example proves the pattern from `docs/architecture/roadmap.md`'s
MSVC-backend feasibility study: eBasic can drive a real, live WinUI3
(Windows App SDK / Fluent Design) window, even though `.bas` source itself
cannot author XAML or WinRT/COM code directly (see "Why not compiled
in-process" below for the full reasoning).

Two pieces:

- **`host/`** - a real, hand-written C++/WinRT WinUI3 application
  (`eb_winui3_host.vcxproj`), built with plain command-line `msbuild`, not
  the Visual Studio IDE and not the C++/WinRT VS extension (which isn't
  installed on every machine, and which normally wires up the XAML markup
  compiler automatically - here that wiring is done by hand in the
  `.vcxproj` itself; see the comments in that file). It reads a window
  title and message from its own command-line arguments (`argv[1]`,
  `argv[2]`) and shows them in a `TextBlock`, with a `Button` that updates
  its own label on each click.
- **`hello_winui3.bas`** - a small eBasic program that launches
  `eb_winui3_host.exe` with a title and message, via `ShellExecuteA`
  (`Stdcall`/`EXTERN "C"`, `shell32.dll`).

## Why a separate process, not an in-process DLL

The first approach tried here was the more obviously "integrated" one:
compile the WinUI3 code as a DLL and have the eBasic-compiled `.exe` load
it directly (`EXTERN "C"` calling into exported functions like
`create_window`). That was built and it loaded - but the window itself
crashed on creation, with a `STATUS_STOWED_EXCEPTION` (`0xc000027b`) raised
from inside the *system*-installed `Microsoft.UI.Xaml.dll`.

The root cause is the Windows App SDK's deployment model, not a bug in the
shim code: `WindowsAppSDKSelfContained=true` bundles the WinUI3 runtime
files next to the build output, but the `MddBootstrapInitialize` bootstrap
API that wires a process up to the Windows App Runtime still resolves the
actual framework package from the system-wide MSIX registration under
`C:\Program Files\WindowsApps\Microsoft.WindowsAppRuntime...\`, regardless
of what's sitting next to the DLL. That's fine for a process that was
built from the ground up as a Windows App SDK app (like `eb_winui3_host`
itself) - it isn't set up to tolerate being loaded as a plugin into an
arbitrary foreign host process (like eBasic's plain-C++-runtime `.exe`)
that never initialized that context itself.

Rather than fight that boundary, this example crosses it as a **process**
boundary instead of a **linkage** boundary: `eb_winui3_host.exe` stays a
complete, independent, self-contained Windows App SDK application (exactly
the proven-working smoke-test configuration), and eBasic launches it with
`ShellExecuteA` the same way a shell script or a Start Menu shortcut
would. This sidesteps the incompatibility entirely, and is arguably a more
robust pattern anyway: the WinUI3 window's process lifetime, message loop,
and any future crash are fully isolated from the eBasic host program.

## Building `host/`

Requires Visual Studio (with the "Desktop development with C++" and
Windows App SDK components) and an "x64 Native Tools" environment, or
`vcvarsall.bat` sourced first:

```bash
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd examples\winui3_shim\host
msbuild eb_winui3_host.vcxproj -t:restore -p:Configuration=Debug -p:Platform=x64
msbuild eb_winui3_host.vcxproj -p:Configuration=Debug -p:Platform=x64
```

This produces `host\x64\Debug\eb_winui3_host.exe`. The `restore` step pulls
the `Microsoft.WindowsAppSDK` and `Microsoft.Windows.CppWinRT` NuGet
packages the project references.

## Running `hello_winui3.bas`

Run from the repository root, since the launched path in the script is
relative to it:

```bash
ebc examples\winui3_shim\hello_winui3.bas -o hello_winui3.exe
hello_winui3.exe
```

This prints `launched` and opens a WinUI3 window titled "Hello from
eBasic" showing the message text, with a working "Click me" button.
