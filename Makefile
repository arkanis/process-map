# -Wno-unused-parameter is needed for raygui.h
CFLAGS = -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CPPFLAGS = -I deps/include
LDLIBS += -lGL -lm -lpthread -ldl -lrt -lX11


map: deps/lib/libraylib.a


deps/raylib.tar.gz:
	wget https://github.com/raysan5/raylib/archive/refs/tags/3.5.0.tar.gz -O deps/raylib.tar.gz

deps/lib/libraylib.a: deps/raylib.tar.gz
	mkdir -p deps/raylib
	tar -xaf deps/raylib.tar.gz -C deps/raylib --strip-components=1
	cd deps/raylib/src; make -j 8 PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_33
	mkdir -p deps/lib
	mv deps/raylib/libraylib.a deps/lib
	mkdir -p deps/include
	cp deps/raylib/src/*.h deps/include  # copies more than we want but doesn't matter for the experiments
	cp deps/raylib/src/external/glad.h deps/include
	rm -rf deps/raylib


clean:
	rm -rf map deps/include deps/lib deps/raylib