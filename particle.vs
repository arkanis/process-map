#version 330

uniform mat4 mvp;
uniform uint currentTime;

in vec2 pos;
in uint time;

//out uint age;

void main() {
	uint age = currentTime - time;
    gl_Position = mvp * vec4(pos, 0.0, 1.0);
    gl_PointSize = (age < 2u) ? 20 : 10;
}