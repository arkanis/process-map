CFLAGS = -std=c99 -Wall -Wextra -Werror
CPPFLAGS = -I deps/include
LDLIBS += -lGL -lm -lpthread -ldl -lrt -lX11


map_shader: deps/lib/libraylib.a


deps/raylib.tar.gz:
	wget https://github.com/raysan5/raylib/archive/refs/tags/3.5.0.tar.gz -O deps/raylib.tar.gz

deps/lib/libraylib.a: deps/raylib.tar.gz
	mkdir -p deps/raylib
	tar -xaf deps/raylib.tar.gz -C deps/raylib --strip-components=1
	cd deps/raylib/src; make -j 8
	mkdir -p deps/lib
	mv deps/raylib/libraylib.a deps/lib
	mkdir -p deps/include
	mv deps/raylib/src/*.h deps/include  # copies more than we want but doesn't matter for this experiment
	rm -rf deps/raylib


clean:
	rm -rf map_shader deps/include deps/lib