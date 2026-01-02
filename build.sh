
LINUX="LINUX"
WINDOWS="WIN32"
MAC="MAC"
OS="unknown"

MAC_LIB_DIR="./lib/mac_osx"
LINUX_LIB_DIR="./lib/linux"
WIN32_LIB_DIR="./lib/win32"

LIB_DIR="unknown"

FLAGS="-Wall -g -DDEBUG=1"
SILENCED_WARNINGS="-Wno-c99-designator -Wno-reorder-init-list"

BUILD_DIR="./build_debug"
ENV="Debug"

GAME_EXECUTABLE_NAME="unknown"
TMP_GAME_EXECUTABLE_NAME="unknown"
CURR_GAME_EXECUTABLE_NAME="unknown"

DEBUG_GAME_DEPENDENCIES="$BUILD_DIR/imgui.a"
DEBUG_PLAT_DEPENDENCIES="$BUILD_DIR/imgui.a $BUILD_DIR/imgui_backend.a"

UNAME=$(uname -s)

case "$UNAME" in
	Linux)
		OS=$LINUX
		;;
	Darwin)
		OS=$MAC
		;;
	CYGWIN*|MINGW*|MSYS*)
		OS=$WINDOWS
		;;
	*)
		echo "Unknown OS: $UNAME"
		exit 1
esac

echo "Detected OS: $OS"

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

