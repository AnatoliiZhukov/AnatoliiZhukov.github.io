// Written by Anatolii Zhukov, 2026. Released under CC0 1.0 (public domain).
// https://creativecommons.org/publicdomain/zero/1.0/

shader_type canvas_item;

uniform float x_rotation : hint_range(-180.0, 180.0) = 0.0;
uniform float y_rotation : hint_range(-180.0, 180.0) = 0.0;
uniform float depth : hint_range(0.1, 100.0) = 2.0; 
uniform vec2 rect_size = vec2(100.0, 100.0);

// 0.5, 0.5 = center
uniform vec2 pivot = vec2(0.5, 0.5); 

varying vec2 persp_uv;
varying float persp_w;

void vertex() {
	// Shift origin using the new pivot uniform
	vec2 center = rect_size * pivot;
	vec3 pos = vec3(VERTEX - center, 0.0);
	
	float sx = sin(radians(x_rotation));
	float cx = cos(radians(x_rotation));
	vec3 p1 = vec3(
		pos.x,
		pos.y * cx - pos.z * sx,
		pos.y * sx + pos.z * cx
	);
	
	float sy = sin(radians(y_rotation));
	float cy = cos(radians(y_rotation));
	vec3 p2 = vec3(
		p1.x * cy + p1.z * sy,
		p1.y,
		p1.x * -sy + p1.z * cy
	);
	
	float camera_dist = depth * rect_size.x;
	float w = (camera_dist - p2.z) / camera_dist;
	
	w = max(0.001, w);
	
	VERTEX = (p2.xy / w) + center;
	
	persp_uv = UV / w;
	persp_w = 1.0 / w;
}

void fragment() {
	vec2 real_uv = persp_uv / persp_w;
	COLOR = texture(TEXTURE, real_uv) * COLOR;
}