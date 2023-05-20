mkdir -p build_wasm_54 && cd build_wasm_54
emcmake cmake -DLUA_VERSION=5.4.6 -DCMAKE_BUILD_TYPE=Release ..
cd ..
cmake --build build_wasm_54 --config Release
mkdir -p plugin_lua54/Plugins/WebGL/
cp build_wasm_54/libxlua.a plugin_lua54/Plugins/WebGL/libxlua.a
