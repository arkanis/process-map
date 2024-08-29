#version 330

uniform sampler2D texture0;
uniform vec4      selectedColor;

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

// From https://www.shadertoy.com/view/XljGzV
vec3 hsl2rgb(in vec3 c) {
    vec3 rgb = clamp( abs(mod(c.x*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0 );
    return c.z + c.y * (rgb-0.5)*(1.0-abs(2.0*c.z-1.0));
}

// The first two components of the range color contain 2*8 bits of randomness. Convert that to one float with 8 bits of
// randomness (x component of result) and two floats with 4 bits of randomness (y and z of result). Easier to construct
// and HSL color with those tree values.
vec3 randInRangeColorToVec3(vec4 rangeColor) {
	return vec3(
		rangeColor.r,  // full 8 bits of randomness
		float((uint(rangeColor.g * 255) >> 0) & uint(0x0f)) / 16.0,
		float((uint(rangeColor.g * 255) >> 4) & uint(0x0f)) / 16.0
	);
}

void main() {
	vec4 colorCenter = texture(texture0, fragTexCoord);
	vec4 colorTop    = texture(texture0, fragTexCoord - dFdy(fragTexCoord.y));
	vec4 colorRight  = texture(texture0, fragTexCoord + dFdx(fragTexCoord.x));
	vec4 colorBottom = texture(texture0, fragTexCoord + dFdy(fragTexCoord.y));
	vec4 colorLeft   = texture(texture0, fragTexCoord - dFdx(fragTexCoord.x));
	
	float highlightWeight = float(colorCenter == selectedColor);
	float borderWeight = float(any(bvec4(colorCenter != colorTop, colorCenter != colorRight, colorCenter != colorBottom, colorCenter != colorLeft)));
	
	vec3 displayColor = vec3(0);
	vec3 rand = randInRangeColorToVec3(colorCenter);
	switch (uint(colorCenter.a * 255)) {
		case uint(0):  // unused
			discard;
			break;
		case uint(1):  // gap
			displayColor = vec3(0.98);
			break;
		case uint(2):  // file backed range with no permission information or no permission bits whatsoever (not even writable)
			displayColor = vec3(0.8);
			break;
		case uint(3):  // anonymous range
			displayColor = vec3(0.9);
			break;
		
		case uint(4):  // executable range, red hues
			displayColor = hsl2rgb(vec3((rand.x - 0.5) * 0.15 + 0.00, 0.25 + 0.25 * rand.y, 0.8 + rand.z * 0.1));
			break;
		case uint(5):  // writable range, blue hues
			displayColor = hsl2rgb(vec3((rand.x - 0.5) * 0.15 + 0.66, 0.25 + 0.25 * rand.y, 0.8 + rand.z * 0.1));
			break;
		case uint(6):  // readable range, green hues
			displayColor = hsl2rgb(vec3((rand.x - 0.5) * 0.15 + 0.33, 0.25 + 0.25 * rand.y, 0.8 + rand.z * 0.1));
			break;
	}
	
	finalColor = vec4(displayColor * mix(1, 1.05, highlightWeight) * mix(1, 0.9, borderWeight), 1.0);
}