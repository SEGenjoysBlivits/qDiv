/*
qDivClient - The Graphical side of the 2D sandbox game "qDiv"
Copyright (C) 2023  Gabriel F. Hodges

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#define STB_IMAGE_IMPLEMENTATION
#include<stdio.h>
#include<unistd.h>
#include "include/glad/glad.h"
#include<GLFW/glfw3.h>
#include</usr/include/cglm/cglm.h>
#include<stb/stb_image.h>
#include "qDivBridge.h"
#define QDIV_DRAW(count, offset) glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (const GLvoid*)(offset))
#define QDIV_MATRIX_UPDATE() glUniformMatrix3fv(matrixUniform, 1, GL_FALSE, (const GLfloat*)&motion)
#define QDIV_MATRIX_RESET() glUniformMatrix3fv(matrixUniform, 1, GL_FALSE, (const GLfloat*)&matrix)
#define QDIV_COLOR_UPDATE(red, green, blue, alpha) glUniform4fv(colorUniform, 1, (vec4){red, green, blue, alpha})
#define QDIV_COLOR_RESET() glUniform4fv(colorUniform, 1, (vec4){1.0, 1.0, 1.0, 1.0})

const float qBlock = 0.03125f;

uint32_t indexData[98304];
float fieldMS[32][32][2][327680];

// Graphics
GLFWwindow* window;
int32_t windowSquare = 715;
GLuint FVBO[32][32][2], FVAO[32][32][2];
GLuint EBO;
uint32_t floorTex[1];
uint32_t wallTex[1];
GLuint windowUniform;
GLuint offsetUniform;
GLuint scaleUniform;
GLuint colorUniform;
GLuint matrixUniform;
GLuint samplerUni;
mat3 motion;
mat3 matrix = {
	1.f, 0.f, 0.f,
	0.f, 1.f, 0.f,
	0.f, 0.f, 1.f
};

field_t local;

void fillVertices(float* restrict meshIQ, int32_t vertexSL, float posX, float posY, float posXP, float posYP) {
	meshIQ[vertexSL++] = posX;
	meshIQ[vertexSL++] = posY;
	meshIQ[vertexSL++] = posX;
	meshIQ[vertexSL++] = posYP;
	meshIQ[vertexSL++] = posXP;
	meshIQ[vertexSL++] = posYP;
	meshIQ[vertexSL++] = posXP;
	meshIQ[vertexSL++] = posY;
}

void prepareIndexData() {
	int32_t indexSL = 0;
	int32_t value = 0;
	while(indexSL < 98304) {
		indexData[indexSL++] = value + 0;
		indexData[indexSL++] = value + 1;
		indexData[indexSL++] = value + 3;
		indexData[indexSL++] = value + 1;
		indexData[indexSL++] = value + 2;
		indexData[indexSL++] = value + 3;
		value += 4;
	}
}

void loadTexture(uint32_t *texture, const uint8_t *file, size_t fileSZ) {
	GLFWimage textureIQ;
	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	textureIQ.pixels = stbi_load_from_memory(file, fileSZ, &textureIQ.width, &textureIQ.height, 0, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureIQ.width, textureIQ.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureIQ.pixels);
	stbi_image_free(textureIQ.pixels);
}

// Grand Manager
void renderField(int32_t lclX, int32_t lclY, int32_t layerIQ) {
	int32_t vertexSL = 0;
	int32_t blockX = 127;
	int32_t blockY = 127;
	float initX, initY, texX, texY, posX, posY, sizeX, sizeY;
	float* meshIQ = fieldMS[lclX][lclY][layerIQ];
	block_st* blockIQ;
	glBindVertexArray(FVAO[lclX][lclY][layerIQ]);
	glBindBuffer(GL_ARRAY_BUFFER, FVBO[lclX][lclY][layerIQ]);
	initX = (float)(lclX - 16) * 4.f - qBlock;
	initY = (float)(lclY - 16) * 4.f - qBlock;
	posX = initX;
	posY = initY;
	while(vertexSL < 131072) {
		blockIQ = &block[layerIQ][local.block[blockX][blockY][layerIQ]];
		sizeX = blockIQ -> sizeX;
		sizeY = blockIQ -> sizeY;
		texX = posX - (sizeX - blockT) * 2.f;
		fillVertices(meshIQ, vertexSL, texX, posY, texX + (sizeX * 4.f), posY + (sizeY * 4.f));
		texX = blockIQ -> texX;
		texY = blockIQ -> texY;
		fillVertices(meshIQ, vertexSL + 131072, texX, texY, texX + sizeX, texY + sizeY);
		vertexSL += 8;
		blockX--;
		posX -= qBlock;
		if(blockX == -1) {
			blockX = 127;
			blockY--;
			posX = initX;
			posY -= qBlock;
		}
	}
	vertexSL = 262144;
	while(vertexSL < 327680) meshIQ[vertexSL++] = 1.f;
	/*blockX = 127;
	blockY = 127;
	while(vertexSL < 327680) {
		if(blockX < 6 || blockY < 6) {
			meshIQ[vertexSL++] = 0.f;
			meshIQ[vertexSL++] = 0.f;
			meshIQ[vertexSL++] = 0.f;
			meshIQ[vertexSL++] = 0.f;
		}else{
			meshIQ[vertexSL++] = 1.f;
			meshIQ[vertexSL++] = 1.f;
			meshIQ[vertexSL++] = 1.f;
			meshIQ[vertexSL++] = 1.f;
		}
		blockX--;
		if(blockX == -1) {
			blockX = 127;
			blockY--;
		}
	}*/
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fieldMS[lclX][lclY][layerIQ]), meshIQ);
	QDIV_DRAW(98304, 0);
}

