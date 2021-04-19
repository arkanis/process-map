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
	
	float a = float(uint(colorCenter.g * 255) & uint(0x0f)) / 16.0;
	float b = float(uint(colorCenter.g * 255) >> 4)         / 16.0;
	vec3 displayColor = hsl2rgb(vec3(colorCenter.r, 0.25 + 0.25 * a, 0.8 + b * 0.1));
	finalColor = vec4(displayColor * mix(1, 1.25, highlightWeight) * mix(1, 0.9, borderWeight), 1.0);
}