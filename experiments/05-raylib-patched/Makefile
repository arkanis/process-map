# -Wno-unused-parameter is needed for raygui.h
CFLAGS = -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CPPFLAGS = -I deps/include
LDLIBS += -lGL -lm -lpthread -ldl -lrt -lX11

BINARIES = 01-particles 02-sdf-text 03-map 04-map-cleanup 05-tasks

all: $(BINARIES)
$(BINARIES): deps/lib/libraylib.a


deps/raylib.tar.gz:
	wget https://github.com/raysan5/raylib/archive/refs/tags/3.5.0.tar.gz -O deps/raylib.tar.gz

deps/lib/libraylib.a: deps/raylib.tar.gz
	mkdir -p deps/raylib
	tar -xaf deps/raylib.tar.gz -C deps/raylib --strip-components=1
	cp deps/rlgl.h deps/raylib/src
	cd deps/raylib/src; make -j 8 PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_33
	mkdir -p deps/lib
	mv deps/raylib/libraylib.a deps/lib
	mkdir -p deps/include
	cp deps/raylib/src/*.h deps/include  # copies more than we want but doesn't matter for the experiments
	cp deps/raylib/src/external/glad.h deps/include
	#rm -rf deps/raylib  # we might want to view the source code, so don't delete for now


clean:
	rm -rf $(BINARIES) deps/include deps/lib deps/raylib