if [[ "$OS" == $MAC ]]; then
	if command -v gdate >/dev/null 2>&1; then
		DATE_CMD="gdate"
	else
		echo "Error: gdate not found. Install with: brew install coreutils"
		exit 1
	fi
	LIB_DIR=$MAC_LIB_DIR;

	GAME_EXECUTABLE_NAME="game_main.dylib"
	TMP_GAME_EXECUTABLE_NAME="$GAME_EXECUTABLE_NAME.tmp"
	CURR_GAME_EXECUTABLE_NAME=$GAME_EXECUTABLE_NAME
	if [[ "$1" == "-g" ]]; then
		CURR_GAME_EXECUTABLE_NAME=$TMP_GAME_EXECUTABLE_NAME	
	fi

	GAME_CLANG_CMD="clang++ -std=c++14 $FLAGS $SILENCED_WARNINGS $DEBUG_GAME_DEPENDENCIES ./src/game_main.cpp -dynamiclib -o $BUILD_DIR/$CURR_GAME_EXECUTABLE_NAME -I./lib -I./lib/imgui"

    GLAD_CLANG_CMD="clang++ -arch arm64 -mmacosx-version-min=10.7 -x c ./lib/glad/src/glad.c -o $LIB_DIR/glad.o -c -I./lib/glad/include"

	PLATFORM_CLANG_CMD="clang++ -std=c++14 $SILENCED_WARNINGS $FLAGS $DEBUG_PLAT_DEPENDENCIES ./src/platform_main.cpp -o $BUILD_DIR/platform_main -L$BUILD_DIR -lSDL3 -Wl,-rpath,@loader_path -I./lib/glad/include -I./lib -I./lib/imgui -I./lib/imgui/backends $LIB_DIR/glad.o"

	mkdir -p $LIB_DIR
	mkdir -p $BUILD_DIR
	cp $LIB_DIR/*.dylib $BUILD_DIR

	# Want to ensure that we have fresh debug, I found that on re-run the dSYMs were
	# not being regenerated
	rm -rf $BUILD_DIR/platform_main.dSYM
	rm -rf $BUILD_DIR/game_main.dylib.dSYM

elif [[ "$OS" == "$LINUX" ]]; then
	DATE_CMD="date"

	GAME_EXECUTABLE_NAME="game_main.so"
	TMP_GAME_EXECUTABLE_NAME="$GAME_EXECUTABLE_NAME.tmp"
	CURR_GAME_EXECUTABLE_NAME=$GAME_EXECUTABLE_NAME
	if [[ "$1" == "-g" ]]; then
		CURR_GAME_EXECUTABLE_NAME=$TMP_GAME_EXECUTABLE_NAME	
	fi

	GAME_CLANG_CMD="clang++ -std=c++14 $FLAGS -Wl,--no-undefined $SILENCED_WARNINGS ./src/game_main.cpp $DEBUG_GAME_DEPENDENCIES -shared -fPIC -o $BUILD_DIR/$CURR_GAME_EXECUTABLE_NAME -I./lib -I./lib/imgui"

	GLAD_CLANG_CMD="clang++ -x c ./lib/glad/src/glad.c -o ./lib/linux/glad.o -c -I./lib/glad/include"

	PLATFORM_CLANG_CMD="clang++ -std=c++14 $SILENCED_WARNINGS $FLAGS ./src/platform_main.cpp $DEBUG_PLAT_DEPENDENCIES -o $BUILD_DIR/platform_main -L$BUILD_DIR -lSDL3 -Wl,-rpath,@loader_path -I./lib/glad/include -I./lib -I./lib/imgui -I./lib/imgui/backends $LINUX_LIB_DIR/glad.o"

	LIB_DIR=$LINUX_LIB_DIR;
	
elif [[ "$OS" == "$WIN32" ]]; then
	DATE_CMD="date"

	GAME_EXECUTABLE_NAME="game_main.dll"
	TMP_GAME_EXECUTABLE_NAME="$GAME_EXECUTABLE_NAME.tmp"
	CURR_GAME_EXECUTABLE_NAME=$GAME_EXECUTABLE_NAME
	if [[ "$1" == "-g" ]]; then
		CURR_GAME_EXECUTABLE_NAME=$TMP_GAME_EXECUTABLE_NAME	
	fi

	LIB_DIR=$WIN32_LIB_DIR;
fi

mkdir -p $LIB_DIR;

# Hot reloading causes the vscode lldb instance to lose debug symbols.
# reload with this "target symbols add build_debug/game_main.dylib.dSYM/Contents/Resources/DWARF/game_main.dylib"
if [[ "$1" == "-g" ]]; then
    echo "Only compiling game_main..."

    start_time=$($DATE_CMD +%s%N)
    $GAME_CLANG_CMD
	mv $BUILD_DIR/$CURR_GAME_EXECUTABLE_NAME $BUILD_DIR/$GAME_EXECUTABLE_NAME
    # if the dSYM file name is different after reloading, lldb does not reload the symbols and connect it to the source map.
	if [[ "$OS" == "$MAC" ]]; then
    	cp -r $BUILD_DIR/game_main.dylib.tmp.dSYM $BUILD_DIR/temp_game_main.dylib.dSYM
	fi
    end_time=$($DATE_CMD +%s%N)
    compile_time=$(( (end_time - start_time) / 1000000 ))
    echo "Compilation completed in ${compile_time} milliseconds."
    exit 1
fi

mkdir -p $BUILD_DIR
mkdir -p $BUILD_DIR/resources

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

cp -r ./resources/assets $BUILD_DIR/resources
cp -r ./resources/shaders $BUILD_DIR/resources

clang-format -i ./src/*

# Capture the start time in nanoseconds using gdate
start_time=$($DATE_CMD +%s%N)
if [[ ! -e $LIB_DIR/glad.o ]]; then
	$GLAD_CLANG_CMD
fi

if [ "$ENV" == "Debug" ] && { [ ! -e "$BUILD_DIR/imgui.a" ] || [ ! -e "$BUILD_DIR/imgui_backend.a" ]; }; then
    mkdir -p "$BUILD_DIR"

    echo "Building ImGui core..."
    for f in ./lib/imgui/*.cpp; do
        base=$(basename "$f" .cpp)
        clang++ -std=c++14 -Wall -g -DDEBUG=1 -c "$f" -I./lib/imgui -fPIC -o "$BUILD_DIR/$base.o"
    done
    ar rcs "$BUILD_DIR/imgui.a" "$BUILD_DIR"/*.o

    echo "Building ImGui backends..."
    clang++ -std=c++14 -Wall -g -DDEBUG=1 -c ./lib/imgui/backends/imgui_impl_sdl3.cpp -I./lib/imgui -o "$BUILD_DIR/imgui_impl_sdl3.o"
    clang++ -std=c++14 -Wall -g -DDEBUG=1 -c ./lib/imgui/backends/imgui_impl_opengl3.cpp -I./lib/imgui -o "$BUILD_DIR/imgui_impl_opengl3.o"

    ar rcs "$BUILD_DIR/imgui_backend.a" "$BUILD_DIR/imgui_impl_sdl3.o" "$BUILD_DIR/imgui_impl_opengl3.o"
fi

$PLATFORM_CLANG_CMD
$GAME_CLANG_CMD

# Capture the end time in nanoseconds using gdate
end_time=$($DATE_CMD +%s%N)

# Calculate the compile time in milliseconds
compile_time=$(( (end_time - start_time) / 1000000 ))

# Print the compile time
echo "Compilation completed in ${compile_time} milliseconds."

if [[ "$1" == "-r" ]]; then
	echo "Auto running the program..."
	$BUILD_DIR/platform_main
fi

# if [ ! -e "$BUILD_DIR/mac_osx.o" ] || [ ./src/mac_osx.cpp -nt "$BUILD_DIR/mac_osx.o" ]; then
#     echo "Recompiling mac_osx.cpp..."
#     clang++ -std=c++14 -g -DDEBUG=1 -c ./src/mac_osx.cpp -o "$BUILD_DIR/mac_osx.o"
# fi

# if [[ "$1" == "-mac" ]]; then
#     APP_DIR="./waffle_zombies_arm64.app"
#     echo "Building app file..."
#     mkdir -p $APP_DIR/Contents/MacOS
#     mkdir -p $APP_DIR/Contents/Resources
#     cp $BUILD_DIR/platform_main $APP_DIR/Contents/MacOS
#     cp -r $BUILD_DIR/*.dylib $APP_DIR/Contents/MacOS
#     cp $BUILD_DIR/game_main.dylib $APP_DIR/Contents/Resources
#
#     mkdir -p $APP_DIR/Contents/Resources/assets
#     cp -r $BUILD_DIR/assets $APP_DIR/Contents/Resources
#     cp -r $BUILD_DIR/shaders $APP_DIR/Contents/Resources
#     cp ./Info.plist $APP_DIR/Contents/Info.plist
#
#     codesign --sign "Apple Development: Parker Pestana (883DSPZG9N)" \
#     --timestamp \
#     --options runtime \
#     --deep \
#     $APP_DIR
#
#     ditto -c -k --sequesterRsrc --keepParent $APP_DIR waffle_zombies.zip
#
#     xcrun notarytool submit waffle_zombies.zip --apple-id "parkerpestana@gmail.com" --team-id 883DSPZG9N --password "zhww-lzbh-uwnp-eozk" --wait
#
#     xcrun stapler staple waffle_zombies_arm64.app
#
#     ditto -c -k --sequesterRsrc --keepParent waffle_zombies.app waffle_zombies_final_arm64.zip
# fi

