#version 330

uniform sampler2D texture0;
uniform vec4      selectedColor;

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

void main() {
	vec4 colorCenter = texture(texture0, fragTexCoord);
	vec4 colorTop    = texture(texture0, fragTexCoord - dFdy(fragTexCoord.y));
	vec4 colorRight  = texture(texture0, fragTexCoord + dFdx(fragTexCoord.x));
	vec4 colorBottom = texture(texture0, fragTexCoord + dFdy(fragTexCoord.y));
	vec4 colorLeft   = texture(texture0, fragTexCoord - dFdx(fragTexCoord.x));
	
	float highlightWeight = float(colorCenter == selectedColor);
	float borderWeight = float(any(bvec4(colorCenter != colorTop, colorCenter != colorRight, colorCenter != colorBottom, colorCenter != colorLeft)));
	finalColor = vec4(colorCenter.rgb * mix(1, 1.25, highlightWeight) * mix(1, 0.9, borderWeight), 1.0);
}