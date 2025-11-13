FLAGS="-Wall -g -DDEBUG=1"

BUILD_DIR="./build_debug"
ENV="Debug"

DEBUG_GAME_DEPENDENCIES="$BUILD_DIR/imgui.a"

DEBUG_PLAT_DEPENDENCIES="$BUILD_DIR/imgui.a $BUILD_DIR/imgui_backend.a"

if [[ "$1" == "-release" || "$1" == "-mac" ]]; then
    echo "Compiling in release mode..."
	DEBUG_GAME_DEPENDENCIES=""
	DEBUG_PLAT_DEPENDENCIES=""
    FLAGS="-O3 -arch arm64 -mmacosx-version-min=10.7"
    BUILD_DIR="./build_release"
	ENV="Release"
fi

if [[ "$1" == "-h" ]]; then
	echo "Deleting build directory and rebuilding..."
	rm -rf $BUILD_DIR
	bear -- ./build.sh 
	exit 1
fi

GAME_CLANG_CMD="clang++ -std=c++14 $FLAGS $DEBUG_GAME_DEPENDENCIES ./src/game_main.cpp -dynamiclib -o $BUILD_DIR/game_main.dylib -I./lib -I./lib/imgui"

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

if [ ! -e ./assets/asset_ids.h -o ! -e ./assets/sprite_atlas.png -o ! -e ./assets/asset_tables.h ]; then
	./game_tools/asset_manager/ase_bmp_extract.sh
	./game_tools/asset_manager/build.sh
	./game_tools/asset_manager/pack
fi

cp ./assets/asset_ids.h src
cp ./assets/asset_tables.h src
cp ./assets/sprite_atlas.png ./resources/assets

cp -r ./resources/* $BUILD_DIR/resources

# Capture the start time in nanoseconds using gdate
start_time=$(gdate +%s%N)
if [ ! -e ./lib/mac_osx/glad.o ]; then
    clang++ -arch arm64 -mmacosx-version-min=10.7 -x c ./lib/glad/src/glad.c -o ./lib/mac_osx/glad.o -c -I./lib/glad/include
fi

if [ "$ENV" = "Debug" ] && { [ ! -e "$BUILD_DIR/imgui.a" ] || [ ! -e "$BUILD_DIR/imgui_backend.a" ]; }; then
    mkdir -p "$BUILD_DIR"

    echo "Building ImGui core..."
    for f in ./lib/imgui/*.cpp; do
        base=$(basename "$f" .cpp)
        clang++ -std=c++14 -Wall -g -DDEBUG=1 -c "$f" -I./lib/imgui -o "$BUILD_DIR/$base.o"
    done
    ar rcs "$BUILD_DIR/imgui.a" "$BUILD_DIR"/*.o

    echo "Building ImGui backends..."
    clang++ -std=c++14 -Wall -g -DDEBUG=1 -c ./lib/imgui/backends/imgui_impl_sdl3.cpp -I./lib/imgui -o "$BUILD_DIR/imgui_impl_sdl3.o"
    clang++ -std=c++14 -Wall -g -DDEBUG=1 -c ./lib/imgui/backends/imgui_impl_opengl3.cpp -I./lib/imgui -o "$BUILD_DIR/imgui_impl_opengl3.o"

    ar rcs "$BUILD_DIR/imgui_backend.a" "$BUILD_DIR/imgui_impl_sdl3.o" "$BUILD_DIR/imgui_impl_opengl3.o"
fi

# if [ ! -e "$BUILD_DIR/mac_osx.o" ] || [ ./src/mac_osx.cpp -nt "$BUILD_DIR/mac_osx.o" ]; then
#     echo "Recompiling mac_osx.cpp..."
#     clang++ -std=c++14 -g -DDEBUG=1 -c ./src/mac_osx.cpp -o "$BUILD_DIR/mac_osx.o"
# fi

# Want to ensure that we have fresh debug, I found that on re-run the dSYMs were
# not being regenerated
rm -rf $BUILD_DIR/platform_main.dSYM
rm -rf $BUILD_DIR/game_main.dylib.dSYM

clang++ -std=c++14 $FLAGS $DEBUG_PLAT_DEPENDENCIES ./src/platform_main.cpp -o $BUILD_DIR/platform_main -L$BUILD_DIR -lSDL3 -Wl,-rpath,@loader_path -I./lib/glad/include -I./lib -I./lib/imgui -I./lib/imgui/backends ./lib/mac_osx/glad.o
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
