#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

/*
// NOTE: Add here your custom variables
const float smoothing = 1.0/16.0;

void main()
{
    // Texel color fetching from texture sampler
    // NOTE: Calculate alpha using signed distance field (SDF)
    float distance = texture(texture0, fragTexCoord).a;
    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);
    
    // Calculate final fragment color
    finalColor = vec4(fragColor.rgb, fragColor.a*alpha);
}
*/


const float threshold = 0;
const float zero_level = 128;
const float dist_per_pixel = 64.0;


// Improved texture hardware interpolation from http://www.iquilezles.org/www/articles/hwinterpolation/hwinterpolation.htm
// Note that the article uses the opposite convention for texture coordinates (uv = normalized coordinates [0..1],
// st = non-normalized coordinates [0..w] and [0..h]).
vec4 textureGood(sampler2D sam, vec2 uv) {
	vec2 res = textureSize(sam, 0);
	vec2 st = uv*res - 0.5;
	vec2 iuv = floor( st );
	vec2 fuv = fract( st );
	
	vec4 a = texture2D( sam, (iuv+vec2(0.5,0.5))/res );
	vec4 b = texture2D( sam, (iuv+vec2(1.5,0.5))/res );
	vec4 c = texture2D( sam, (iuv+vec2(0.5,1.5))/res );
	vec4 d = texture2D( sam, (iuv+vec2(1.5,1.5))/res );
	
	return mix( mix( a, b, fuv.x), mix( c, d, fuv.x), fuv.y );
}

void main() {
	// OpenGL conventsion for texture coordinates (see https://stackoverflow.com/a/13622636, 2nd comment)
	vec2 st = fragTexCoord.xy;  // texture coords in normalized texture coordinates [0..1]
	
	// The normal texture2D() function causes stair artifacts when we're zoomed in on the edge of
	// the shape. This is caused by the 24.8 fixed point hardware implementation of bilinear filtering.
	// See http://www.iquilezles.org/www/articles/hwinterpolation/hwinterpolation.htm. It can only
	// produce 256 steps between two texels, no matter how far they're apart on the screen. When we
	// render a 32x32 shape with very high magnification this is not enough. Solved by doing the
	// bilinear filterting by hand with the textureGood() function shown in the article.
	//float distance = (textureGood(texture0, st).a * 255.0 - zero_level) / dist_per_pixel - threshold;
	float distance = (texture(texture0, st).a * 255.0 - zero_level) / dist_per_pixel - threshold;
	
	// Code explained in the first shader. We just use it here and focus on the texture interpolation.
	float distance_change_to_next_pixel = length(vec2(dFdx(distance), dFdy(distance)));
	
	float content_coverage = smoothstep(-distance_change_to_next_pixel, distance_change_to_next_pixel, distance);
	
	if (content_coverage == 0.0f) {
		discard;
	} else {
		finalColor = vec4(fragColor.rgb, fragColor.a * content_coverage);
	}
}