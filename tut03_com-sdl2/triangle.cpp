/**
 * From the OpenGL Programming wikibook: http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Use glew.h instead of gl.h to get all the GL prototypes declared */
#include <GL/glew.h>
/* Using SDL2 for the base window and OpenGL context init */
#include "SDL.h"

#include "../common-sdl2/shader_utils.h"

GLuint vbo_triangle, vbo_triangle_colors;
GLuint program;
GLint attribute_coord2d, attribute_v_color;
GLint uniform_fade;

struct attributes {
	GLfloat coord2d[2];
	GLfloat v_color[3];
};

int init_resources() {
	/*
	  GLfloat triangle_vertices[] = {
	  0.0,  0.8,
	  -0.8, -0.8,
	  0.8, -0.8,
	  };
	*/
	/*
	  GLfloat triangle_attributes[] = {
	  0.0,  0.8,   1.0, 1.0, 0.0,
	  -0.8, -0.8,   0.0, 0.0, 1.0,
	  0.8, -0.8,   1.0, 0.0, 0.0,
	  };
	*/
	struct attributes triangle_attributes[] = {
		{{ 0.0,  0.8}, {1.0, 1.0, 0.0}},
		{{-0.8, -0.8}, {0.0, 0.0, 1.0}},
		{{ 0.8, -0.8}, {1.0, 0.0, 0.0}}
	};
	glGenBuffers(1, &vbo_triangle);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_triangle);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_attributes), triangle_attributes, GL_STATIC_DRAW);
	
	/*
	  GLfloat triangle_colors[] = {
	  1.0, 1.0, 0.0,
	  0.0, 0.0, 1.0,
	  1.0, 0.0, 0.0,
	  };
	  glGenBuffers(1, &vbo_triangle_colors);
	  glBindBuffer(GL_ARRAY_BUFFER, vbo_triangle_colors);
	  glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_colors), triangle_colors, GL_STATIC_DRAW);
	*/
	
	GLint link_ok = GL_FALSE;
	
	GLuint vs, fs;
	if ((vs = create_shader("triangle.v.glsl", GL_VERTEX_SHADER))   == 0) return 0;
	if ((fs = create_shader("triangle.f.glsl", GL_FRAGMENT_SHADER)) == 0) return 0;
	
	program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
	if (!link_ok) {
		fprintf(stderr, "glLinkProgram:");
		print_log(program);
		return 0;
	}
	
	const char* attribute_name;
	attribute_name = "coord2d";
	attribute_coord2d = glGetAttribLocation(program, attribute_name);
	if (attribute_coord2d == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", attribute_name);
		return 0;
	}
	attribute_name = "v_color";
	attribute_v_color = glGetAttribLocation(program, attribute_name);
	if (attribute_v_color == -1) {
		fprintf(stderr, "Could not bind attribute %s\n", attribute_name);
		return 0;
	}
	const char* uniform_name;
	uniform_name = "fade";
	uniform_fade = glGetUniformLocation(program, uniform_name);
	if (uniform_fade == -1) {
		fprintf(stderr, "Could not bind uniform_fade %s\n", uniform_name);
		return 0;
	}
	
	return 1;
}

void logic() {
	// alpha 0->1->0 every 5 seconds
	float cur_fade = sinf(SDL_GetTicks() / 1000.0 * (2*3.14) / 5) / 2 + 0.5;
	glUseProgram(program);
	glUniform1f(uniform_fade, cur_fade);
}

void render(SDL_Window* window)
{
  glClearColor(1.0, 1.0, 1.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(program);

  /*
  glEnableVertexAttribArray(attribute_coord2d);
  /\* Describe our vertices array to OpenGL (it can't guess its format automatically) *\/
  glBindBuffer(GL_ARRAY_BUFFER, vbo_triangle);
  glVertexAttribPointer(
    attribute_coord2d, // attribute
    2,                 // number of elements per vertex, here (x,y)
    GL_FLOAT,          // the type of each element
    GL_FALSE,          // take our values as-is
    0,                 // no extra data between each position
    0                  // offset of first element
  );

  glEnableVertexAttribArray(attribute_v_color);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_triangle_colors);
  glVertexAttribPointer(
    attribute_v_color, // attribute
    3,                 // number of elements per vertex, here (x,y)
    GL_FLOAT,          // the type of each element
    GL_FALSE,          // take our values as-is
    0,                 // no extra data between each position
    0                  // offset of first element
  );
  */

  glEnableVertexAttribArray(attribute_coord2d);
  glEnableVertexAttribArray(attribute_v_color);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_triangle);
  glVertexAttribPointer(
    attribute_coord2d,   // attribute
    2,                   // number of elements per vertex, here (x,y)
    GL_FLOAT,            // the type of each element
    GL_FALSE,            // take our values as-is
    sizeof(struct attributes),  // next coord2d appears every 5 floats
    0                    // offset of first element
  );
  glVertexAttribPointer(
    attribute_v_color,      // attribute
    3,                      // number of elements per vertex, here (r,g,b)
    GL_FLOAT,               // the type of each element
    GL_FALSE,               // take our values as-is
    sizeof(struct attributes),  // stride
    //(GLvoid*) (2 * sizeof(GLfloat))     // offset of first element
    (GLvoid*) offsetof(struct attributes, v_color)  // offset
  );

  /* Push each element in buffer_vertices to the vertex shader */
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glDisableVertexAttribArray(attribute_coord2d);
  glDisableVertexAttribArray(attribute_v_color);
  SDL_GL_SwapWindow(window);
}

void free_resources()
{
  glDeleteProgram(program);
  glDeleteBuffers(1, &vbo_triangle);
  //glDeleteBuffers(1, &vbo_triangle_colors);
}

void mainLoop(SDL_Window* window) {
	while (true) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT)
				return;
		}
		logic();
		render(window);
	}
}

int main(int argc, char* argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("My Triangular Fade",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		640, 480,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	if (SDL_GL_CreateContext(window) == NULL) {
		fprintf(stderr, "Error: SDL_GL_CreateContext: %s\n", SDL_GetError());
		return 1;
	}

	GLenum glew_status = glewInit();
	if (glew_status != GLEW_OK) {
		fprintf(stderr, "Error: glewInit: %s\n", glewGetErrorString(glew_status));
		return 1;
	}
	if (!GLEW_VERSION_2_0) {
		fprintf(stderr, "Error: your graphic card does not support OpenGL 2.0\n");
		return 1;
	}

	if (!init_resources())
		return 1;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	mainLoop(window);
	
	free_resources();
	return 0;
}