mac:
	clang main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program -Wno-deprecated-declarations -Ilib/include -Llib/mac -lglfw3 -framework Cocoa -framework IOKit -framework OpenGL -framework OpenAL -lm -DKGFW_DEBUG -DKGFW_OPENGL=33

linux:
	clang main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program -Ilib/include -lglfw -lGL -lopenal -lm -DKGFW_OPENGL=33 -DKGFW_DEBUG

emscripten:
	emcc main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program.html -s USE_WEBGL2=1 -s USE_GLFW=3 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -Ilib/include -lglfw -lGL -lopenal -lm -DKGFW_OPENGL=33 --preload-file assets # -DKGFW_DEBUG

run:
	pylauncher ./program $(PWD)
