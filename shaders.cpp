
#include "barebones/main.hpp"

void create_shaders(main_t& main) {
	main.set_shared_program("g3d_single_frame",main.create_program(
		"uniform mat4 MVP_MATRIX;\n"
		"uniform mat3 NORMAL_MATRIX;\n"
		"attribute vec3 VERTEX_0;\n"
		"attribute vec3 NORMAL_0;\n"
		"attribute vec2 TEX_COORD_0;\n"
		"varying vec2 tex_coord_0;\n"
		"varying vec3 normal;\n"
		"void main() {\n"
		"	gl_Position = MVP_MATRIX * vec4(VERTEX_0,1.);\n"
		"	normal = NORMAL_MATRIX * NORMAL_0;\n"
		"	tex_coord_0 = TEX_COORD_0;\n"
		"}\n",
		"uniform vec4 COLOUR;\n"
		"uniform sampler2D TEX_UNIT_0;\n"
		"uniform vec3 LIGHT_0;\n"
		"varying vec2 tex_coord_0;\n"
		"varying vec3 normal;\n"
		"void main() {\n"
		"	vec3 texel = texture2D(TEX_UNIT_0,tex_coord_0).rgb;\n"
		"	float intensity = min(max(dot(LIGHT_0,normal),.6),1.);\n"
		"	gl_FragColor = vec4(COLOUR.rgb * texel * intensity,COLOUR.a);\n"
		"}\n"));
	main.set_shared_program("g3d_multi_frame",main.create_program(
		"uniform mat4 MVP_MATRIX;\n"
		"uniform mat3 NORMAL_MATRIX;\n"
		"uniform float LERP;\n"
		"attribute vec3 VERTEX_0;\n"
		"attribute vec3 NORMAL_0;\n"
		"attribute vec3 VERTEX_1;\n"
		"attribute vec3 NORMAL_1;\n"
		"attribute vec2 TEX_COORD_0;\n"
		"varying vec2 tex_coord_0;\n"
		"varying vec3 normal;\n"
		"void main() {\n"
		"	gl_Position = mix(MVP_MATRIX * vec4(VERTEX_0,1.),MVP_MATRIX * vec4(VERTEX_1,1.),LERP);\n"
		"	normal = mix(NORMAL_MATRIX * NORMAL_0,NORMAL_MATRIX * NORMAL_1,LERP);\n"
		"	tex_coord_0 = TEX_COORD_0;\n"
		"}\n",
		"uniform vec4 COLOUR;\n"
		"uniform sampler2D TEX_UNIT_0;\n"
		"uniform vec3 LIGHT_0;\n"
		"varying vec2 tex_coord_0;\n"
		"varying vec3 normal;\n"
		"void main() {\n"
		"	vec3 texel = texture2D(TEX_UNIT_0,tex_coord_0).rgb;\n"
		"	float intensity = min(max(dot(LIGHT_0,normal),0.6),1.);\n"
		"	gl_FragColor = vec4(COLOUR.rgb * texel * intensity,COLOUR.a);\n"
		"}\n"));
	main.set_shared_program("path_t",main.create_program(
			"uniform mat4 MVP_MATRIX;\n"
			"attribute vec2 VERTEX;\n"
			"void main() {\n"
			"	gl_Position = MVP_MATRIX * vec4(VERTEX,-2.,1.);\n"
			"}\n",
			"uniform vec4 COLOUR;\n"
			"void main() {\n"
			"	gl_FragColor = COLOUR;\n"
			"}\n"));
}

