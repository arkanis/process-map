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
	
	//finalColor = vec4(vec3(colorCenter.r), 1);
	//if (colorCenter == vec4(0))
	//	finalColor = vec4(0, 0, 0.5, 0.25);
	//else
	//	finalColor = fragColor;
	
	float borderWeight = float(any(bvec4(colorCenter != colorTop, colorCenter != colorRight, colorCenter != colorBottom, colorCenter != colorLeft)) && colorCenter != vec4(0));
	finalColor = vec4(fragColor.rgb, fragColor.a * borderWeight);
}

