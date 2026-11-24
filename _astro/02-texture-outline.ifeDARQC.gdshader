// Written by Anatolii Zhukov, 2026. Released under CC0 1.0 (public domain).
// https://creativecommons.org/publicdomain/zero/1.0/

shader_type canvas_item;

uniform vec2 offset = vec2(8.0, 8.0); 
uniform vec4 outline_color : source_color = vec4(0.0, 0.0, 0.0, 1.0);

void fragment() {
	vec2 offset_uv = UV - (offset * TEXTURE_PIXEL_SIZE);
	vec4 base_color = texture(TEXTURE, UV);
	vec4 shadow_sample = texture(TEXTURE, offset_uv);
	vec4 final_shadow = vec4(outline_color.rgb, outline_color.a * shadow_sample.a);
	
	COLOR = mix(final_shadow, base_color, base_color.a);
}