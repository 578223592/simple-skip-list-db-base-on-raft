# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.27

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /home/swx/.local/share/JetBrains/Toolbox/apps/clion-nova/bin/cmake/linux/x64/bin/cmake

# The command to remove a file.
RM = /home/swx/.local/share/JetBrains/Toolbox/apps/clion-nova/bin/cmake/linux/x64/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/swx/2023_project/simple-skip-list-db-base-on-raft

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug

# Include any dependencies generated for this target.
include example/CMakeFiles/callerMain.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include example/CMakeFiles/callerMain.dir/compiler_depend.make

# Include the progress variables for this target.
include example/CMakeFiles/callerMain.dir/progress.make

# Include the compile flags for this target's objects.
include example/CMakeFiles/callerMain.dir/flags.make

example/CMakeFiles/callerMain.dir/callerMain.cpp.o: example/CMakeFiles/callerMain.dir/flags.make
example/CMakeFiles/callerMain.dir/callerMain.cpp.o: /home/swx/2023_project/simple-skip-list-db-base-on-raft/example/callerMain.cpp
example/CMakeFiles/callerMain.dir/callerMain.cpp.o: example/CMakeFiles/callerMain.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object example/CMakeFiles/callerMain.dir/callerMain.cpp.o"
	cd /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/example && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT example/CMakeFiles/callerMain.dir/callerMain.cpp.o -MF CMakeFiles/callerMain.dir/callerMain.cpp.o.d -o CMakeFiles/callerMain.dir/callerMain.cpp.o -c /home/swx/2023_project/simple-skip-list-db-base-on-raft/example/callerMain.cpp

example/CMakeFiles/callerMain.dir/callerMain.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/callerMain.dir/callerMain.cpp.i"
	cd /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/example && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/swx/2023_project/simple-skip-list-db-base-on-raft/example/callerMain.cpp > CMakeFiles/callerMain.dir/callerMain.cpp.i

example/CMakeFiles/callerMain.dir/callerMain.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/callerMain.dir/callerMain.cpp.s"
	cd /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/example && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/swx/2023_project/simple-skip-list-db-base-on-raft/example/callerMain.cpp -o CMakeFiles/callerMain.dir/callerMain.cpp.s

# Object files for target callerMain
callerMain_OBJECTS = \
"CMakeFiles/callerMain.dir/callerMain.cpp.o"

# External object files for target callerMain
callerMain_EXTERNAL_OBJECTS =

/home/swx/2023_project/simple-skip-list-db-base-on-raft/out_bin/callerMain: example/CMakeFiles/callerMain.dir/callerMain.cpp.o
/home/swx/2023_project/simple-skip-list-db-base-on-raft/out_bin/callerMain: example/CMakeFiles/callerMain.dir/build.make
/home/swx/2023_project/simple-skip-list-db-base-on-raft/out_bin/callerMain: /home/swx/2023_project/simple-skip-list-db-base-on-raft/lib/libskip_list_on_raft.a
/home/swx/2023_project/simple-skip-list-db-base-on-raft/out_bin/callerMain: example/CMakeFiles/callerMain.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable /home/swx/2023_project/simple-skip-list-db-base-on-raft/out_bin/callerMain"
	cd /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/example && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/callerMain.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
example/CMakeFiles/callerMain.dir/build: /home/swx/2023_project/simple-skip-list-db-base-on-raft/out_bin/callerMain
.PHONY : example/CMakeFiles/callerMain.dir/build

example/CMakeFiles/callerMain.dir/clean:
	cd /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/example && $(CMAKE_COMMAND) -P CMakeFiles/callerMain.dir/cmake_clean.cmake
.PHONY : example/CMakeFiles/callerMain.dir/clean

example/CMakeFiles/callerMain.dir/depend:
	cd /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/swx/2023_project/simple-skip-list-db-base-on-raft /home/swx/2023_project/simple-skip-list-db-base-on-raft/example /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/example /home/swx/2023_project/simple-skip-list-db-base-on-raft/cmake-build-debug/example/CMakeFiles/callerMain.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : example/CMakeFiles/callerMain.dir/depend

