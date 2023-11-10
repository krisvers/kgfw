mac:
	clang main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program -Wno-deprecated-declarations -Ilib/include -Llib/mac -lglfw3 -framework Cocoa -framework IOKit -framework OpenGL -framework OpenAL -lm -DKGFW_DEBUG -DKGFW_OPENGL=33

linux:
	clang main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program -Ilib/include -lglfw -lvulkan -lopenal -lm -DKGFW_VULKAN

run:
	pylauncher ./program $(PWD)
