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
#define QDIV_CLIENT
#ifdef _WIN32
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "Ws2_32.lib")
#include<winsock2.h>
#include<windows.h>
#include<ws2tcpip.h>
#define _GLFW_WIN32
#else
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<signal.h>
#define _GLFW_X11
#endif
#define STB_IMAGE_IMPLEMENTATION
#include<stdio.h>
#include<pthread.h>
#include<time.h>
#include "include/glad/glad.h"
#include<GLFW/glfw3.h>
#include</usr/include/cglm/cglm.h>
#include<stb/stb_image.h>
#include "qDivLib.h"
#include "elements.h"
#define PRIMITIVE_MODE

enum {
	START_MENU,
	SERVER_MENU,
	SETTINGS_MENU,
	CONNECTING_MENU,
	INGAME_MENU,
	ARTIFACT_MENU,
	ARTIFACT_INFO
} menuEnum;

enum {
	TEXT_LEFT,
	TEXT_RIGHT,
	TEXT_CENTER
} textTypeEnum;

enum {
	BUTTON_ACTION,
	BUTTON_OPTION,
	BUTTON_PROMPT
} buttonTypeEnum;

typedef void(*artifactRenderer)(int, int, double, double, entityData*, playerData*, void*);

typedef struct {
	char name[16];
	char desc[256];
	bool crossCriterial;
	unsigned int texture;
	artifactRenderer primary;
	double primaryUseTime;
	artifactRenderer secondary;
	double secondaryUseTime;
	unsigned long long qEnergy;
	struct {
		int Template;
		unsigned int value;
	} criterion[QDIV_ARTIFACT_CRITERIA];
} artifactSettings;

const float qBlock = 0.03125f;

float vertexData[] = {
	0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Pos
	0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Tex
	1.0f, 1.0f, 1.0f, 1.0f // Lgt
};
unsigned int indexData[98304];
float menuGrid[8000];
float fieldMesh[3][3][2][327680];

GLFWwindow* window;
int windowWidth = 715;
int windowHeight = 715;
int windowSquare = 715;
GLuint VBO, VAO;
GLuint MVBO, MVAO;
GLuint AVBO, AVAO;
GLuint FVBO[3][3][2], FVAO[3][3][2];
GLuint EBO;
unsigned int logoTex;
unsigned int text;
unsigned int interfaceTex;
unsigned int dummyTex;
unsigned int foundationTex;
unsigned int floorTex[4];
unsigned int wallTex;
unsigned int artifactTex[6];
unsigned int selectionTex;
unsigned int blankTex;
GLuint windowUniform;
GLuint offsetUniform;
GLuint scaleUniform;
GLuint colorUniform;
GLuint matrixUniform;
#define TRANSFORM glUniformMatrix3fv(matrixUniform, 1, GL_FALSE, (const GLfloat*)&motion)
#define RESET glUniformMatrix3fv(matrixUniform, 1, GL_FALSE, (const GLfloat*)&matrix)
GLuint samplerUni;
mat3 motion;
mat3 matrix = {
	1.f, 0.f, 0.f,
	0.f, 1.f, 0.f,
	0.f, 0.f, 1.f
};
struct timeval qFrameStart;
struct timeval qFrameEnd;
double qTime;
int currentMenu;
int* pMenu = &currentMenu;
struct {
	int role;
	int page;
} artifactMenu;

struct {
	char name[16];
	int nameCursor;
	char server[16][39];
	int serverCursor[16];
	bool vsync;
	bool cursorsync;
} settings;

char currentServer[39];
char uuid[16];

bool LeftClick;
bool RightClick;
volatile int Keyboard = 0x00;
volatile int* pKeyboard = &Keyboard;
char utf8_Text = 0x00;
char* putf8_Text = &utf8_Text;
unsigned int promptCursor = 0;

bool ConnectButton = false;
bool* pConnectButton = &ConnectButton;

int connection = OFFLINE_NET;
int* pconnection = &connection;
int* sockPT;

double cursorX;
double cursorY;
double* pcursorX = &cursorX;
double* pcursorY = &cursorY;
double syncedCursorX;
double syncedCursorY;
double* psyncedCursorX = &syncedCursorX;
double* psyncedCursorY = &syncedCursorY;
fieldData local[3][3];
int lightMap[384][384];

entityData entityDF;
entityData* entitySelf = &entityDF;
unsigned int criterionSelf[MAX_CRITERION];

bool meshTask[3][3][2] = {false};
bool lightTask[3][3][2] = {false};
bool blockTask[3][3][128][128][2] = {false};
bool shutterTask[3][2][384] = {false};

artifactSettings artifact[6][4000];

double doubMin(double a, double b) {
	return a < b ? a : b;
}

// Packet
void makeDirectionPacket(int direction) {
	SendPacket[0] = 0x02;
	memcpy(SendPacket+1, &direction, sizeof(int));
}

// Packet
void makeArtifactRequest(int role, int artifact) {
	SendPacket[0] = 0x08;
	memcpy(SendPacket+1, &role, sizeof(int));
	memcpy(SendPacket+5, &artifact, sizeof(int));
}

// Packet
void makeUsagePacket(int usage, double inRelX, double inRelY) {
	SendPacket[0] = 0x0A;
	usageData usageIQ;
	usageIQ.usage = usage;
	usageIQ.useRelX = inRelX;
	usageIQ.useRelY = inRelY;
	memcpy(SendPacket+1, &usageIQ, sizeof(usageData));
}

bool hoverCheck(double minX, double minY, double maxX, double maxY) {
	return cursorX < maxX && cursorY < maxY && cursorX > minX && cursorY > minY;
}

void screenToBlock(double inX, double inY, int* outX, int* outY, int* lclX, int* lclY) {
	*lclX = 1;
	*lclY = 1;
	*outX = (int)floor(inX * 32.0 + entitySelf -> posX);
	if(*outX < 0) {
		*outX += 128;
		*lclX = 0;
	}else if(*outX >= 128) {
		*outX -= 128;
		*lclX = 2;
	}
	*outY = (int)floor(inY * 32.0 + entitySelf -> posY);
	if(*outY < 0) {
		*outY += 128;
		*lclY = 0;
	}else if(*outY >= 128) {
		*outY -= 128;
		*lclY = 2;
	}
}

// Callback
void windowCallback(GLFWwindow* window, int width, int height) {
	int higher = width < height;
	windowSquare = higher ? width : height;
	glViewport(!higher * ((width / 2) - windowSquare / 2), higher * ((height / 2) - windowSquare / 2), windowSquare, windowSquare);
	windowWidth = width;
	windowHeight = height;
}

// Callback
void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
	LeftClick = button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS;
	RightClick = button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS;
	if(currentMenu == INGAME_MENU && hoverCheck(-0.5f, -0.5f, 0.5f, 0.5f)) {
		if(action == GLFW_RELEASE) {
			entitySelf -> unique.Player.currentUsage = NO_USAGE;
			makeUsagePacket(NO_USAGE, cursorX * 32.0, cursorY * 32.0);
			send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
		}else if(action == GLFW_PRESS) {
			if(button == GLFW_MOUSE_BUTTON_LEFT) entitySelf -> unique.Player.currentUsage = PRIMARY_USAGE;
			if(button == GLFW_MOUSE_BUTTON_RIGHT) entitySelf -> unique.Player.currentUsage = SECONDARY_USAGE;
			makeUsagePacket(entitySelf -> unique.Player.currentUsage, cursorX * 32.0, cursorY * 32.0);
			send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
		}
	}
}

// Callback
void cursorCallback(GLFWwindow* window, double xpos, double ypos) {
	cursorX = ((xpos - (windowWidth - windowSquare) * 0.5) / (double)windowSquare - 0.5) * 2;
	cursorY = ((-ypos - (windowHeight - windowSquare) * 0.5) / (double)windowSquare + 0.5) * 2;
}

