Name:           ebasic
Version:        1.8.0
Release:        1%{?dist}
Summary:        BASIC-to-C++ transpiler with package manager and doc generator
Group:          Development/Tools
Packager:       eBasic contributors <noreply@example.com>

License:        MIT
URL:            https://github.com/yann64/ebasic
Source0:        https://github.com/yann64/ebasic/archive/refs/tags/v%{version}/ebasic-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  make
# ebc/ebpm invoke a real backend C++ compiler as a subprocess at runtime,
# not just at build time - declared explicitly rather than assumed.
Requires:       gcc-c++

%description
eBasic is an extended dialect of the BASIC programming language,
transpiled to C++ and compiled with a real backend compiler
(g++/clang++). Same syntax as FreeBASIC, reimplemented from scratch,
with TYPE-based OOP, C/C++ interop, namespaces, and a Cargo-style
package manager.

This package provides three tools:
 - ebc: the compiler driver
 - ebpm: the package manager
 - docgen: doc-comment ('''-marked) to Markdown/HTML generator

%prep
%autosetup -n ebasic-%{version}

%build
# Plain cmake invocations rather than the Fedora %cmake/%cmake_build
# convenience macros - those come from a cmake-rpm-macros-style package
# that isn't universally present on every RPM-based distro (confirmed:
# not available at all in this environment's own RPM toolchain), so
# this is both more portable and directly locally-verifiable.
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -- %{?_smp_mflags}

%install
cmake --install build --prefix %{buildroot}%{_prefix}
mkdir -p %{buildroot}%{_mandir}/man1
install -m 644 debian/man/*.1 %{buildroot}%{_mandir}/man1/

%check
ctest --test-dir build --output-on-failure

%files
%license LICENSE
%doc README.md
%{_bindir}/ebc
%{_bindir}/ebpm
%{_bindir}/docgen
%{_bindir}/ebasic_lsp
%{_datadir}/ebasic/
%{_mandir}/man1/ebc.1*
%{_mandir}/man1/ebpm.1*
%{_mandir}/man1/docgen.1*

%changelog
* Sun Sep 06 2026 eBasic contributors <noreply@example.com> - 1.8.0-1
- v1.8.0: calling through a PROPERTY of function-pointer type
  (obj.SomeProp(1, 2), via a plain receiver or This.) - calls the
  getter, then calls its result. Completes the "calling through a
  stored function pointer" family of features (DIM'd variable,
  parameter, TYPE field, and now PROPERTY).

* Sun Sep 06 2026 eBasic contributors <noreply@example.com> - 1.7.0-1
- v1.7.0: calling through a TYPE field of function-pointer type
  (obj.cb(1, 2), via a plain receiver or This.), completing the "calling
  through a stored function pointer" feature from 1.6.0. A function-
  pointer-typed PROPERTY is explicitly rejected with a clear diagnostic
  (a different, unimplemented codegen shape), not silently mishandled.

* Sat Sep 05 2026 eBasic contributors <noreply@example.com> - 1.6.0-1
- v1.6.0: calling through a stored function pointer (cb(1, 2), including
  through a function-pointer parameter inside a higher-order FUNCTION),
  and Stdcall on a plain eBasic-defined SUB/FUNCTION so @ProcName can
  target real Win32 Stdcall callback APIs (EnumWindows, SetTimer) on
  32-bit x86. Verified live against real cl.exe as well as g++.

* Sat Sep 05 2026 eBasic contributors <noreply@example.com> - 1.5.0-1
- v1.5.0: typed function-pointer parameters in EXTERN/DECLARE (@ProcName
  can now produce a real, structurally-checked function-pointer type
  instead of always degrading to ANY PTR), and a real MSVC precompiled-
  header rule (cl.exe now gets the same PCH speedup g++ already had,
  verified live against real cl.exe).

* Sat Sep 05 2026 eBasic contributors <noreply@example.com> - 1.4.0-1
- v1.4.0: WinUI3 shim example (examples/winui3_shim/) - a hand-written
  C++/WinRT Windows App SDK application driven by a real eBasic program
  via ShellExecuteA (Stdcall/EXTERN), demonstrating the recommended
  pattern from the MSVC/WinUI3 feasibility study's Phase 3. No compiler
  behavior changes in this release.

* Sat Sep 05 2026 eBasic contributors <noreply@example.com> - 1.3.0-1
- v1.3.0: Stdcall calling convention on EXTERN/DECLARE - Win32 APIs
  (User32/GDI/...) are always extern "C" __stdcall; Cdecl was previously
  the only calling convention Declare supported. Also folds in a fix
  landed since 1.2.0: EBASIC_LIBRARY_PATH (ebpm's own external-library
  search-path env var) now uses ';' instead of ':' as its delimiter,
  fixing a real Windows bug where ':' collided with an absolute path's
  own drive-letter colon.

* Sat Sep 05 2026 eBasic contributors <noreply@example.com> - 1.2.0-1
- v1.2.0: MSVC backend support for ebc (new windows-msvc CMake preset and
  CI job) - ebc now detects cl/clang-cl on -cxx/CXX and switches to
  MSVC-style command-line flags, alongside the existing g++/clang++ path.

* Fri Sep 04 2026 eBasic contributors <noreply@example.com> - 1.1.0-2
- Packaging fix: %files was missing %{_bindir}/ebasic_lsp (the LSP
  binary, shipped since M7 but never added here) - rpmbuild's stricter
  "installed but unpackaged file" check caught it building this very
  release; the .deb never needed a %files-equivalent list, so this went
  unnoticed there. No source change - Release bump only.

* Fri Sep 04 2026 eBasic contributors <noreply@example.com> - 1.1.0-1
- v1.1.0: M9 - FreeBASIC-parity preprocessor directives (#elseif/#if
  expressions/#undef/function-like and variadic #define/#macro-
  #endmacro/stringize+concatenate/#print/#error/#assert/__LINE__/
  __FILE__/__DATE__/__TIME__).

* Tue Jul 28 2026 eBasic contributors <noreply@example.com> - 1.0.0-1
- First tagged release (v1.0.0): all planned milestones complete, plus
  OS-conditional dependencies and Linux packaging.

* Tue Jul 28 2026 eBasic contributors <noreply@example.com> - 0.1.0-1
- Initial packaging.
