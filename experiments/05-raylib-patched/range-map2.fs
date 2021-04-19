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

void main() {
	vec4 colorCenter = texture(texture0, fragTexCoord);
	vec4 colorTop    = texture(texture0, fragTexCoord - dFdy(fragTexCoord.y));
	vec4 colorRight  = texture(texture0, fragTexCoord + dFdx(fragTexCoord.x));
	vec4 colorBottom = texture(texture0, fragTexCoord + dFdy(fragTexCoord.y));
	vec4 colorLeft   = texture(texture0, fragTexCoord - dFdx(fragTexCoord.x));
	
	float highlightWeight = float(colorCenter == selectedColor);
	float borderWeight = float(any(bvec4(colorCenter != colorTop, colorCenter != colorRight, colorCenter != colorBottom, colorCenter != colorLeft)));
	
	vec3 displayColor = vec3(0);
	switch (uint(colorCenter.a * 255)) {
		case uint(0):  // unused
			discard;
			break;
		case uint(1):  // gap
			displayColor = vec3(0.98);
			break;
		case uint(2):  // file backed range
			float rand1 = colorCenter.r;
			float rand2 = float((uint(colorCenter.g * 255) >> 0) & uint(0x0f)) / 16.0;
			float rand3 = float((uint(colorCenter.g * 255) >> 4) & uint(0x0f)) / 16.0;
			displayColor = hsl2rgb(vec3(rand1, 0.25 + 0.25 * rand2, 0.8 + rand3 * 0.1));
			break;
		case uint(3):  // anonymous range
			displayColor = vec3(0.9);
			break;
	}
	
	finalColor = vec4(displayColor * mix(1, 1.05, highlightWeight) * mix(1, 0.9, borderWeight), 1.0);
}