// Callback
void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if(currentMenu == INGAME_MENU) {
		if(key == GLFW_KEY_UP) {
			if(action == GLFW_PRESS) {
				makeDirectionPacket(NORTH);
				send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
			}
			if(action == GLFW_RELEASE) {
				makeDirectionPacket(NORTH_STOP);
				send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
			}
		}
		if(key == GLFW_KEY_RIGHT) {
			if(action == GLFW_PRESS) {
				makeDirectionPacket(EAST);
				send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
			}
			if(action == GLFW_RELEASE) {
				makeDirectionPacket(EAST_STOP);
				send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
			}
		}
		if(key == GLFW_KEY_DOWN) {
			if(action == GLFW_PRESS) {
				makeDirectionPacket(SOUTH);
				send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
			}
			if(action == GLFW_RELEASE) {
				makeDirectionPacket(SOUTH_STOP);
				send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
			}
		}
		if(key == GLFW_KEY_LEFT) {
			if(action == GLFW_PRESS) {
				makeDirectionPacket(WEST);
				send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
			}
			if(action == GLFW_RELEASE) {
				makeDirectionPacket(WEST_STOP);
				send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
			}
		}
	}
	if(action == GLFW_PRESS) {
		Keyboard = key;
	}else{
		Keyboard = 0x00;
	}
}

// Callback
void typingCallback(GLFWwindow* window, unsigned int codepoint) {
	utf8_Text = codepoint;
}

void setupBufferAttributes(unsigned int vaoIQ, unsigned int vboIQ, float* vertices, GLenum usage, size_t size1, size_t size2) {
	glBindVertexArray(vaoIQ);
	glBindBuffer(GL_ARRAY_BUFFER, vboIQ);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, usage);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)(size1 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)(size2 * sizeof(float)));
	glEnableVertexAttribArray(2);
}

void setScalePos(float scaleX, float scaleY, float offsetX, float offsetY) {
	glm_scale2d_to(matrix, (vec2){scaleX, scaleY}, motion);
	glm_translate2d(motion, (vec2){offsetX / scaleX, offsetY / scaleY});
	TRANSFORM;
}

void setColor(float red, float green, float blue, float alpha) {
	glUniform4fv(colorUniform, 1, (vec4){red, green, blue, alpha});
}

void stxysetColor(float scaleX, float scaleY, float offsetX, float offsetY, float red, float green, float blue, float alpha) {
	setScalePos(scaleX, scaleY, offsetX, offsetY);
	setColor(red, green, blue, alpha);
}

void uniformDefaults() {
	stxysetColor(1.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f);
}

void fillVertices(float* restrict meshIQ, int vertexSL, float posX, float posY, float posXP, float posYP) {
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
	int indexSL = 0;
	int value = 0;
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

void prepareMesh() {
	int bfrX = 0;
	int bfrY = 0;
	while(bfrY < 3) {
		int vertexSL = 0;
		float initX = -6.f + (float)bfrX * 4.f;
		float initY = -6.f + (float)bfrY * 4.f;
		float posX = initX;
		float posY = initY;
		while(vertexSL < 131072) {
			fillVertices(fieldMesh[bfrX][bfrY][0], vertexSL, posX, posY, posX + qBlock, posY + qBlock);
			fillVertices(fieldMesh[bfrX][bfrY][1], vertexSL, posX, posY, posX + qBlock, posY + qBlock);
			vertexSL += 8;
			posX += qBlock;
			if(posX >= initX + 4.f) {
				posX = initX;
				posY += qBlock;
			}
		}
		bfrX++;
		if(bfrX == 3) {
			bfrX = 0;
			bfrY++;
		}
	}
}

void prepareGrid() {
	int vertexSL = 0;
	float posX = -1.f;
	float posY = -1.f;
	while(vertexSL < 3200) {
		fillVertices(menuGrid, vertexSL, posX, posY, posX + 0.1f, posY + 0.1f);
		vertexSL += 8;
		posX += 0.1f;
		if(posX >= 1.f) {
			posX = -1.f;
			posY += 0.1f;
		}
	}
	float texX = 0.25f;
	float texY = 0.f;
	while(vertexSL < 6400) {
		fillVertices(menuGrid, vertexSL, texX, texY, texX + 0.25f, texY + 0.25f);
		vertexSL += 8;
		switch(vertexSL) {
			case 3208:
				texX = 0.5f;
				break;
			case 3352:
				texX = 0.75f;
				break;
			case 3360:
				texX = 0.f;
				break;
			case 6240:
				texX = 0.25f;
				break;
			case 6248:
				texX = 0.5f;
				break;
			case 6392:
				texX = 0.75f;
				break;
		}
	}
	while(vertexSL < 8000) menuGrid[vertexSL++] = 1.f;
}

void setAtlasArea(float inX, float inY, float inSize) {
	static float texX = 0.f;
	static float texY = 0.f;
	static float size = 1.f;
	if(inX != texX|| inY != texY || size != inSize) {
		texX = inX;
		texY = inY;
		size = inSize;
		fillVertices(vertexData, 8, inX, inY, inX + inSize, inY + inSize);
		for(int vertexSL = 16; vertexSL < 20; vertexSL++) vertexData[vertexSL] = 1.0f;
		glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertexData) / 5 * 2, sizeof(vertexData) / 5 * 2, &vertexData[8]);
	}
}

void loadTexture(unsigned int *texture, const unsigned char *file, size_t fileSZ) {
	GLFWimage textureIQ;
	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	textureIQ.pixels = stbi_load_from_memory(file, fileSZ, &textureIQ.width, &textureIQ.height, 0, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureIQ.width, textureIQ.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureIQ.pixels);
	if(file == logo_png) glfwSetWindowIcon(window, 1, &textureIQ);
	stbi_image_free(textureIQ.pixels);
}

bool isArtifactUnlocked(int roleSL, int artifactSL, entityData* entityIQ) {
	if(entityIQ -> qEnergy < artifact[roleSL][artifactSL].qEnergy) return false;
	for(int criterionSL = 0; criterionSL < QDIV_ARTIFACT_CRITERIA; criterionSL++) {
		int TemplateSL = artifact[roleSL][artifactSL].criterion[criterionSL].Template;
		if(TemplateSL != NO_CRITERION && artifact[roleSL][artifactSL].criterion[criterionSL].value > entityIQ -> unique.Player.criterion[TemplateSL]) return false;
	}
	return true;
}

