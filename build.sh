#!/bin/bash

FLAGS="-Wall -g -DDEBUG=1"
BUILD_DIR="./build_debug"

if [[ "$1" == "-release" || "$1" == "-mac" ]]; then
    echo "Compiling in release mode..."
    FLAGS="-O3 -arch arm64 -mmacosx-version-min=10.7"
    BUILD_DIR="./build_release"
fi

GAME_CLANG_CMD="clang++ -std=c++14 $FLAGS ./src/game_main.cpp -dynamiclib -o $BUILD_DIR/game_main.dylib -I./lib"

# Hot reloading causes the vscode lldb instance to lose debug symbols.
# reload with this "target symbols add build_debug/game_main.dylib.dSYM/Contents/Resources/DWARF/game_main.dylib"
if [[ "$1" == "-g" ]]; then
    echo "Only compiling game_main..."
    rm -rf $BUILD_DIR/game_main.dylib.dSYM
    rm -rf $BUILD_DIR/temp_game_main.dylib.dSYM
    start_time=$(gdate +%s%N)
    $GAME_CLANG_CMD
    # if the dSYM file name is different after reloading, lldb does not reload the symbols and connect it to the source map.
    cp -r $BUILD_DIR/game_main.dylib.dSYM $BUILD_DIR/temp_game_main.dylib.dSYM
    end_time=$(gdate +%s%N)
    compile_time=$(( (end_time - start_time) / 1000000 ))
    echo "Compilation completed in ${compile_time} milliseconds."
    exit 1
fi

mkdir -p $BUILD_DIR
mkdir -p $BUILD_DIR/resources

cp ./lib/mac_osx/*.dylib $BUILD_DIR

if [ ! -e ./game_tools/font_render/font_render ]; then
	./game_tools/font_render/build.sh
fi

if [ ! -e ./resources/assets/font_data -o ! -e ./resources/assets/font_texture.png ]; then
	./game_tools/font_render/font_render
fi

cp -r ./resources/* $BUILD_DIR/resources

# Capture the start time in nanoseconds using gdate
start_time=$(gdate +%s%N)
if [ ! -e ./lib/mac_osx/glad.o ]; then
    clang++ -arch arm64 -mmacosx-version-min=10.7 -x c ./lib/glad/src/glad.c -o ./lib/mac_osx/glad.o -c -I./lib/glad/include
fi

# if [ ! -e "$BUILD_DIR/mac_osx.o" ] || [ ./src/mac_osx.cpp -nt "$BUILD_DIR/mac_osx.o" ]; then
#     echo "Recompiling mac_osx.cpp..."
#     clang++ -std=c++14 -g -DDEBUG=1 -c ./src/mac_osx.cpp -o "$BUILD_DIR/mac_osx.o"
# fi

# Want to ensure that we have fresh debug, I found that on re-run the dSYMs were
# not being regenerated
rm -rf $BUILD_DIR/platform_main.dSYM
rm -rf $BUILD_DIR/game_main.dylib.dSYM

clang++ -std=c++14 $FLAGS ./src/platform_main.cpp -o $BUILD_DIR/platform_main -L$BUILD_DIR -lSDL3 -Wl,-rpath,@loader_path -I./lib/glad/include -I./lib ./lib/mac_osx/glad.o
$GAME_CLANG_CMD

# Capture the end time in nanoseconds using gdate
end_time=$(gdate +%s%N)

# Calculate the compile time in milliseconds
compile_time=$(( (end_time - start_time) / 1000000 ))

# Print the compile time
echo "Compilation completed in ${compile_time} milliseconds."

if [[ "$1" == "-mac" ]]; then
    APP_DIR="./waffle_zombies_arm64.app"
    echo "Building app file..."
    mkdir -p $APP_DIR/Contents/MacOS
    mkdir -p $APP_DIR/Contents/Resources
    cp $BUILD_DIR/platform_main $APP_DIR/Contents/MacOS
    cp -r $BUILD_DIR/*.dylib $APP_DIR/Contents/MacOS
    cp $BUILD_DIR/game_main.dylib $APP_DIR/Contents/Resources

    mkdir -p $APP_DIR/Contents/Resources/assets
    cp -r $BUILD_DIR/assets $APP_DIR/Contents/Resources
    cp -r $BUILD_DIR/shaders $APP_DIR/Contents/Resources
    cp ./Info.plist $APP_DIR/Contents/Info.plist

    codesign --sign "Apple Development: Parker Pestana (883DSPZG9N)" \
    --timestamp \
    --options runtime \
    --deep \
    $APP_DIR

    ditto -c -k --sequesterRsrc --keepParent $APP_DIR waffle_zombies.zip

    xcrun notarytool submit waffle_zombies.zip --apple-id "parkerpestana@gmail.com" --team-id 883DSPZG9N --password "zhww-lzbh-uwnp-eozk" --wait

    xcrun stapler staple waffle_zombies_arm64.app

    ditto -c -k --sequesterRsrc --keepParent waffle_zombies.app waffle_zombies_final_arm64.zip
fi

if [[ "$1" == "-r" ]]; then
	echo "Auto running the program..."
	$BUILD_DIR/platform_main
fi
