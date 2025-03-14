#ifndef SHADERS_H
#define SHADERS_H

constexpr char vertex_shader[] =
"#version 410 core\n                                                  "
"void main() {\n                                                      "
"  vec2 position = vec2(gl_VertexID % 2, gl_VertexID / 2) * 4.0 - 1;\n"
"  gl_Position = vec4(position, 0.0, 1.0);\n                          "
"}                                                                    ";

constexpr char fragment_shader[] =
"#version 410 core\n                                         "
"uniform sampler2D u_tex;\n                                  "
"uniform ivec2 u_fb_size;\n                                  "
"layout(location = 0) out vec4 f_color;\n                    "
"void main() {\n                                             "
"  vec2 f_uv = gl_FragCoord.xy / vec2(u_fb_size);\n          "
"  f_color = vec4(texture(u_tex, f_uv).rgb, 1.0);\n          "
"}                                                           ";

#endif // include guard