int32_t main() {
	printf("\n–––– qDivNoiseSimulator ––––\n\n");
	int8_t* bufferIQ;
	int32_t result;
	int32_t seed = 0;
	int32_t radius = 16;
	size_t bufferSZ = QSMread("settings.qsm", &bufferIQ);
	if(bufferIQ != NULL) {
		QSMencode(&result, (void*)&windowSquare, bufferIQ, bufferSZ, NULL, "WindowSize", 10);
		QSMencode(&result, (void*)&seed, bufferIQ, bufferSZ, NULL, "WorldSeed", 9);
		QSMencode(&result, (void*)&radius, bufferIQ, bufferSZ, NULL, "SimulationRadius", 16);
	}
	free(bufferIQ);
	puts("> Initialising GLFW");
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	int8_t windowVersion[32];
	sprintf(windowVersion, "qDivNoiseSimulator");
	window = glfwCreateWindow(windowSquare, windowSquare, windowVersion, NULL, NULL);
	glfwMakeContextCurrent(window);
	gladLoadGL();
	puts("> Preparing Index Data");
	prepareIndexData();
	puts("> Creating Buffers");
	uint32_t VS = glCreateShader(GL_VERTEX_SHADER);
	uint32_t FS = glCreateShader(GL_FRAGMENT_SHADER);
	uint32_t PS = glCreateProgram();
	const GLchar* vertexFL = (const GLchar *)shaders_vertexshader_glsl;
	const GLchar* fragmentFL = (const GLchar *)shaders_fragmentshader_glsl;
	glShaderSource(VS, 1, &vertexFL, NULL);
	glShaderSource(FS, 1, &fragmentFL, NULL);
	glCompileShader(VS);
	glCompileShader(FS);
	glAttachShader(PS, VS);
	glAttachShader(PS, FS);
	glLinkProgram(PS);
	glGenBuffers(1, &EBO);
	glGenVertexArrays(2048, (GLuint*)FVAO);
	glGenBuffers(2048, (GLuint*)FVBO);
	int32_t bfrX = 0;
	int32_t bfrY = 0;
	int32_t bfrL = 0;
	while(bfrL < 2) {
		glBindVertexArray(FVAO[bfrX][bfrY][bfrL]);
		glBindBuffer(GL_ARRAY_BUFFER, FVBO[bfrX][bfrY][bfrL]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(fieldMS) / 2048, fieldMS[bfrX][bfrY][bfrL], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexData), indexData, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)(131072*sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)(262144*sizeof(float)));
		glEnableVertexAttribArray(2);
		bfrX++;
		if(bfrX == 32) {
			bfrX = 0;
			bfrY++;
			if(bfrY == 32) {
				bfrY = 0;
				bfrL++;
			}
		}
	}
	glEnable(GL_BLEND);
	puts("> Adding Block Types");
	makeBlocks();
	puts("> Loading Textures");
	loadTexture(floorTex + 0, floor0_png, floor0_pngSZ);
	loadTexture(wallTex + 0, wall0_png, wall0_pngSZ);
	glUseProgram(PS);
	colorUniform = glGetUniformLocation(PS, "color");
	matrixUniform = glGetUniformLocation(PS, "matrix");
	samplerUni = glGetUniformLocation(PS, "sampler");
	glm_scale2d_to(matrix, (vec2){1.0 / (double)(radius * 4), 1.0 / (double)(radius * 4)}, motion);
	glm_translate2d(motion, (vec2){4.0, 4.0});
	QDIV_MATRIX_UPDATE();
	QDIV_COLOR_RESET();
	glUniform1i(samplerUni, 0);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	puts("> Graphical Environment running");
	int32_t seedIQ;
	while(!glfwWindowShouldClose(window)) {
		getchar();
		glClear(GL_COLOR_BUFFER_BIT);
		if(seed == 0) {
			QDIV_RANDOM(&seedIQ, sizeof(int32_t));
		}else{
			seedIQ = seed;
		}
		initGenerator(seedIQ);
		int32_t lclX = 16 - radius;
		int32_t lclY = 16 - radius;
		int32_t count = 0;
		while(lclY < 16 + radius) {
			local.fldX = lclX - 16;
			local.fldY = lclY - 16;
			useGenerator(&local);
			glBindTexture(GL_TEXTURE_2D, floorTex[0]);
			renderField(lclX, lclY, 0);
			glBindTexture(GL_TEXTURE_2D, wallTex[0]);
			renderField(lclX, lclY, 1);
			printf("\r> Rendering: %d / %d", count, radius * radius * 4);
			lclX++;
			count++;
			if(lclX == 16 + radius) {
				lclX = 16 - radius;
				lclY++;
			}
		}
		printf("\r> Seed: %d          \n", seedIQ);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glfwTerminate();
	return 0;
}
