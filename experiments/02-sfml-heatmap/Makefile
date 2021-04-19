CC := gcc
CXX := g++
CPPFLAGS := -I deps/include/
CFLAGS   := 
CXXFLAGS := -std=c++11 -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS  := -L deps/lib/
LDLIBS   := 

dev-packages: deps/SFML-dev-packages deps/mruby-dev-packages


#
# Targets for binaries
#
main: LDLIBS += $(SFML_LIBS)
main: deps/lib/libsfml-graphics-s.a deps/lib/libsfml-window-s.a deps/lib/libsfml-audio-s.a deps/lib/libsfml-network-s.a deps/lib/libsfml-system-s.a


# SFML: Download and compile as static library
SFML_LIBS := -lXrandr -ludev -lpthread -lrt -lGL -lX11 -lXext -lfreetype -ljpeg

deps/SFML-dev-packages:
	sudo apt-get install libudev-dev libfreetype6-dev libjpeg-dev libx11-dev libxrandr-dev libxcb1-dev libx11-xcb-dev libxcb-randr0-dev libxcb-image0-dev libgl1-mesa-dev libflac-dev libogg-dev libvorbis-dev libvorbisenc2 libvorbisfile3 libopenal-dev

deps/SFML-2.5.1-sources.zip:
	wget https://www.sfml-dev.org/files/SFML-2.5.1-sources.zip -O deps/SFML-2.5.1-sources.zip

deps/include/SFML deps/lib/libsfml-audio-s.a deps/lib/libsfml-graphics-s.a deps/lib/libsfml-network-s.a deps/lib/libsfml-system-s.a deps/lib/libsfml-window-s.a: deps/SFML-2.5.1-sources.zip
	unzip deps/SFML-2.5.1-sources.zip -d deps
	mkdir deps/SFML-2.5.1/build
	cd deps/SFML-2.5.1/build && cmake -D BUILD_SHARED_LIBS=FALSE .. && make -j 4
	mkdir -p deps/lib
	mv deps/SFML-2.5.1/build/lib/*.a deps/lib
	mkdir -p deps/include
	mv deps/SFML-2.5.1/include/SFML deps/include
	rm -rf deps/SFML-2.5.1