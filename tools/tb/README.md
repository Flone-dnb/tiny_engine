# tb

Tiny single file build system for C89/C90 code that works on Windows (modern and old like Windows 98) and Linux.

# Usage

`tb [-v] [CC <name>] [<target name>] [<mode>] [<predefined_variables>]`

- looks for a `build.tb` file in the current directory and scans the file and all included `.tb` files to collect and build all targets
- example: `tb editor release`
- if no arguments (just `tb`) compiles `tb all debug`
- optionally you can specify a compiler: `tb CC gcc editor debug`
- you can also specify `-v` for verbose output of the compilation commands

Here's a list of features that a `build.tb` file has:

- comments start with #
- built-in variables:
    - `SYSTEM_64BIT` defined only if detected a 64-bit environment
    - `SYSTEM_32BIT` defined only if detected a 32-bit environment
    - `OS_WINDOWS` defined only if running on Windows OS
    - `OS_LINUX` defined only if running on Linux
    - `BUILD_DEBUG` defined only if building in debug mode
    - `BUILD_RELEASE` defined only if building in release mode
    - `COMPILER_MSVC` defined only if compiling with MSVC
    - `COMPILER_GCC` defined only if compiling with GCC
    - `COMPILER_CLANG` defined only if compiling with Clang
- branching:
```
if <variable>
    # goes here if <variable> is defined
else
    ...
endif
```
- built-it functions (note that all paths the you specify in the functions are considered relatively to the current `build.tb` file):
    - `target(<name>)` creates a new target (project) or changes the current target if `target()` was specified previously
    - `set_target_type(exe/lib/dll)` changes the type of the current target: exe (default, executable), lib (static library), dll (dynamic/shared library)
    - `add_macro(<name>)` or `add_macro(<name>=<value>)` to create a predefined macro to be defined in the code of the current target
    - `add_private_includes(<relative_paths>)` and `add_public_includes(<relative_paths>)` to add an include path for the current target, you can also specify a list of items separated by a space or a new line, public includes are exposed to targets that link to the current target
    - `add_sources(<relative_paths>)` to add a .c/.h file to the current target, you can also specify a list of items separated by a space or a new line, it's important to also add header files (if you have any) so that they will be tracked for changes (important for recompilation)
    - `add_linker_flags(<flags>)` to add a single linker flag or a list of flags (separated by a space) for the current compiler
    - `info("<text>")` to show an informational message
    - `warn("<text>")` to show a warning message
    - `error("<text>")` to show an error and stop the build
    - `add_subproject(<relative path>)` specify a relative directory path that contains another `build.tb` file to include targets from that file to also build
    - `link_target(<target name>)` to create a dependency from the current target to the specified target, in this case the other target is expected to be a LIB/DLL target, all public includes and macros of the other target are added to the current target
    - `unlink_target(<target name>)` undoes `link_target`
    - `create_function(<name>)` creates a new function, all code before `end_function()` will be "copy-pasted" to the place where the function is called, the function will be visible to all included subprojects
    - `create_symlink(<path> <symlink location>)` creates a symlink that references the specified path
    - `duplicate_target(<existing target name> <new target name>)` clones all current properties of an already existing target and creates a new target with a different name, the new target is made current after this call
    - `dump_state()` prints the current state of the build configuration (might be useful for debugging)
- create variables with `<name> = <value>`

The build parameters (like sources, macros, etc) are cached and in case of a rebuild the system will only recompile affected object files.

# Build

- compiler that supports C89/C90
- make or nmake using `make -f make.mk` or `nmake -f nmake.mk` (or just compile it somehow else it's just a single source file)
