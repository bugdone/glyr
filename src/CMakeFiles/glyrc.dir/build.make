# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canoncical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/spohst/Projects/gittry/glyr

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/spohst/Projects/gittry/glyr

# Include any dependencies generated for this target.
include src/CMakeFiles/glyrc.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/glyrc.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/glyrc.dir/flags.make

src/CMakeFiles/glyrc.dir/main.c.o: src/CMakeFiles/glyrc.dir/flags.make
src/CMakeFiles/glyrc.dir/main.c.o: src/main.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/spohst/Projects/gittry/glyr/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object src/CMakeFiles/glyrc.dir/main.c.o"
	cd /home/spohst/Projects/gittry/glyr/src && /usr/bin/gcc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/glyrc.dir/main.c.o   -c /home/spohst/Projects/gittry/glyr/src/main.c

src/CMakeFiles/glyrc.dir/main.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/glyrc.dir/main.c.i"
	cd /home/spohst/Projects/gittry/glyr/src && /usr/bin/gcc  $(C_DEFINES) $(C_FLAGS) -E /home/spohst/Projects/gittry/glyr/src/main.c > CMakeFiles/glyrc.dir/main.c.i

src/CMakeFiles/glyrc.dir/main.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/glyrc.dir/main.c.s"
	cd /home/spohst/Projects/gittry/glyr/src && /usr/bin/gcc  $(C_DEFINES) $(C_FLAGS) -S /home/spohst/Projects/gittry/glyr/src/main.c -o CMakeFiles/glyrc.dir/main.c.s

src/CMakeFiles/glyrc.dir/main.c.o.requires:
.PHONY : src/CMakeFiles/glyrc.dir/main.c.o.requires

src/CMakeFiles/glyrc.dir/main.c.o.provides: src/CMakeFiles/glyrc.dir/main.c.o.requires
	$(MAKE) -f src/CMakeFiles/glyrc.dir/build.make src/CMakeFiles/glyrc.dir/main.c.o.provides.build
.PHONY : src/CMakeFiles/glyrc.dir/main.c.o.provides

src/CMakeFiles/glyrc.dir/main.c.o.provides.build: src/CMakeFiles/glyrc.dir/main.c.o

# Object files for target glyrc
glyrc_OBJECTS = \
"CMakeFiles/glyrc.dir/main.c.o"

# External object files for target glyrc
glyrc_EXTERNAL_OBJECTS =

src/glyrc: src/CMakeFiles/glyrc.dir/main.c.o
src/glyrc: lib/libglyr.so
src/glyrc: src/CMakeFiles/glyrc.dir/build.make
src/glyrc: src/CMakeFiles/glyrc.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable glyrc"
	cd /home/spohst/Projects/gittry/glyr/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/glyrc.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/glyrc.dir/build: src/glyrc
.PHONY : src/CMakeFiles/glyrc.dir/build

src/CMakeFiles/glyrc.dir/requires: src/CMakeFiles/glyrc.dir/main.c.o.requires
.PHONY : src/CMakeFiles/glyrc.dir/requires

src/CMakeFiles/glyrc.dir/clean:
	cd /home/spohst/Projects/gittry/glyr/src && $(CMAKE_COMMAND) -P CMakeFiles/glyrc.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/glyrc.dir/clean

src/CMakeFiles/glyrc.dir/depend:
	cd /home/spohst/Projects/gittry/glyr && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/spohst/Projects/gittry/glyr /home/spohst/Projects/gittry/glyr/src /home/spohst/Projects/gittry/glyr /home/spohst/Projects/gittry/glyr/src /home/spohst/Projects/gittry/glyr/src/CMakeFiles/glyrc.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/glyrc.dir/depend

