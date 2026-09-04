Name:           ebasic
Version:        1.1.0
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
%{_datadir}/ebasic/
%{_mandir}/man1/ebc.1*
%{_mandir}/man1/ebpm.1*
%{_mandir}/man1/docgen.1*

%changelog
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