// Action Renderer
void simpleSwing(int lclX, int lclY, double posX, double posY, entityData* entityIQ, playerData* playerIQ, void* restrict artifactVD) {
	artifactSettings* restrict artifactIQ = (artifactSettings* restrict)artifactVD;
	glm_scale2d_to(matrix, (vec2){qBlock * 2, qBlock * 2}, motion);
	glm_translate2d(motion, (vec2){(entityIQ -> posX - entitySelf -> posX) * 0.5, (entityIQ -> posY - entitySelf -> posY) * 0.5});
	glm_rotate2d(motion, playerIQ -> useTimer * 6.28 / artifactIQ -> primaryUseTime);
	TRANSFORM;
	glBindTexture(GL_TEXTURE_2D, artifactIQ -> texture);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Entity Renderer
void minimum(void* entityVD) {
	entityData* entityIQ = (entityData*)entityVD;
	double boxIQ = entityType[entityIQ -> type].hitBox;
	double scaleIQ = qBlock * boxIQ;
	glm_scale2d_to(matrix, (vec2){scaleIQ, scaleIQ}, motion);
	glm_translate2d(motion, (vec2){(entityIQ -> posX - entitySelf -> posX + (entityIQ -> fldX - entitySelf -> fldX) * 128.0 - 0.5) / boxIQ, (entityIQ -> posY - entitySelf -> posY + (entityIQ -> fldY - entitySelf -> fldY) * 128.0 - 0.5) / boxIQ});
	TRANSFORM;
	glBindTexture(GL_TEXTURE_2D, dummyTex);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void makeEntityTypes() {
	entityType[PLAYER] = makeEntityType(5, true, 2, false, 32.0, 1, &minimum);
	entityType[SHALLAND_SNAIL] = makeEntityType(5, true, 4, false, 0.25, 1, &minimum);
}

artifactSettings makeArtifact(char* inName, char* inDesc, const unsigned char* inTexture, size_t inTextureSZ, bool inCross, unsigned long long inEnergy, int Template1, unsigned int value1, int Template2, unsigned int value2, int Template3, unsigned int value3, int Template4, unsigned int value4, artifactRenderer inPrimary, double inPrimaryTime, artifactRenderer inSecondary, double inSecondaryTime) {
	artifactSettings artifactIQ;
	strcpy(artifactIQ.name, inName);
	strcpy(artifactIQ.desc, inDesc);
	artifactIQ.crossCriterial = inCross;
	loadTexture(&artifactIQ.texture, inTexture, inTextureSZ);
	if(inEnergy > 0) {
		artifactIQ.qEnergy = inEnergy;
		artifactIQ.criterion[0].Template = NO_CRITERION;
		artifactIQ.criterion[0].value = 0;
	}else{
		artifactIQ.qEnergy = 0;
		artifactIQ.criterion[0].Template = Template1;
		artifactIQ.criterion[0].value = value1;
	}
	artifactIQ.criterion[1].Template = Template2;
	artifactIQ.criterion[1].value = value2;
	artifactIQ.criterion[2].Template = Template3;
	artifactIQ.criterion[2].value = value3;
	artifactIQ.criterion[3].Template = Template4;
	artifactIQ.criterion[3].value = value4;
	artifactIQ.primary = inPrimary;
	artifactIQ.primaryUseTime = inPrimaryTime;
	artifactIQ.secondary = inSecondary;
	artifactIQ.secondaryUseTime = inSecondaryTime;
	return artifactIQ;
}

void makeArtifacts() {
	artifact[WARRIOR][OLD_SLICER] = makeArtifact("Old Slicer", "Use this to defeat your\nfirst few enemies.", artifacts_old_slicer_png, artifacts_old_slicer_pngSZ, false, 0, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, &simpleSwing, 0.5, NULL, 0);
	artifact[BUILDER][OLD_SWINGER] = makeArtifact("Old Swinger", "Use this to break your\nfirst few blocks and\nget your first qEnergy.", artifacts_old_swinger_png, artifacts_old_swinger_pngSZ, false, 0, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, &simpleSwing, 1, NULL, 0);
	artifact[BUILDER][BLOCK] = makeArtifact("Block", "First ever artifact of qDiv!\nCan be placed.", artifacts_block_png, artifacts_block_pngSZ, false, 5, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, NULL, 0, NULL, 0);
	artifact[BUILDER][PLASTIC_ARTIFACT] = makeArtifact("Plastic", "Just a test.", artifacts_plastic_png, artifacts_plastic_pngSZ, false, 15, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, NULL, 0, NULL, 0);
	artifact[BUILDER][LAMP_ARTIFACT] = makeArtifact("Block", "Just a test.", artifacts_lamp_png, artifacts_lamp_pngSZ, false, 80, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0, NULL, 0, NULL, 0);
}

void renderText(char* restrict toRender, size_t length, float posX, float posY, float chSC, int typeIQ) {
	posX /= chSC;
	posY /= chSC;
	glBindTexture(GL_TEXTURE_2D, text);
	float actSC = 0.66f;
	switch(typeIQ) {
		case TEXT_RIGHT:
			posX -= (float)length * actSC + actSC;
			break;
		case TEXT_CENTER:
			posX -= ((float)length * actSC) * 0.5f;
			break;
	}
	glm_scale2d_to(matrix, (vec2){chSC, chSC}, motion);
	glm_translate2d(motion, (vec2){posX, posY});
	TRANSFORM;
	bool newLine = false;
	bool whiteSpace = false;
	bool chHG = false;
	double currentPosition = 0;
	for(int chSL = 0; chSL < (int)length; chSL++) {
		switch(toRender[chSL]) {
			case 0x20:
				whiteSpace = true;
				break;
			case '\n':
				newLine = true;
				break;
			case 'g':
			case 'p':
			case 'q':
			case 'y':
				chHG = true;
			default:
				setAtlasArea((float)(toRender[chSL] % 16) * 0.0625f, (float)(toRender[chSL] / 16) * 0.0625f, 0.062f);
		}
		if(chHG) glm_translate2d(motion, (vec2){0.0, -0.225});
		TRANSFORM;
		if(newLine) {
			glm_translate2d(motion, (vec2){-currentPosition, -1.5});
			newLine = 0;
			currentPosition = 0;
		}else{
			if(!whiteSpace) {
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
				if(chHG) glm_translate2d(motion, (vec2){0.0, 0.225});
			}else{
				whiteSpace = 0;
			}
			glm_translate2d(motion, (vec2){0.75, 0.0});
			currentPosition += 0.75;
			chHG = 0;
		}
	}
	setAtlasArea(0.0f, 0.0f, 1.0f);
	uniformDefaults();
}

void renderSelectedArtifact() {
	glBindVertexArray(VAO);
	float texY;
	float scale = 0.1f;
	float posX = -0.975f;
	float posY = 0.775f;
	if(hoverCheck(posX, posY, posX + scale, posY + scale)) {
		texY = 0.25f;
		if(LeftClick) {
			LeftClick = false;
			artifactMenu.role = entitySelf -> unique.Player.role;
			currentMenu = ARTIFACT_MENU;
		}
	}else{
		texY = 0.f;
	}
	glBindTexture(GL_TEXTURE_2D, interfaceTex);
	setAtlasArea(0.f, texY, 0.25f);
	setScalePos(scale, scale, posX, posY);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindTexture(GL_TEXTURE_2D, artifact[entitySelf -> unique.Player.role][entitySelf -> unique.Player.artifact].texture);
	setAtlasArea(0.f, 0.f, 1.f);
	setScalePos(scale * 0.5f, scale * 0.5f, posX + 0.025f, posY + 0.025f);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

bool menuButton(float scale, float posX, float posY, int length, char* restrict nestedText, int* restrict nestedCursor, size_t nestedLength) {
	glBindVertexArray(VAO);
	float texY;
	bool clicked = false;
	float red;
	float green;
	float blue;
	if(hoverCheck(posX, posY, posX + (scale * length), posY + scale)) {
		texY = nestedCursor == NULL ? 0.25f : 0.5f;
		red = nestedCursor == NULL ? 0.22f : 0.52f;
		green = 0.f;
		blue = 0.82f;
		if(LeftClick) {
			LeftClick = false;
			clicked = true;
		}
		if(nestedCursor != NULL) {
			switch(Keyboard) {
				case GLFW_KEY_BACKSPACE:
					if(*nestedCursor != 0) {
						if(nestedText[(*nestedCursor)--] == 0x00) {
							nestedText[*nestedCursor] = 0x00;
						}else{
							memmove(nestedText + *nestedCursor, nestedText + *nestedCursor + 1, nestedLength - *nestedCursor - 1);
							nestedText[nestedLength - 1] = 0x00;
						}
					}
					Keyboard = 0x00;
					break;
				case GLFW_KEY_ENTER:
					clicked = true;
					Keyboard = 0x00;
					break;
				case GLFW_KEY_LEFT:
					if(*nestedCursor > 0) (*nestedCursor)--;
					Keyboard = 0x00;
					break;
				case GLFW_KEY_RIGHT:
					if(nestedText[*nestedCursor] != 0x00 && *nestedCursor != nestedLength - 1) (*nestedCursor)++;
					Keyboard = 0x00;
					break;
			}
			if(utf8_Text != 0x00) {
				if(nestedText[nestedLength - 1] == 0x00) {
					if(nestedText[*nestedCursor + 1] != 0x00) memmove(nestedText + *nestedCursor + 1, nestedText + *nestedCursor, nestedLength - *nestedCursor - 1);
					nestedText[(*nestedCursor)++] = utf8_Text;
				}
				utf8_Text = 0x00;
			}
		}
	}else{
		red = 1.f;
		green = 1.f;
		blue = 1.f;
		texY = 0.f;
	}
	glBindTexture(GL_TEXTURE_2D, interfaceTex);
	setAtlasArea(0.25f, texY, 0.25f);
	setScalePos(scale, scale, posX, posY);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	setAtlasArea(0.5f, texY, 0.25f);
	for(int posSL = 1; posSL < length; posSL++) {
		if(posSL == length - 1) setAtlasArea(0.75f, texY, 0.25f);
		posX += scale;
		setScalePos(scale, scale, posX, posY);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}
	setColor(red, green, blue, 1.f);
	if(nestedCursor == NULL) {
		renderText(nestedText, nestedLength, posX * scale, posY + scale * 0.3f, scale * 0.5f, TEXT_CENTER);
	}else{
		if(red == 0.52f) {
			glBindTexture(GL_TEXTURE_2D, blankTex);
			setScalePos(scale * 0.05f, scale * 0.5f, scale * (posX - 0.5f * (float)length) + (float)(*nestedCursor + 1) * scale * 0.33f, posY + scale * 0.3f);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		}
		renderText(nestedText, nestedLength, scale * (posX - 0.5f * (float)length), posY + scale * 0.3f, scale * 0.5f, TEXT_LEFT);
	}
	return clicked;
}

int screenToArtifact() {
	return (int)((cursorX + 1) * 10) + ((int)((-cursorY + 0.9) * 10)) * 20 + artifactMenu.page * 360;
}

void renderBlockSelection() {
	double posX = settings.cursorsync ? syncedCursorX : cursorX;
	double posY = settings.cursorsync ? syncedCursorY : cursorY;
	double selectX = (floor(posX * 32) - (entitySelf -> posX - floor(entitySelf -> posX))) * qBlock;
	double selectY = (floor(posY * 32) - (entitySelf -> posY - floor(entitySelf -> posY))) * qBlock;
	if(selectX > posX) {
		selectX -= qBlock;
	}else if(selectX < posX - qBlock) {
		selectX += qBlock;
	}
	if(selectY > posY) {
		selectY -= qBlock;
	}else if(selectY < posY - qBlock) {
		selectY += qBlock;
	}
	char energy[32];
	int blockX, blockY, lclX, lclY;
	screenToBlock(settings.cursorsync ? syncedCursorX : cursorX, settings.cursorsync ? syncedCursorY : cursorY, &blockX, &blockY, &lclX, &lclY);
	int* blockPos = local[lclX][lclY].block[blockX][blockY];
	int layerSL = getOccupiedLayer(blockPos);
	if(layerSL == -1) {
		strcpy(energy, "0");
	}else{
		blockType* blockIQ = &block[layerSL][blockPos[layerSL]];
		sprintf(energy, "%llu", blockIQ -> qEnergy);
		if(qEnergyRelevance(entitySelf -> qEnergy, blockIQ)) {
			if(blockIQ -> qEnergyStatic) {
				setColor(sin(qTime * 6.28) * 0.5f + 0.5f, sin(qTime * 6.28) * 0.5f + 0.5f, 1.f, 1.f);
			}else{
				setColor(sin(qTime * 6.28) * 0.5f + 0.5f, 1.f, sin(qTime * 6.28) * 0.5f + 0.5f, 1.f);
			}
		}else{
			setColor(1.f, sin(qTime * 3.14) * 0.5f + 0.5f, sin(qTime * 3.14) * 0.5f + 0.5f, 1.f);
		}
	}
	setScalePos(qBlock, qBlock, selectX, selectY);
	glBindTexture(GL_TEXTURE_2D, selectionTex);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	renderText(energy, strlen(energy), selectX + qBlock * 1.5, selectY, qBlock * 1.5, TEXT_LEFT);
}

void illuminate(int illuminance, int blockX, int blockY) {
	int posX, posY, relPosX, relPosY, calcIllu;
	posY = blockY - illuminance + 1;
	while(posY < blockY + illuminance) {
		if(posY > -1 && posY < 384) {
			posX = blockX - illuminance + 1;
			while(posX < blockX + illuminance) {
				if(posX > -1 && posX < 384) {
					relPosX = abs(posX - blockX);
					relPosY = abs(posY - blockY);
					calcIllu = illuminance - ((relPosX > relPosY) * relPosX + (relPosX <= relPosY) * relPosY);
					if(lightMap[posX][posY] < calcIllu) lightMap[posX][posY] = calcIllu;
				}
				posX++;
			}
		}
		posY++;
	}
}

void fillLightMap() {
	memset(lightMap, 0x00, sizeof(lightMap));
	int blockX = 0;
	int blockY = 0;
	blockType* floorIQ;
	blockType* wallIQ;
	while(blockY < 384) {
		floorIQ = &block[0][local[blockX / 128][blockY / 128].block[blockX % 128][blockY % 128][0]];
		wallIQ = &block[1][local[blockX / 128][blockY / 128].block[blockX % 128][blockY % 128][1]];
		if(wallIQ -> illuminant) {
			illuminate(4, blockX, blockY);
		}else if(wallIQ -> transparent && floorIQ -> illuminant) {
			illuminate(4, blockX, blockY);
		}
		blockX++;
		if(blockX == 384) {
			blockX = 0;
			blockY++;
		}
	}
}

// Grand Manager
void renderField(int lclX, int lclY, int layerIQ) {
	fieldData* fieldIQ = &local[lclX][lclY];
	int vertexSL = 131072;
	int blockX = 0;
	int blockY = 0;
	float* meshIQ = fieldMesh[lclX][lclY][layerIQ];
	blockType* blockIQ;
	glBindVertexArray(FVAO[lclX][lclY][layerIQ]);
	glBindBuffer(GL_ARRAY_BUFFER, FVBO[lclX][lclY][layerIQ]);
	if(meshTask[lclX][lclY][layerIQ]) {
		while(vertexSL < 262144) {
			blockIQ = &block[layerIQ][fieldIQ -> block[blockX][blockY][layerIQ]];
			float posX = blockIQ -> texX;
			float posY = blockIQ -> texY;
			fillVertices(meshIQ, vertexSL, posX, posY, posX + blockT, posY + blockT);
			vertexSL += 8;
			blockX++;
			if(blockX == 128) {
				blockX = 0;
				blockY++;
			}
		}
		glBufferSubData(GL_ARRAY_BUFFER, sizeof(fieldMesh[lclX][lclY][layerIQ]) / 5 * 2, sizeof(fieldMesh[lclX][lclY][layerIQ]) / 5 * 2, &fieldMesh[lclX][lclY][layerIQ][131072]);
		meshTask[lclX][lclY][layerIQ] = false;
	}
	#ifndef PRIMITIVE_MODE
	else{
		while(vertexSL < 262144) {
			if(blockTask[lclX][lclY][blockX][blockY][layerIQ]) {
				blockIQ = &block[layerIQ][fieldIQ -> block[blockX][blockY][layerIQ]];
				float posX = blockIQ -> texX;
				float posY = blockIQ -> texY;
				fillVertices(meshIQ, vertexSL, posX, posY, posX + blockT, posY + blockT);
				glBufferSubData(GL_ARRAY_BUFFER, vertexSL*sizeof(float), 8*sizeof(float), &fieldMesh[lclX][lclY][layerIQ][vertexSL]);
				blockTask[lclX][lclY][blockX][blockY][layerIQ] = false;
			}
			vertexSL += 8;
			blockX++;
			if(blockX == 128) {
				blockX = 0;
				blockY++;
			}
		}
	}
	#endif
	if(lightTask[lclX][lclY][layerIQ]) {
		blockX = lclX * 128;
		blockY = lclY * 128;
		float lgtC, lgtN, lgtE, lgtS, lgtW, valSW, valNW, valNE, valSE;
		bool shdC, shdSW, shdNW, shdNE, shdSE;
		float sunLight = (currentHour > 12 ? (float)(12 - (currentHour % 12)) : (float)currentHour) * 0.083 * 0.5f;
		while(vertexSL < 327680) {
			if(layerIQ == 1 || blockX < 1 || blockX > 382 || blockY < 1 || blockY > 382) {
				shdC = false;
				shdSW = false;
				shdNW = false;
				shdNE = false;
				shdSE = false;
			}else{
				if(!block[1][local[blockX / 128][blockY / 128].block[blockX % 128][blockY % 128][1]].transparent && !block[1][local[blockX / 128][blockY / 128].block[blockX % 128][blockY % 128][1]].illuminant) {
					shdC = true;
					shdSW = true;
					shdNW = true;
					shdNE = true;
					shdSE = true;
				}else{
					int blockN = local[(blockX) / 128][(blockY + 1) / 128].block[(blockX) % 128][(blockY + 1) % 128][1];
					int blockE = local[(blockX + 1) / 128][(blockY) / 128].block[(blockX + 1) % 128][(blockY) % 128][1];
					int blockS = local[(blockX) / 128][(blockY - 1) / 128].block[(blockX) % 128][(blockY - 1) % 128][1];
					int blockW = local[(blockX - 1) / 128][(blockY) / 128].block[(blockX - 1) % 128][(blockY) % 128][1];
					int blockNE = local[(blockX + 1) / 128][(blockY + 1) / 128].block[(blockX + 1) % 128][(blockY + 1) % 128][1];
					int blockNW = local[(blockX - 1) / 128][(blockY + 1) / 128].block[(blockX - 1) % 128][(blockY + 1) % 128][1];
					int blockSE = local[(blockX + 1) / 128][(blockY - 1) / 128].block[(blockX + 1) % 128][(blockY - 1) % 128][1];
					int blockSW = local[(blockX - 1) / 128][(blockY - 1) / 128].block[(blockX - 1) % 128][(blockY - 1) % 128][1];
					shdSW = !(block[1][blockS].transparent && block[1][blockW].transparent && block[1][blockSW].transparent) && !(block[1][blockS].illuminant || block[1][blockW].illuminant || block[1][blockSW].illuminant);
					shdNW = !(block[1][blockN].transparent && block[1][blockW].transparent && block[1][blockNW].transparent) && !(block[1][blockN].illuminant || block[1][blockW].illuminant || block[1][blockNW].illuminant);
					shdNE = !(block[1][blockN].transparent && block[1][blockE].transparent && block[1][blockNE].transparent) && !(block[1][blockN].illuminant || block[1][blockE].illuminant || block[1][blockNE].illuminant);
					shdSE = !(block[1][blockS].transparent && block[1][blockE].transparent && block[1][blockSE].transparent) && !(block[1][blockS].illuminant || block[1][blockE].illuminant || block[1][blockSE].illuminant);
				}
			}
			lgtC = (float)lightMap[blockX][blockY];
			lgtN = (float)lightMap[blockX][blockY + 1];
			lgtE = (float)lightMap[blockX + 1][blockY];
			lgtS = (float)lightMap[blockX][blockY - 1];
			lgtW = (float)lightMap[blockX - 1][blockY];
			valSW = (lgtC + lgtS + lgtW + (float)lightMap[blockX - 1][blockY - 1]) * 0.0625f;
			valNW = (lgtC + lgtN + lgtW + (float)lightMap[blockX - 1][blockY + 1]) * 0.0625f;
			valNE = (lgtC + lgtN + lgtE + (float)lightMap[blockX + 1][blockY + 1]) * 0.0625f;
			valSE = (lgtC + lgtS + lgtE + (float)lightMap[blockX + 1][blockY - 1]) * 0.0625f;
			meshIQ[vertexSL++] = ((sunLight >= valSW) * sunLight + (sunLight < valSW) * valSW) * (shdSW * -0.25f + 1);
			meshIQ[vertexSL++] = ((sunLight >= valNW) * sunLight + (sunLight < valNW) * valNW) * (shdNW * -0.25f + 1);
			meshIQ[vertexSL++] = ((sunLight >= valNE) * sunLight + (sunLight < valNE) * valNE) * (shdNE * -0.25f + 1);
			meshIQ[vertexSL++] = ((sunLight >= valSE) * sunLight + (sunLight < valSE) * valSE) * (shdSE * -0.25f + 1);
			blockX++;
			if(blockX == (lclX + 1) * 128) {
				blockX = lclX * 128;
				blockY++;
			}
		}
		glBufferSubData(GL_ARRAY_BUFFER, sizeof(fieldMesh[lclX][lclY][layerIQ]) / 5 * 4, sizeof(fieldMesh[lclX][lclY][layerIQ]) / 5, &fieldMesh[lclX][lclY][layerIQ][262144]);
		lightTask[lclX][lclY][layerIQ] = false;
	}
	int offsetRN, factorRN;
	switch(lclY) {
		case 0:
			offsetRN = 96;
			factorRN = 32;
			break;
		case 1:
			offsetRN = clampInt(0, (int)(entitySelf -> posY - 32), 96);
			factorRN = 64;
			break;
		case 2:
			offsetRN = 0;
			factorRN = 32;
			break;
		default:
			nonsense(910);
	}
	glDrawElements(GL_TRIANGLES, 768 * factorRN, GL_UNSIGNED_INT, (const GLvoid*)(3072 * offsetRN));
}

// Grand Manager
void renderEntities() {
	entityData* entityIQ;
	entitySettings typeIQ;
	for(int entitySL = 0; entitySL < 10000; entitySL++) {
		if(entitySlot[entitySL] == 1) {
			entityIQ = entity + entitySL;
			if(entityType[entityIQ -> type].action != NULL) {
				(*entityType[entityIQ -> type].action)((void*)entityIQ);
			}else{
				nonsense(911);
			}
		}
	}
}

// Grand Manager
void renderActions() {
	double posX, posY;
	int lclX, lclY, layerSL;
	FOR_EVERY_ENTITY {
		if(entitySlot[entitySL] == 1 && entity[entitySL].type == PLAYER) {
			entityData* entityIQ = entity + entitySL;
			playerData* playerIQ = &entityIQ -> unique.Player;
			if(playerIQ -> currentUsage != NO_USAGE) {
				posX = entityIQ -> posX + playerIQ -> useRelX;
				posY = entityIQ -> posY + playerIQ -> useRelY;
				derelativize(&lclX, &lclY, &posX, &posY);
				artifactSettings* artifactIQ = &artifact[playerIQ -> role][playerIQ -> artifact];
				if(playerIQ -> currentUsage == PRIMARY_USAGE && artifactIQ -> primary != NULL) {
					(*artifactIQ -> primary)(lclX, lclY, posX, posY, entityIQ, playerIQ, (void* restrict)artifactIQ);
				}else if(playerIQ -> currentUsage == SECONDARY_USAGE && artifactIQ -> secondary != NULL) {
					(*artifactIQ -> secondary)(lclX, lclY, posX, posY, entityIQ, playerIQ, (void* restrict)artifactIQ);
				}
			}
		}
	}
}

// Thread
void* thread_usage() {
	while(*pRun) {
		usleep(100000);
		*psyncedCursorX = *pcursorX;
		*psyncedCursorY = *pcursorY;
		if(currentMenu == INGAME_MENU && entitySelf -> unique.Player.currentUsage != NO_USAGE && hoverCheck(-0.5f, -0.5f, 0.5f, 0.5f)) {
			makeUsagePacket(entitySelf -> unique.Player.currentUsage, *pcursorX * 32.0, *pcursorY * 32.0);
			send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
		}
	}
}

// Thread
void* thread_gate() {
	#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 0), &wsa);
	#else
	signal(SIGPIPE, SIG_IGN);
	#endif
	struct sockaddr_in6 server;
	struct timeval qTimeOut;
	int sockSF = socket(AF_INET6, SOCK_STREAM, 0);
	fd_set sockRD;
	sockPT = &sockSF;
	server.sin6_family = AF_INET6;
    server.sin6_port = htons(QDIV_PORT);
    while(*pRun) {
    	usleep(1000);
    	if(*pConnectButton) {
			puts("[Out] Connecting to specified server");
			if(inet_pton(AF_INET6, currentServer, &server.sin6_addr) > 0 && connect(sockSF, (struct sockaddr*)&server, sizeof(server)) >= 0) {
				*pconnection = CONNECTED_NET;
				while(*pconnection != OFFLINE_NET) {
					FD_ZERO(&sockRD);
        			FD_SET(sockSF, &sockRD);
        			qTimeOut.tv_sec = 1;
        			qTimeOut.tv_usec = 0;
        			entityData entityIQ;
        			if(select(sockSF+1, &sockRD, NULL, NULL, &qTimeOut) > 0) {
						memset(ReceivePacket, 0x00, QDIV_PACKET_SIZE);
						read(sockSF, ReceivePacket, QDIV_PACKET_SIZE);
						switch(ReceivePacket[0]) {
							case 0x01:
								*pconnection = OFFLINE_NET;
								*pMenu = START_MENU;
								close(sockSF);
								break;
							case 0x03:
								int entitySL;
								memcpy(&entitySL, &ReceivePacket[1], sizeof(int));
								entitySlot[entitySL] = 0;
								break;
							case 0x04:
								memcpy(&entityIQ, &ReceivePacket[1], sizeof(entityData));
								if(entitySelf == &entity[entityIQ.slot]) {
									entityIQ.unique.Player.criterion = criterionSelf;
									int posSFX = entitySelf -> fldX;
									int posSFY = entitySelf -> fldY;
									int posIQX = entityIQ.fldX;
									int posIQY = entityIQ.fldY;
									if(posSFX != posIQX || posSFY != posIQY) {
										size_t fieldSize = sizeof(local[0][0]);
										if(posIQX > posSFX) {
											memcpy(&local[0][0], &local[1][0], fieldSize);
											memcpy(&local[0][1], &local[1][1], fieldSize);
											memcpy(&local[0][2], &local[1][2], fieldSize);
											memcpy(&local[1][0], &local[2][0], fieldSize);
											memcpy(&local[1][1], &local[2][1], fieldSize);
											memcpy(&local[1][2], &local[2][2], fieldSize);
											meshTask[0][0][0] = true;
											meshTask[0][1][0] = true;
											meshTask[0][2][0] = true;
											meshTask[1][0][0] = true;
											meshTask[1][1][0] = true;
											meshTask[1][2][0] = true;
											meshTask[0][0][1] = true;
											meshTask[0][1][1] = true;
											meshTask[0][2][1] = true;
											meshTask[1][0][1] = true;
											meshTask[1][1][1] = true;
											meshTask[1][2][1] = true;
										}
										if(posIQX < posSFX) {
											memcpy(&local[2][0], &local[1][0], fieldSize);
											memcpy(&local[2][1], &local[1][1], fieldSize);
											memcpy(&local[2][2], &local[1][2], fieldSize);
											memcpy(&local[1][0], &local[0][0], fieldSize);
											memcpy(&local[1][1], &local[0][1], fieldSize);
											memcpy(&local[1][2], &local[0][2], fieldSize);
											meshTask[2][0][0] = true;
											meshTask[2][1][0] = true;
											meshTask[2][2][0] = true;
											meshTask[1][0][0] = true;
											meshTask[1][1][0] = true;
											meshTask[1][2][0] = true;
											meshTask[2][0][1] = true;
											meshTask[2][1][1] = true;
											meshTask[2][2][1] = true;
											meshTask[1][0][1] = true;
											meshTask[1][1][1] = true;
											meshTask[1][2][1] = true;
										}
										if(posIQY > posSFY) {
											memcpy(&local[0][0], &local[0][1], fieldSize);
											memcpy(&local[1][0], &local[1][1], fieldSize);
											memcpy(&local[2][0], &local[2][1], fieldSize);
											memcpy(&local[0][1], &local[0][2], fieldSize);
											memcpy(&local[1][1], &local[1][2], fieldSize);
											memcpy(&local[2][1], &local[2][2], fieldSize);
											meshTask[0][0][0] = true;
											meshTask[1][0][0] = true;
											meshTask[2][0][0] = true;
											meshTask[0][1][0] = true;
											meshTask[1][1][0] = true;
											meshTask[2][1][0] = true;
											meshTask[0][0][1] = true;
											meshTask[1][0][1] = true;
											meshTask[2][0][1] = true;
											meshTask[0][1][1] = true;
											meshTask[1][1][1] = true;
											meshTask[2][1][1] = true;
										}
										if(posIQY < posSFY) {
											memcpy(&local[0][2], &local[0][1], fieldSize);
											memcpy(&local[1][2], &local[1][1], fieldSize);
											memcpy(&local[2][2], &local[2][1], fieldSize);
											memcpy(&local[0][1], &local[0][0], fieldSize);
											memcpy(&local[1][1], &local[1][0], fieldSize);
											memcpy(&local[2][1], &local[2][0], fieldSize);
											meshTask[0][2][0] = true;
											meshTask[1][2][0] = true;
											meshTask[2][2][0] = true;
											meshTask[0][1][0] = true;
											meshTask[1][1][0] = true;
											meshTask[2][1][0] = true;
											meshTask[0][2][1] = true;
											meshTask[1][2][1] = true;
											meshTask[2][2][1] = true;
											meshTask[0][1][1] = true;
											meshTask[1][1][1] = true;
											meshTask[2][1][1] = true;
										}
									}
								}
								entity[entityIQ.slot] = entityIQ;
								entitySlot[entityIQ.slot] = 1;
								break;
							case 0x05:
								fieldSlice sliceIQ;
								memcpy(&sliceIQ, &ReceivePacket[1], sizeof(fieldSlice));
								int lclX = sliceIQ.fldX - entitySelf -> fldX + 1;
								int lclY = sliceIQ.fldY - entitySelf -> fldY + 1;
								if(lclX > -1 && lclX < 3 && lclY > -1 && lclY < 3) {
									if(sliceIQ.posX == 128) {
										local[lclX][lclY].fldX = sliceIQ.fldX;
										local[lclX][lclY].fldY = sliceIQ.fldY;
										meshTask[lclX][lclY][0] = true;
										meshTask[lclX][lclY][1] = true;
									}else{
										memcpy(&local[lclX][lclY].block[sliceIQ.posX], &sliceIQ.block, 256*sizeof(int));
									}
								}else{
									nonsense(828);
								}
								break;
							case 0x06:
								memcpy(&entityIQ, &ReceivePacket[1], sizeof(entityData));
								entity[entityIQ.slot] = entityIQ;
								entitySlot[entityIQ.slot] = 1;
								entitySelf = &entity[entityIQ.slot];
								entitySelf -> unique.Player.criterion = criterionSelf;
								*pMenu = INGAME_MENU;
								*pconnection = CONNECTED_NET;
								break;
							case 0x07:
								blockData dataIQ;
								memcpy(&dataIQ, ReceivePacket+1, sizeof(blockData));
								lclX = dataIQ.fldX - entitySelf -> fldX + 1;
								lclY = dataIQ.fldY - entitySelf -> fldY + 1;
								int* blockIQ = &local[lclX][lclY].block[dataIQ.posX][dataIQ.posY][dataIQ.layer];
								int blockPR = *blockIQ;
								if(lclX > -1 && lclX < 3 && lclY > -1 && lclY < 3) {
									*blockIQ = dataIQ.block;
									blockTask[lclX][lclY][dataIQ.posX][dataIQ.posY][dataIQ.layer] = true;
									if((block[dataIQ.layer][*blockIQ].illuminant || block[dataIQ.layer][blockPR].illuminant > block[dataIQ.layer][dataIQ.block].illuminant) && (dataIQ.layer == 1 || block[1][local[lclX][lclY].block[dataIQ.posX][dataIQ.posY][1]].transparent)) {
										fillLightMap();
										lightTask[lclX][lclY][0] = true;
										lightTask[lclX][lclY][1] = true;
									}
								}else{
									nonsense(830);
								}
								break;
							case 0x09:
								int templateIQ, value;
								memcpy(&templateIQ, ReceivePacket+1, sizeof(int));
								memcpy(&value, ReceivePacket+5, sizeof(int));
								criterionSelf[templateIQ] = value;
								break;
							case 0x0B:
								currentHour = (int)ReceivePacket[1];
								fillLightMap();
								lightTask[0][0][0] = true;
								lightTask[1][0][0] = true;
								lightTask[2][0][0] = true;
								lightTask[0][1][0] = true;
								lightTask[1][1][0] = true;
								lightTask[2][1][0] = true;
								lightTask[0][2][0] = true;
								lightTask[1][2][0] = true;
								lightTask[2][2][0] = true;
								lightTask[0][0][1] = true;
								lightTask[1][0][1] = true;
								lightTask[2][0][1] = true;
								lightTask[0][1][1] = true;
								lightTask[1][1][1] = true;
								lightTask[2][1][1] = true;
								lightTask[0][2][1] = true;
								lightTask[1][2][1] = true;
								lightTask[2][2][1] = true;
								break;
						}
					}
				}
				close(sockSF);
			}else{
				*pConnectButton = false;
				puts("[Out] Failed to establish Connection");
			}
		}
    }
}

int main() {
	printf("\n–––– qDivClient-%d.%c ––––\n\n", QDIV_VERSION, QDIV_BRANCH);
	puts("[Out] Initialising GLFW");
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	char windowVersion[32];
	sprintf(windowVersion, "qDiv-%d.%c", QDIV_VERSION, QDIV_BRANCH);
	window = glfwCreateWindow(715, 715, windowVersion, NULL, NULL);
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, windowCallback);
	glfwSetKeyCallback(window, keyboardCallback);
	glfwSetMouseButtonCallback(window, mouseCallback);
	glfwSetCursorPosCallback(window, cursorCallback);
	glfwSetCharCallback(window, typingCallback);
	gladLoadGL();
	puts("[Out] Preparing Index Data");
	prepareIndexData();
	puts("[Out] Preparing Field Mesh");
	prepareMesh();
	puts("[Out] Preparing Menu Grid");
	prepareGrid();
	puts("[Out] Creating Buffers");
	unsigned int VS = glCreateShader(GL_VERTEX_SHADER);
	unsigned int FS = glCreateShader(GL_FRAGMENT_SHADER);
	unsigned int PS = glCreateProgram();
	const GLchar* vertexFL = (const GLchar*)shaders_vertexshader_glsl;
	const GLchar* fragmentFL = (const GLchar*)shaders_fragmentshader_glsl;
	glShaderSource(VS, 1, &vertexFL, NULL);
	glShaderSource(FS, 1, &fragmentFL, NULL);
	glCompileShader(VS);
	glCompileShader(FS);
	glAttachShader(PS, VS);
	glAttachShader(PS, FS);
	glLinkProgram(PS);
	glGenBuffers(1, &EBO);
	glGenVertexArrays(18, (GLuint*)FVAO);
	glGenBuffers(18, (GLuint*)FVBO);
	int bfrX = 0;
	int bfrY = 0;
	int bfrL = 0;
	while(bfrL < 2) {
		glBindVertexArray(FVAO[bfrX][bfrY][bfrL]);
		glBindBuffer(GL_ARRAY_BUFFER, FVBO[bfrX][bfrY][bfrL]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(fieldMesh) / 18, fieldMesh[bfrX][bfrY][bfrL], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexData), indexData, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)(131072*sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)(262144*sizeof(float)));
		glEnableVertexAttribArray(2);
		bfrX++;
		if(bfrX == 3) {
			bfrX = 0;
			bfrY++;
			if(bfrY == 3) {
				bfrY = 0;
				bfrL++;
			}
		}
	}
	glGenVertexArrays(1, &MVAO);
	glGenBuffers(1, &MVBO);
	glBindVertexArray(MVAO);
	glBindBuffer(GL_ARRAY_BUFFER, MVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(menuGrid), menuGrid, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexData), indexData, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)(3200*sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)(6400*sizeof(float)));
	glEnableVertexAttribArray(2);
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexData), indexData, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)(8*sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)(16*sizeof(float)));
	glEnableVertexAttribArray(2);
	glEnable(GL_BLEND);
	libMain();
	puts("[Out] Loading Textures");
	loadTexture(&logoTex, logo_png, logo_pngSZ);
	loadTexture(&text, text_png, text_pngSZ);
	loadTexture(&interfaceTex, interface_png, interface_pngSZ);
	loadTexture(&selectionTex, selection_png, selection_pngSZ);
	loadTexture(floorTex + 0, floor0_png, floor0_pngSZ);
	loadTexture(floorTex + 1, floor1_png, floor1_pngSZ);
	loadTexture(floorTex + 2, floor2_png, floor2_pngSZ);
	loadTexture(floorTex + 3, floor3_png, floor3_pngSZ);
	loadTexture(&wallTex, wall_png, wall_pngSZ);
	loadTexture(&foundationTex, foundation_png, foundation_pngSZ);
	loadTexture(&dummyTex, dummy_png, dummy_pngSZ);
	loadTexture(&blankTex, blank_png, blank_pngSZ);
	makeArtifacts();
	makeEntityTypes();
	glUseProgram(PS);
	colorUniform = glGetUniformLocation(PS, "color");
	matrixUniform = glGetUniformLocation(PS, "matrix");
	samplerUni = glGetUniformLocation(PS, "sampler");
	uniformDefaults();
	glUniform1i(samplerUni, 0);
	glClearColor(0.f, 0.f, 0.f, 0.f);
    pthread_t gate_id, usage_id;
    pthread_create(&gate_id, NULL, thread_gate, NULL);
    pthread_create(&usage_id, NULL, thread_usage, NULL);
    entityDF.slot = 0;
    entityDF.type = PLAYER;
    entityDF.active = 1;
    entityDF.zone = 0;
    entityDF.fldX = 0;
    entityDF.fldY = 0;
    entityDF.posX = 0;
    entityDF.posY = 0;
    entityDF.motX = 0;
    entityDF.motY = 0;
    entityDF.qEnergy = 0;
    entityDF.health = entityType[entityDF.type].maxHealth;
    strcpy(entityDF.name, "entityDF");
	playerData* playerDF = &entityDF.unique.Player;
	playerDF -> artifact = 0;
	playerDF -> criterion = criterionSelf;
    currentMenu = START_MENU;
    FILE* settingsFile = fopen("data/settings.dat", "rb");
    if(settingsFile == NULL) {
    	settingsFile = fopen("settings.dat", "rb");
    	if(settingsFile == NULL) {
    		memset(settings.name, 0x00, sizeof(settings.name));
			settings.nameCursor = 0;
			settings.vsync = true;
			glfwSwapInterval(1);
			goto settingsNull;
    	}
    }
	fread(&settings, 1, sizeof(settings), settingsFile);
	fclose(settingsFile);
	settings.vsync ? glfwSwapInterval(1) : glfwSwapInterval(0);
	settingsNull:
    artifactMenu.role = BUILDER;
    artifactMenu.page = 0;
    char textIQ[32];
	puts("[Out] Graphical Environment running");
	while(!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);
		glUniform2fv(windowUniform, 1, (vec2){(float)windowWidth, (float)windowHeight});
		gettimeofday(&qFrameEnd, NULL);
		if(qFrameStart.tv_usec > qFrameEnd.tv_usec) qFrameEnd.tv_usec += 1000000;
		qFactor = (double)(qFrameEnd.tv_usec - qFrameStart.tv_usec) * 0.000001;
		gettimeofday(&qFrameStart, NULL);
		qTime = (double)qFrameStart.tv_sec + (double)qFrameStart.tv_usec * 0.000001;
		switch(currentMenu) {
			case START_MENU:
				glBindVertexArray(VAO);
				glBindTexture(GL_TEXTURE_2D, logoTex);
				setScalePos(0.75, 0.75, -0.375, 0.375);
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
				uniformDefaults();
				if(menuButton(0.1f, -0.4f, 0.15f, 8, "Play", NULL, 4)) currentMenu = SERVER_MENU;
				if(menuButton(0.1f, -0.4f, 0.f, 8, "Settings", NULL, 8)) currentMenu = SETTINGS_MENU;
				if(menuButton(0.1f, -0.4f, -0.15f, 8, "About qDiv", NULL, 10));
				renderText("Copyright 2023\nGabriel F. Hodges", 32,-0.98f, -0.9f, 0.05, TEXT_LEFT);
				break;
			case SERVER_MENU:
				int serverSL = 0;
				float posY = 0.8f;
				while(serverSL < 16) {
					posY -= 0.1f;
					if(menuButton(0.08f, -0.4f, posY, 10, settings.server[serverSL], settings.serverCursor + serverSL, sizeof(settings.server[serverSL]))) {
						memcpy(currentServer, settings.server[serverSL], sizeof(currentServer));
						ConnectButton = true;
					}
					serverSL++;
				}
				if(Keyboard == GLFW_KEY_ESCAPE) {
					Keyboard = 0x00;
					currentMenu = START_MENU;
				}
				break;
			case SETTINGS_MENU:
				char* vsyncOPT = settings.vsync ? "Vsync On" : "Vsync Off";
				char* cursorOPT = settings.cursorsync ? "Cursor: Synced" : "Cursor: Instant";
				menuButton(0.1f, -0.4f, 0.3f, 8, settings.name, &settings.nameCursor, sizeof(settings.name));
				if(menuButton(0.1f, -0.4f, 0.15f, 8, vsyncOPT, NULL, strlen(vsyncOPT))) {
					if(settings.vsync) {
						glfwSwapInterval(0);
						settings.vsync = false;
					}else{
						glfwSwapInterval(1);
						settings.vsync = true;
					}
				}
				if(menuButton(0.1f, -0.4f, 0.f, 8, cursorOPT, NULL, strlen(cursorOPT))) settings.cursorsync = settings.cursorsync ? false : true;
				if(Keyboard == GLFW_KEY_ESCAPE) {
					Keyboard = 0x00;
					currentMenu = START_MENU;
				}
				break;
			case INGAME_MENU:
				glm_translate2d_to(matrix, (vec2){-qBlock * entitySelf -> posX + 2.0, -qBlock * entitySelf -> posY + 2.0}, motion);
				TRANSFORM;
				glBindTexture(GL_TEXTURE_2D, floorTex[qFrameStart.tv_usec / 250000]);
				renderField(1, 1, 0);
				if(entitySelf -> posX < 64) {
					renderField(0, 1, 0);
					if(entitySelf -> posY < 64) {
						renderField(1, 0, 0);
						renderField(0, 0, 0);
					}else{
						renderField(1, 2, 0);
						renderField(0, 2, 0);
					}
				}else{
					renderField(2, 1, 0);
					if(entitySelf -> posY < 64) {
						renderField(1, 0, 0);
						renderField(2, 0, 0);
					}else{
						renderField(1, 2, 0);
						renderField(2, 2, 0);
					}
				}
				glBindTexture(GL_TEXTURE_2D, wallTex);
				renderField(1, 1, 1);
				if(entitySelf -> posX < 64) {
					renderField(0, 1, 1);
					if(entitySelf -> posY < 64) {
						renderField(1, 0, 1);
						renderField(0, 0, 1);
					}else{
						renderField(1, 2, 1);
						renderField(0, 2, 1);
					}
				}else{
					renderField(2, 1, 1);
					if(entitySelf -> posY < 64) {
						renderField(1, 0, 1);
						renderField(2, 0, 1);
					}else{
						renderField(1, 2, 1);
						renderField(2, 2, 1);
					}
				}
				glBindVertexArray(VAO);
				glBindBuffer(GL_ARRAY_BUFFER, VBO);
				renderActions();
				renderEntities();
				renderBlockSelection();
				uniformDefaults();
				glBindVertexArray(MVAO);
				glBindTexture(GL_TEXTURE_2D, interfaceTex);
				glDrawElements(GL_TRIANGLES, 120, GL_UNSIGNED_INT, 0);
				glDrawElements(GL_TRIANGLES, 120, GL_UNSIGNED_INT, (const GLvoid*)9120);
				glBindVertexArray(VAO);
				sprintf(textIQ, "%llu", entitySelf -> qEnergy);
				renderText(textIQ, strlen(textIQ), -0.975, -0.975, 0.06, TEXT_LEFT);
				sprintf(textIQ, "%s", entitySelf -> name);
				renderText(textIQ, strlen(textIQ), -0.975, 0.925, 0.06, TEXT_LEFT);
				sprintf(textIQ, "%d", (int)(1 / qFactor));
				renderText(textIQ, strlen(textIQ), 0.5, 0.5, 0.05, TEXT_LEFT);
				renderSelectedArtifact();
				uniformDefaults();
				break;
			case ARTIFACT_MENU:
				int hoveredArtifact = screenToArtifact();
				glBindVertexArray(MVAO);
				glBindTexture(GL_TEXTURE_2D, interfaceTex);
				glDrawElements(GL_TRIANGLES, 2400, GL_UNSIGNED_INT, 0);
				glBindVertexArray(VAO);
				sprintf(textIQ, "%llu", entitySelf -> qEnergy);
				renderText(textIQ, strlen(textIQ), -0.975, -0.975, 0.06, TEXT_LEFT);
				sprintf(textIQ, "%s", role[artifactMenu.role].name);
				setColor(role[artifactMenu.role].textColor.red, role[artifactMenu.role].textColor.green, role[artifactMenu.role].textColor.blue, 1.f);
				renderText(textIQ, strlen(textIQ), -0.975, 0.925, 0.06, TEXT_LEFT);
				sprintf(textIQ, "Page: %d/%d", artifactMenu.page + 1, role[artifactMenu.role].maxArtifact / 360 + 1);
				renderText(textIQ, strlen(textIQ), 0.975, -0.975, 0.06, TEXT_RIGHT);
				if(hoveredArtifact > -1 && hoveredArtifact < role[artifactMenu.role].maxArtifact && hoverCheck(-1.f, -0.9f, 1.f, 0.9f)) {
					float posX = (float)(floor(cursorX * 10) * 0.1f);
					float posY = (float)(floor(cursorY * 10) * 0.1f);
					setAtlasArea(0.f, 0.25f, 0.25f);
					setScalePos(0.1f, 0.1f, posX, posY);
					glBindTexture(GL_TEXTURE_2D, interfaceTex);
					glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
					uniformDefaults();
					if(LeftClick && isArtifactUnlocked(artifactMenu.role, hoveredArtifact, entitySelf)) {
						LeftClick = false;
						makeArtifactRequest(artifactMenu.role, hoveredArtifact);
						send(*sockPT, SendPacket, QDIV_PACKET_SIZE, 0);
					}
					if(RightClick) {
						currentMenu = ARTIFACT_INFO;
					}
				}
				setAtlasArea(0.f, 0.f, 1.f);
				int artifactSL = 360 * artifactMenu.page;
				float slotX = -0.975f;
				float slotY = 0.825f;
				while(artifactSL < role[artifactMenu.role].maxArtifact && slotY > -0.9f) {
					setScalePos(0.05f, 0.05f, slotX, slotY);
					glBindTexture(GL_TEXTURE_2D, artifact[artifactMenu.role][artifactSL++].texture);
					glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
					slotX += 0.1f;
					if(slotX >= 1.f) {
						slotX = -0.975f;
						slotY -= 0.1f;
					}
				}
				uniformDefaults();
				switch(Keyboard) {
					case GLFW_KEY_ESCAPE:
						Keyboard = 0x00;
						currentMenu = INGAME_MENU;
						break;
					case GLFW_KEY_UP:
						artifactMenu.role--;
						artifactMenu.page = 0;
						if(artifactMenu.role < WARRIOR) artifactMenu.role = WIZARD;
						Keyboard = 0x00;
						break;
					case GLFW_KEY_RIGHT:
						artifactMenu.page++;
						if(artifactMenu.page > role[artifactMenu.role].maxArtifact / 360) artifactMenu.page = 0;
						Keyboard = 0x00;
						break;
					case GLFW_KEY_DOWN:
						artifactMenu.role++;
						artifactMenu.page = 0;
						if(artifactMenu.role > WIZARD) artifactMenu.role = WARRIOR;
						Keyboard = 0x00;
						break;
					case GLFW_KEY_LEFT:
						artifactMenu.page--;
						if(artifactMenu.page < 0) artifactMenu.page = role[artifactMenu.role].maxArtifact / 360;
						Keyboard = 0x00;
						break;
				}
				break;
			case ARTIFACT_INFO:
				char desc[256];
				glBindVertexArray(VAO);
				artifactSettings* artifactIQ = &artifact[artifactMenu.role][hoveredArtifact];
				sprintf(textIQ, "%s", artifactIQ -> name);
				renderText(textIQ, strlen(textIQ), -0.95, 0.85, 0.1, TEXT_LEFT);
				sprintf(desc, "%s", artifactIQ -> desc);
				renderText(desc, strlen(desc), -0.95, 0.7, 0.05, TEXT_LEFT);
				int templateSL, criterionSL;
				if(artifactIQ -> qEnergy > 0) {
					criterionSL = 1;
					sprintf(textIQ, "%llu/%llu qEnergy", entitySelf -> qEnergy, artifactIQ -> qEnergy);
					renderText(textIQ, strlen(textIQ), 0.85, -0.95, 0.05, TEXT_RIGHT);
				}else{
					criterionSL = 0;
				}
				for(int criterionSL = 0; criterionSL < 4; criterionSL++) {
					templateSL = artifactIQ -> criterion[criterionSL].Template;
					if(templateSL != NO_CRITERION) {
						sprintf(textIQ, "%d/%d %s", criterionSelf[templateSL], artifactIQ -> criterion[criterionSL].value, criterionTemplate[templateSL].desc);
						renderText(textIQ, strlen(textIQ), 0.85, -0.95 + (double)criterionSL * 0.1, 0.05, TEXT_RIGHT);
					}
				}
				glBindTexture(GL_TEXTURE_2D, artifactIQ -> texture);
				setScalePos(0.375, 0.375, -0.875, -0.875);
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
				if(Keyboard == GLFW_KEY_ESCAPE) {
					Keyboard = 0x00;
					currentMenu = ARTIFACT_MENU;
				}
				break;
		}
		if(connection == CONNECTED_NET) {
			for(int entitySL = 0; entitySL < 10000; entitySL++) {
				if(entitySlot[entitySL] == 1) {
					entity[entitySL].posX += entity[entitySL].motX * qFactor;
					entity[entitySL].posY += entity[entitySL].motY * qFactor;
					if(entity[entitySL].type == PLAYER) {
						entityData* entityIQ = entity + entitySL;
						playerData* playerIQ = &entityIQ -> unique.Player;
						if(playerIQ -> currentUsage == NO_USAGE) {
							playerIQ -> useTimer = 0;
						}else{
							playerIQ -> useTimer += 1 * qFactor;
						}
					}
					entitySlot[entitySL] = entityInRange(entity + entitySL, entitySelf);
				}
			}
		}
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	settingsFile = fopen("data/settings.dat", "wb");
	if(settingsFile == NULL) settingsFile = fopen("settings.dat", "wb");
	fwrite(&settings, 1, sizeof(settings), settingsFile);
	fclose(settingsFile);
	connection = OFFLINE_NET;
	qDivRun = false;
	pthread_join(usage_id, NULL);
	pthread_join(gate_id, NULL);
	glfwTerminate();
	return 0;
}
