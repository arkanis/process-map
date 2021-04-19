#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

const float threshold = 0;
const float zero_level = 128;
const float dist_per_pixel = 64.0;

void main() {
	float distance = (texture(texture0, fragTexCoord).a * 255.0 - zero_level) / dist_per_pixel - threshold;
	float distance_change_to_next_pixel = length(vec2(dFdx(distance), dFdy(distance)));
	float content_coverage = smoothstep(-distance_change_to_next_pixel, distance_change_to_next_pixel, distance);
	
	if (content_coverage == 0.0f) {
		discard;
	} else {
		finalColor = vec4(fragColor.rgb, fragColor.a * content_coverage);
	}
}