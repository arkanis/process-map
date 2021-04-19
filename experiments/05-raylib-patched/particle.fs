#version 330

uniform vec4 color;

//flat in uint age;

out vec4 finalColor;

void main() {
	finalColor = vec4(color.rgb, color.a * (1 - length(gl_PointCoord.xy - vec2(0.5)) * 2));
}