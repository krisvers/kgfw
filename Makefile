mac:
	clang main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program -Wno-deprecated-declarations -Ilib/include -Ilib/mac/include -Llib/mac -Flib/mac/frameworks -lglfw3 -framework Cocoa -framework IOKit -rpath lib/mac -lMoltenVK -lvulkan -framework OpenAL -lm -DKGFW_VULKAN

linux:
	clang main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program -Ilib/include -lglfw -lGL -lopenal -lm -DKGFW_DEBUG -DKGFW_OPENGL=33

run:
	pylauncher ./program $(PWD)
