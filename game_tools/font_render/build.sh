#!/bin/bash

# /dev/null hides printed output
pushd ~/dev/obsession/game_tools/font_render > /dev/null

start_time=$(gdate +%s%N)
clang++ -std=c++14 -g ./font_render.cpp -o ./font_render -I./lib

popd > /dev/null
# Capture the end time in nanoseconds using gdate
end_time=$(gdate +%s%N)

# Calculate the compile time in milliseconds
compile_time=$(( (end_time - start_time) / 1000000 ))

# Print the compile time
echo "Compilation completed in ${compile_time} milliseconds."
