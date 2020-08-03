if [ -z "$ANDROID_NDK" ]; then
    export ANDROID_NDK=~/android-ndk-r19c
fi

PLATFORM=mac
if [ "$1" == "linux" ]; then
	PLATFORM=linux
fi

TOOL_CHAIN_PATH=$ANDROID_NDK/build

echo make armeabi-v7a ================================================
if [ "$PLATFORM" == "linux" ]; then
	# linux
	STRIP_PATH=$ANDROID_NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-strip
else
	# mac
	STRIP_PATH=$ANDROID_NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/darwin-x86_64/bin/arm-linux-androideabi-strip
fi

mkdir -p build_v7a && cd build_v7a
cmake -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI=armeabi-v7a -DCMAKE_TOOLCHAIN_FILE=$TOOL_CHAIN_PATH/cmake/android.toolchain.cmake -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-clang3.6 -DANDROID_NATIVE_API_LEVEL=android-9 -DANDROID_STRIP_EXEC=$STRIP_PATH ../
cd ..
cmake --build build_v7a --config Release
mkdir -p plugin_lua53/Plugins/Android/libs/armeabi-v7a/
cp build_v7a/libxlua.so plugin_lua53/Plugins/Android/libs/armeabi-v7a/libxlua.so

echo make x86 ================================================
if [ "$PLATFORM" == "linux" ]; then
	# linux
	STRIP_PATH=$ANDROID_NDK/toolchains/x86-4.9/prebuilt/linux-x86_64/bin/i686-linux-android-strip
else
	# mac
	STRIP_PATH=$ANDROID_NDK/toolchains/x86-4.9/prebuilt/darwin-x86_64/bin/i686-linux-android-strip
fi

function build() {
    API=$1
    ABI=$2
    TOOLCHAIN_ANME=$3
    BUILD_PATH=build.Android.${ABI}
    cmake -H. -B${BUILD_PATH} -DANDROID_ABI=${ABI} -DCMAKE_TOOLCHAIN_FILE=${NDK}/build/cmake/android.toolchain.cmake -DANDROID_NATIVE_API_LEVEL=${API} -DANDROID_TOOLCHAIN=clang -DANDROID_TOOLCHAIN_NAME=${TOOLCHAIN_ANME}
    cmake --build ${BUILD_PATH} --config Release
    mkdir -p plugin_lua53/Plugins/Android/libs/${ABI}/
    cp ${BUILD_PATH}/libxlua.so plugin_lua53/Plugins/Android/libs/${ABI}/libxlua.so
}

build android-16 armeabi-v7a arm-linux-androideabi-4.9
build android-16 arm64-v8a  arm-linux-androideabi-clang
build android-16 x86 x86-4.9
