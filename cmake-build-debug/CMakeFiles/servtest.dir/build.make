# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
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

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mnt/c/Users/Л/CLionProjects/Modbas-server

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/c/Users/Л/CLionProjects/Modbas-server/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/servtest.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/servtest.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/servtest.dir/flags.make

CMakeFiles/servtest.dir/main.cpp.o: CMakeFiles/servtest.dir/flags.make
CMakeFiles/servtest.dir/main.cpp.o: ../main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/c/Users/Л/CLionProjects/Modbas-server/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/servtest.dir/main.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/servtest.dir/main.cpp.o -c /mnt/c/Users/Л/CLionProjects/Modbas-server/main.cpp

CMakeFiles/servtest.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/servtest.dir/main.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /mnt/c/Users/Л/CLionProjects/Modbas-server/main.cpp > CMakeFiles/servtest.dir/main.cpp.i

CMakeFiles/servtest.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/servtest.dir/main.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /mnt/c/Users/Л/CLionProjects/Modbas-server/main.cpp -o CMakeFiles/servtest.dir/main.cpp.s

# Object files for target servtest
servtest_OBJECTS = \
"CMakeFiles/servtest.dir/main.cpp.o"

# External object files for target servtest
servtest_EXTERNAL_OBJECTS =

servtest: CMakeFiles/servtest.dir/main.cpp.o
servtest: CMakeFiles/servtest.dir/build.make
servtest: CMakeFiles/servtest.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/mnt/c/Users/Л/CLionProjects/Modbas-server/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable servtest"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/servtest.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/servtest.dir/build: servtest

.PHONY : CMakeFiles/servtest.dir/build

CMakeFiles/servtest.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/servtest.dir/cmake_clean.cmake
.PHONY : CMakeFiles/servtest.dir/clean

CMakeFiles/servtest.dir/depend:
	cd /mnt/c/Users/Л/CLionProjects/Modbas-server/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/c/Users/Л/CLionProjects/Modbas-server /mnt/c/Users/Л/CLionProjects/Modbas-server /mnt/c/Users/Л/CLionProjects/Modbas-server/cmake-build-debug /mnt/c/Users/Л/CLionProjects/Modbas-server/cmake-build-debug /mnt/c/Users/Л/CLionProjects/Modbas-server/cmake-build-debug/CMakeFiles/servtest.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/servtest.dir/depend

