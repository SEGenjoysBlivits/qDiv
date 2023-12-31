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
#define QDIV_MKDIR(folder) mkdir(folder)
#define _GLFW_WIN32
#else
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<unistd.h>
#include<signal.h>
#define QDIV_MKDIR(folder) mkdir(folder, S_IRWXU);
#define _GLFW_X11
#endif
#define STB_IMAGE_IMPLEMENTATION
#define MINIAUDIO_IMPLEMENTATION
#include<stdio.h>
#include<pthread.h>
#include<time.h>
#include "include/glad/glad.h"
#include<GLFW/glfw3.h>
#include</usr/include/cglm/cglm.h>
#include<stb/stb_image.h>
#include "include/miniaudio.h"
#include "qDivLib.h"
#include "qDivClientElements.h" // Only briefly exists when running compile-client.sh
#define QDIV_AUDIO_DECODERS 64
#define QDIV_AUDIO_FORMAT ma_format_f32
#define QDIV_AUDIO_CHANNELS 1
#define QDIV_AUDIO_RATE 48000
#define QDIV_DRAW(count, offset) {\
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (const GLvoid*)(offset));\
	drawCalls++;\
}
#define QDIV_MATRIX_UPDATE() glUniformMatrix3fv(matrixUniform, 1, GL_FALSE, (const GLfloat*)&motion)
#define QDIV_MATRIX_RESET() glUniformMatrix3fv(matrixUniform, 1, GL_FALSE, (const GLfloat*)&matrix)
#define QDIV_COLOR_UPDATE(red, green, blue, alpha) glUniform4fv(colorUniform, 1, (vec4){red, green, blue, alpha})
#define QDIV_COLOR_RESET() glUniform4fv(colorUniform, 1, (vec4){1.0, 1.0, 1.0, 1.0})

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

typedef void(*artifactRenderer)(int32_t, int32_t, double, double, entityData*, playerData*, void*);

typedef struct {
	int8_t name[16];
	int8_t desc[256];
	bool crossCriterial;
	uint32_t texture;
	artifactRenderer primary;
	double primaryUseTime;
	artifactRenderer secondary;
	double secondaryUseTime;
	uint64_t qEnergy;
	struct {
		int32_t Template;
		uint32_t value;
	} criterion[QDIV_ARTIFACT_CRITERIA];
} artifactSettings;

const float qBlock = 0.03125f;

float vertexData[] = {
	0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Pos
	0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Tex
	1.0f, 1.0f, 1.0f, 1.0f // Lgt
};
uint32_t indexData[98304];
float menuGrid[8000];
float fieldMesh[3][3][2][327680];
int32_t drawCalls = 0;

// Graphics
GLFWwindow* window;
int32_t windowWidth = 715;
int32_t windowHeight = 715;
int32_t windowSquare = 715;
GLuint VBO, VAO;
GLuint MVBO, MVAO;
GLuint AVBO, AVAO;
GLuint FVBO[3][3][2], FVAO[3][3][2];
GLuint EBO;
uint32_t logoTex;
uint32_t text;
uint32_t interfaceTex;
uint32_t dummyTex;
uint32_t foundationTex;
uint32_t floorTex[4];
uint32_t wallTex;
uint32_t artifactTex[6];
uint32_t selectionTex;
uint32_t blankTex;
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

// Audio
ma_decoder_config audioDecoderConfig;
ma_decoder audioDecoder[QDIV_AUDIO_DECODERS];
ma_device_config audioDeviceConfig;
ma_device audioDevice;
bool audioTask[QDIV_AUDIO_DECODERS] = {false};

struct timeval qFrameStart;
struct timeval qFrameEnd;
double qTime;
int32_t currentMenu;
int32_t* pMenu = &currentMenu;
struct {
	int32_t role;
	int32_t page;
} artifactMenu;

struct {
	int8_t name[16];
	int32_t nameCursor;
	int8_t server[16][39];
	int32_t serverCursor[16];
	bool vsync;
	bool cursorsync;
	bool verboseDebug;
} settings;

int8_t  currentServer[39];
uint8_t uuid[16];

bool LeftClick;
bool RightClick;
volatile int32_t Keyboard = 0x00;
volatile int32_t* pKeyboard = &Keyboard;
int8_t  utf8_Text = 0x00;
int8_t* putf8_Text = &utf8_Text;
uint32_t promptCursor = 0;

bool ConnectButton = false;
bool* pConnectButton = &ConnectButton;

int32_t Connection = OFFLINE_NET;
int32_t* pConnection = &Connection;
int32_t* sockPT;

double cursorX;
double cursorY;
double* pcursorX = &cursorX;
double* pcursorY = &cursorY;
double syncedCursorX;
double syncedCursorY;
double* psyncedCursorX = &syncedCursorX;
double* psyncedCursorY = &syncedCursorY;
fieldData local[3][3];
int32_t lightMap[384][384];
struct {
	int32_t zone;
	int32_t lclX;
	int32_t lclY;
	int32_t posX;
} currentSegment;


entityData entityDF;
entityData* entitySelf = &entityDF;
uint32_t criterionSelf[MAX_CRITERION];

bool meshTask[3][3][2] = {false};
bool lightTask[3][3][2] = {false};
bool blockTask[3][3][128][128][2] = {false};
bool shutterTask[3][2][384] = {false};

artifactSettings artifact[6][4000];

double doubMin(double a, double b) {
	return a < b ? a : b;
}

// Packet
void makeDirectionPacket(int32_t direction) {
	SendPacket[4] = 0x02;
	memcpy(SendPacket+5, &direction, sizeof(int32_t));
}

// Packet
void makeArtifactRequest(int32_t role, int32_t artifact) {
	SendPacket[4] = 0x08;
	memcpy(SendPacket+5, &role, sizeof(int32_t));
	memcpy(SendPacket+9, &artifact, sizeof(int32_t));
}

// Packet
void makeUsagePacket(int32_t usage, double inRelX, double inRelY) {
	SendPacket[4] = 0x0A;
	usageData usageIQ;
	usageIQ.usage = usage;
	usageIQ.useRelX = inRelX;
	usageIQ.useRelY = inRelY;
	memcpy(SendPacket+5, &usageIQ, sizeof(usageData));
}

bool hoverCheck(double minX, double minY, double maxX, double maxY) {
	return cursorX < maxX && cursorY < maxY && cursorX > minX && cursorY > minY;
}

void screenToBlock(double inX, double inY, int32_t* outX, int32_t* outY, int32_t* lclX, int32_t* lclY) {
	*lclX = 1;
	*lclY = 1;
	*outX = (int32_t)floor(inX * 32.0 + entitySelf -> posX);
	if(*outX < 0) {
		*outX += 128;
		*lclX = 0;
	}else if(*outX >= 128) {
		*outX -= 128;
		*lclX = 2;
	}
	*outY = (int32_t)floor(inY * 32.0 + entitySelf -> posY);
	if(*outY < 0) {
		*outY += 128;
		*lclY = 0;
	}else if(*outY >= 128) {
		*outY -= 128;
		*lclY = 2;
	}
}

// Callback
void callback_window(GLFWwindow* window, int32_t width, int32_t height) {
	int32_t higher = width < height;
	windowSquare = higher ? width : height;
	glViewport(!higher * ((width / 2) - windowSquare / 2), higher * ((height / 2) - windowSquare / 2), windowSquare, windowSquare);
	windowWidth = width;
	windowHeight = height;
}

// Callback
void callback_mouse(GLFWwindow* window, int32_t button, int32_t action, int32_t mods) {
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
void callback_keyboard(GLFWwindow* window, int32_t key, int32_t scancode, int32_t action, int32_t mods) {
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
void callback_typing(GLFWwindow* window, uint32_t codepoint32_t) {
	utf8_Text = codepoint32_t;
}

// Callback
void callback_audio(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
	#define QDIV_AUDIO_BUFFER_SIZE 4096
	int32_t decoderSL = 0;
	float* outputFL = (float*)pOutput;
	float outputTMP[QDIV_AUDIO_BUFFER_SIZE];
	while(decoderSL < QDIV_AUDIO_DECODERS) {
		if(audioTask[decoderSL]) {
			memset(outputTMP, 0x00, sizeof(outputTMP));
			ma_uint64 frameRD;
			ma_uint32 frameSL;
			ma_uint32 frameDC = 0;
			while(frameDC < frameCount) {
				ma_uint32 frameTR = clampInt(0, frameCount - frameDC, ma_countof(outputTMP));
				if(ma_decoder_read_pcm_frames(audioDecoder + decoderSL, outputTMP, frameTR, &frameRD) != MA_SUCCESS || frameRD == 0) break;
				frameSL = 0;
				while(frameSL < frameRD) {
					outputFL[frameDC + frameSL] += outputTMP[frameSL];
					frameSL++;
				}
				frameDC += frameRD;
			}
			if(frameDC < frameCount) audioTask[decoderSL] = false;
    	}
    	decoderSL++;
    }
}

void playSound(const int8_t * file) {
	int32_t decoderSL = 0;
	while(decoderSL < QDIV_AUDIO_DECODERS) {
		if(!audioTask[decoderSL]) {
			ma_decoder_init_file(file, &audioDecoderConfig, audioDecoder + decoderSL);
			audioTask[decoderSL] = true;
			break;
		}
		decoderSL++;
	}
}

void setScalePos(float scaleX, float scaleY, float offsetX, float offsetY) {
	glm_scale2d_to(matrix, (vec2){scaleX, scaleY}, motion);
	glm_translate2d(motion, (vec2){offsetX / scaleX, offsetY / scaleY});
	QDIV_MATRIX_UPDATE();
}

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

void prepareMesh() {
	int32_t bfrX = 0;
	int32_t bfrY = 0;
	while(bfrY < 3) {
		int32_t vertexSL = 0;
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
	int32_t vertexSL = 0;
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
		for(int32_t vertexSL = 16; vertexSL < 20; vertexSL++) vertexData[vertexSL] = 1.0f;
		glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertexData) / 5 * 2, sizeof(vertexData) / 5 * 2, &vertexData[8]);
	}
}

void loadTexture(uint32_t *texture, const uint8_t  *file, size_t fileSZ) {
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

bool isArtifactUnlocked(int32_t roleSL, int32_t artifactSL, entityData* entityIQ) {
	if(entityIQ -> qEnergy < artifact[roleSL][artifactSL].qEnergy) return false;
	for(int32_t criterionSL = 0; criterionSL < QDIV_ARTIFACT_CRITERIA; criterionSL++) {
		int32_t TemplateSL = artifact[roleSL][artifactSL].criterion[criterionSL].Template;
		if(TemplateSL != NO_CRITERION && artifact[roleSL][artifactSL].criterion[criterionSL].value > entityIQ -> unique.Player.criterion[TemplateSL]) return false;
	}
	return true;
}

// Action Renderer
void simpleSwing(int32_t lclX, int32_t lclY, double posX, double posY, entityData* entityIQ, playerData* playerIQ, void* restrict artifactVD) {
	artifactSettings* restrict artifactIQ = (artifactSettings* restrict)artifactVD;
	glm_scale2d_to(matrix, (vec2){qBlock * 2, qBlock * 2}, motion);
	glm_translate2d(motion, (vec2){(entityIQ -> posX - entitySelf -> posX) * 0.5, (entityIQ -> posY - entitySelf -> posY) * 0.5});
	glm_rotate2d(motion, playerIQ -> useTimer * 6.28 / artifactIQ -> primaryUseTime);
	QDIV_MATRIX_UPDATE();
	glBindTexture(GL_TEXTURE_2D, artifactIQ -> texture);
	QDIV_DRAW(6, 0);
}

// Entity Renderer
void minimum(void* entityVD) {
	entityData* entityIQ = (entityData*)entityVD;
	double boxIQ = entityType[entityIQ -> type].hitBox;
	double scaleIQ = qBlock * boxIQ;
	glm_scale2d_to(matrix, (vec2){scaleIQ, scaleIQ}, motion);
	glm_translate2d(motion, (vec2){(entityIQ -> posX - entitySelf -> posX + (entityIQ -> fldX - entitySelf -> fldX) * 128.0 - 0.5) / boxIQ, (entityIQ -> posY - entitySelf -> posY + (entityIQ -> fldY - entitySelf -> fldY) * 128.0 - 0.5) / boxIQ});
	QDIV_MATRIX_UPDATE();
	glBindTexture(GL_TEXTURE_2D, dummyTex);
	QDIV_DRAW(6, 0);
}

void makeEntityTypes() {
	entityType[NULL_ENTITY] = makeEntityType(0, true, 0, false, 0.0, 1, NULL);
	entityType[PLAYER] = makeEntityType(5, true, 2, false, 32.0, 1, &minimum);
	entityType[SHALLAND_SNAIL] = makeEntityType(5, true, 4, false, 0.25, 1, &minimum);
}

artifactSettings makeArtifact(int8_t * inName, int8_t * inDesc, const uint8_t * int32_texture, size_t int32_textureSZ, bool inCross, uint64_t inEnergy, int32_t Template1, uint32_t value1, int32_t Template2, uint32_t value2, int32_t Template3, uint32_t value3, int32_t Template4, uint32_t value4, artifactRenderer inPrimary, double inPrimaryTime, artifactRenderer inSecondary, double inSecondaryTime) {
	artifactSettings artifactIQ;
	strcpy(artifactIQ.name, inName);
	strcpy(artifactIQ.desc, inDesc);
	artifactIQ.crossCriterial = inCross;
	loadTexture(&artifactIQ.texture, int32_texture, int32_textureSZ);
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

void renderText(int8_t * restrict toRender, size_t length, float posX, float posY, float chSC, int32_t typeIQ) {
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
	QDIV_MATRIX_UPDATE();
	bool newLine = false;
	bool whiteSpace = false;
	bool chHG = false;
	double currentPosition = 0;
	for(int32_t chSL = 0; chSL < (int32_t)length; chSL++) {
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
		QDIV_MATRIX_UPDATE();
		if(newLine) {
			glm_translate2d(motion, (vec2){-currentPosition, -1.5});
			newLine = 0;
			currentPosition = 0;
		}else{
			if(!whiteSpace) {
				QDIV_DRAW(6, 0);
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
	QDIV_MATRIX_RESET();
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
	QDIV_DRAW(6, 0);
	glBindTexture(GL_TEXTURE_2D, artifact[entitySelf -> unique.Player.role][entitySelf -> unique.Player.artifact].texture);
	setAtlasArea(0.f, 0.f, 1.f);
	setScalePos(scale * 0.5f, scale * 0.5f, posX + 0.025f, posY + 0.025f);
	QDIV_DRAW(6, 0);
}

bool menuButton(float scale, float posX, float posY, int32_t length, int8_t * restrict nestedText, int32_t* restrict nestedCursor, size_t nestedLength) {
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
			//playSound("sound.flac");
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
	QDIV_DRAW(6, 0);
	setAtlasArea(0.5f, texY, 0.25f);
	for(int32_t posSL = 1; posSL < length; posSL++) {
		if(posSL == length - 1) setAtlasArea(0.75f, texY, 0.25f);
		posX += scale;
		setScalePos(scale, scale, posX, posY);
		QDIV_DRAW(6, 0);
	}
	QDIV_COLOR_UPDATE(red, green, blue, 1.f);
	if(nestedCursor == NULL) {
		renderText(nestedText, nestedLength, posX * scale, posY + scale * 0.3f, scale * 0.5f, TEXT_CENTER);
	}else{
		if(red == 0.52f) {
			glBindTexture(GL_TEXTURE_2D, blankTex);
			setScalePos(scale * 0.05f, scale * 0.5f, scale * (posX - 0.5f * (float)length) + (float)(*nestedCursor + 1) * scale * 0.33f, posY + scale * 0.3f);
			QDIV_DRAW(6, 0);
		}
		renderText(nestedText, nestedLength, scale * (posX - 0.5f * (float)length), posY + scale * 0.3f, scale * 0.5f, TEXT_LEFT);
	}
	QDIV_COLOR_RESET();
	return clicked;
}

int32_t screenToArtifact() {
	return (int32_t)((cursorX + 1) * 10) + ((int32_t)((-cursorY + 0.9) * 10)) * 20 + artifactMenu.page * 360;
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
	int8_t  energy[32];
	int32_t blockX, blockY, lclX, lclY;
	screenToBlock(settings.cursorsync ? syncedCursorX : cursorX, settings.cursorsync ? syncedCursorY : cursorY, &blockX, &blockY, &lclX, &lclY);
	int32_t* blockPos = local[lclX][lclY].block[blockX][blockY];
	int32_t layerSL = getOccupiedLayer(blockPos);
	if(layerSL == -1) {
		strcpy(energy, "0");
	}else{
		blockType* blockIQ = &block[layerSL][blockPos[layerSL]];
		sprintf(energy, "%llu", blockIQ -> qEnergy);
		if(qEnergyRelevance(entitySelf -> qEnergy, blockIQ)) {
			if(blockIQ -> qEnergyStatic) {
				QDIV_COLOR_UPDATE(sin(qTime * 6.28) * 0.5f + 0.5f, sin(qTime * 6.28) * 0.5f + 0.5f, 1.f, 1.f);
			}else{
				QDIV_COLOR_UPDATE(sin(qTime * 6.28) * 0.5f + 0.5f, 1.f, sin(qTime * 6.28) * 0.5f + 0.5f, 1.f);
			}
		}else{
			QDIV_COLOR_UPDATE(1.f, sin(qTime * 3.14) * 0.5f + 0.5f, sin(qTime * 3.14) * 0.5f + 0.5f, 1.f);
		}
	}
	setScalePos(qBlock, qBlock, selectX, selectY);
	glBindTexture(GL_TEXTURE_2D, selectionTex);
	QDIV_DRAW(6, 0);
	renderText(energy, strlen(energy), selectX + qBlock * 1.5, selectY, qBlock * 1.5, TEXT_LEFT);
}

void illuminate(int32_t illuminance, int32_t blockX, int32_t blockY) {
	int32_t posX, posY, relPosX, relPosY, calcIllu;
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
	int32_t blockX = 0;
	int32_t blockY = 0;
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
void renderField(int32_t lclX, int32_t lclY, int32_t layerIQ) {
	fieldData* fieldIQ = &local[lclX][lclY];
	int32_t vertexSL = 131072;
	int32_t blockX = 0;
	int32_t blockY = 0;
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
					int32_t blockN = local[(blockX) / 128][(blockY + 1) / 128].block[(blockX) % 128][(blockY + 1) % 128][1];
					int32_t blockE = local[(blockX + 1) / 128][(blockY) / 128].block[(blockX + 1) % 128][(blockY) % 128][1];
					int32_t blockS = local[(blockX) / 128][(blockY - 1) / 128].block[(blockX) % 128][(blockY - 1) % 128][1];
					int32_t blockW = local[(blockX - 1) / 128][(blockY) / 128].block[(blockX - 1) % 128][(blockY) % 128][1];
					int32_t blockNE = local[(blockX + 1) / 128][(blockY + 1) / 128].block[(blockX + 1) % 128][(blockY + 1) % 128][1];
					int32_t blockNW = local[(blockX - 1) / 128][(blockY + 1) / 128].block[(blockX - 1) % 128][(blockY + 1) % 128][1];
					int32_t blockSE = local[(blockX + 1) / 128][(blockY - 1) / 128].block[(blockX + 1) % 128][(blockY - 1) % 128][1];
					int32_t blockSW = local[(blockX - 1) / 128][(blockY - 1) / 128].block[(blockX - 1) % 128][(blockY - 1) % 128][1];
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
	#ifdef NEW_RENDERER
	int32_t offsetRN, factorRN;
	switch(lclY) {
		case 0:
			offsetRN = 96;
			factorRN = 32;
			break;
		case 1:
			offsetRN = clampint32_t(0, (int32_t)(entitySelf -> posY - 32), 96);
			factorRN = 64;
			break;
		case 2:
			offsetRN = 0;
			factorRN = 32;
			break;
		default:
			nonsense(__LINE__);
	}
	QDIV_DRAW(768 * factorRN, 3072 * offsetRN);
	#else
	switch(lclY) {
		case 0:
			QDIV_DRAW(24576, 294912);
			break;
		case 1:
			QDIV_DRAW(98304, 0);
			break;
		case 2:
			QDIV_DRAW(24576, 0);
			break;
	}
	#endif
}

// Grand Manager
void renderEntities() {
	entityData* entityIQ;
	entitySettings typeIQ;
	for(int32_t entitySL = 0; entitySL < 10000; entitySL++) {
		if(entityTable[entitySL]) {
			entityIQ = entity + entitySL;
			if(entityType[entityIQ -> type].action != NULL) {
				(*entityType[entityIQ -> type].action)((void*)entityIQ);
			}else{
				nonsense(__LINE__);
			}
		}
	}
}

// Grand Manager
void renderActions() {
	double posX, posY;
	int32_t lclX, lclY, layerSL;
	FOR_EVERY_ENTITY {
		if(entityTable[entitySL] && entity[entitySL].type == PLAYER) {
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
	WSAStartup(MAKEWORD(2, 2), &wsa);
	#else
	signal(SIGPIPE, SIG_IGN);
	#endif
	struct sockaddr_in6 server;
	struct timeval qTimeOut;
	int32_t sockSF = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	fd_set sockRD;
	qTimeOut.tv_usec = 0;
	sockPT = &sockSF;
	memset(&server, 0x00, sizeof(server));
	server.sin6_family = AF_INET6;
    server.sin6_port = htons(QDIV_PORT);
    while(*pRun) {
    	usleep(1000);
    	if(*pConnectButton) {
			puts("[Out] Connecting to specified server");
			if(inet_pton(AF_INET6, currentServer, &server.sin6_addr) > 0 && connect(sockSF, (struct sockaddr*)&server, sizeof(server)) >= 0) {
				*pConnection = WAITING_NET;
				*pMenu = CONNECTING_MENU;
				#ifdef QDIV_AUTH
				int8_t fileName[34];
				sprintf(fileName, "player/identity/%s.qid", settings.name);
				FILE* identityFile = fopen(fileName, "rb");
				if(identityFile == NULL) {
					QDIV_RANDOM(uuid, 16);
					identityFile = fopen(fileName, "wb");
					fwrite(uuid, 1, 16, identityFile);
				}else{
					fread(uuid, 1, 16, identityFile);
				}
				fclose(identityFile);
				uint8_t privateKey[ECC_PRV_KEY_SIZE];
				uint8_t internalKey[ECC_PUB_KEY_SIZE];
				uint8_t externalKey[ECC_PUB_KEY_SIZE];
				uint8_t sharedKey[ECC_PRV_KEY_SIZE];
				QDIV_RANDOM(privateKey, ECC_PRV_KEY_SIZE);
				ecdh_generate_keys(internalKey, privateKey);
				SendPacket[4] = 0x0C;
				memcpy(SendPacket+5, internalKey, ECC_PUB_KEY_SIZE);
				send(sockSF, SendPacket, QDIV_PACKET_SIZE, 0);
				FD_ZERO(&sockRD);
				FD_SET(sockSF, &sockRD);
				qTimeOut.tv_sec = 5;
				if(select(sockSF+1, &sockRD, NULL, NULL, &qTimeOut) <= 0) goto disconnect;
				memset(ReceivePacket, 0x00, QDIV_PACKET_SIZE);
				recv(sockSF, ReceivePacket, QDIV_PACKET_SIZE, 0);
				if(ReceivePacket[4] != 0x0C) goto disconnect;
				memcpy(externalKey, ReceivePacket+5, ECC_PUB_KEY_SIZE);
				ecdh_shared_secret(privateKey, externalKey, sharedKey);
				//printf("%s\n", sharedKey);
				SendPacket[4] = 0x0D;
				/*for(int32_t byteSL = 0; byteSL < 16; byteSL++) printf("%x", identitySelf.uuid[byteSL]);
				printf("\n");*/
				//printf("Decrypted: %s\n", uuid);
				for(int32_t cryptSL = 0; cryptSL < 16; cryptSL++) uuid[cryptSL % 16] ^= sharedKey[cryptSL];
				//printf("Encrypted: %s\n", uuid);
				memcpy(SendPacket+5, settings.name, 16);
				memcpy(SendPacket+21, uuid, 16);
				send(sockSF, SendPacket, QDIV_PACKET_SIZE, 0);
				currentSegment.posX = 128;
				#endif
				while(*pConnection != OFFLINE_NET) {
					FD_ZERO(&sockRD);
					FD_SET(sockSF, &sockRD);
					qTimeOut.tv_sec = 1;
        			entityData entityIQ;
        			if(select(sockSF+1, &sockRD, NULL, NULL, &qTimeOut) > 0) {
						memset(ReceivePacket, 0x00, QDIV_PACKET_SIZE);
						recv(sockSF, ReceivePacket, QDIV_PACKET_SIZE, 0);
						if(memcmp(ReceivePacket, SendPacket, 4) == 0) {
							switch(ReceivePacket[4]) {
								case 0x01:
									*pConnection = OFFLINE_NET;
									*pMenu = START_MENU;
									close(sockSF);
									break;
								case 0x03:
									int32_t entitySL;
									memcpy(&entitySL, ReceivePacket+5, sizeof(int32_t));
									entityTable[entitySL] = 0;
									break;
								case 0x04:
									memcpy(&entityIQ, ReceivePacket+5, sizeof(entityData));
									if(entitySelf == &entity[entityIQ.slot]) {
										entityIQ.unique.Player.criterion = criterionSelf;
										int32_t posSFX = entitySelf -> fldX;
										int32_t posSFY = entitySelf -> fldY;
										int32_t posIQX = entityIQ.fldX;
										int32_t posIQY = entityIQ.fldY;
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
									entityTable[entityIQ.slot] = true;
									break;
								case 0x05:
									fieldSelector selectorIQ;
									memcpy(&selectorIQ, ReceivePacket+5, sizeof(fieldSelector));
									int32_t lclX = selectorIQ.fldX - entitySelf -> fldX + 1;
									int32_t lclY = selectorIQ.fldY - entitySelf -> fldY + 1;
									if(selectorIQ.zone == entitySelf -> zone && lclX > -1 && lclX < 3 && lclY > -1 && lclY < 3) {
										if(currentSegment.posX < 128) {
											meshTask[currentSegment.lclX][currentSegment.lclY][0] = true;
											meshTask[currentSegment.lclX][currentSegment.lclY][1] = true;
										}
										currentSegment.lclX = lclX;
										currentSegment.lclY = lclY;
										currentSegment.posX = 0;
									}else{
										nonsense(__LINE__);
									}
									break;
								case 0x06:
									memcpy(&entityIQ, ReceivePacket+5, sizeof(entityData));
									entity[entityIQ.slot] = entityIQ;
									entityTable[entityIQ.slot] = true;
									entitySelf = &entity[entityIQ.slot];
									entitySelf -> unique.Player.criterion = criterionSelf;
									*pMenu = INGAME_MENU;
									*pConnection = CONNECTED_NET;
									break;
								case 0x07:
									blockData dataIQ;
									memcpy(&dataIQ, ReceivePacket+5, sizeof(blockData));
									lclX = dataIQ.fldX - entitySelf -> fldX + 1;
									lclY = dataIQ.fldY - entitySelf -> fldY + 1;
									int32_t* blockIQ = &local[lclX][lclY].block[dataIQ.posX][dataIQ.posY][dataIQ.layer];
									int32_t blockPR = *blockIQ;
									if(lclX > -1 && lclX < 3 && lclY > -1 && lclY < 3) {
										*blockIQ = dataIQ.block;
										blockTask[lclX][lclY][dataIQ.posX][dataIQ.posY][dataIQ.layer] = true;
										if((block[dataIQ.layer][*blockIQ].illuminant || block[dataIQ.layer][blockPR].illuminant > block[dataIQ.layer][dataIQ.block].illuminant) && (dataIQ.layer == 1 || block[1][local[lclX][lclY].block[dataIQ.posX][dataIQ.posY][1]].transparent)) {
											fillLightMap();
											lightTask[lclX][lclY][0] = true;
											lightTask[lclX][lclY][1] = true;
										}
									}else{
										nonsense(__LINE__);
									}
									break;
								case 0x09:
									int32_t templateIQ, value;
									memcpy(&templateIQ, ReceivePacket+5, sizeof(int32_t));
									memcpy(&value, ReceivePacket+9, sizeof(int32_t));
									criterionSelf[templateIQ] = value;
									break;
								case 0x0B:
									puts("Received");
									currentHour = (int32_t)ReceivePacket[5];
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
						}else{
							if(currentSegment.posX < 128) {
								memcpy(local[currentSegment.lclX][currentSegment.lclY].block[currentSegment.posX], ReceivePacket, QDIV_PACKET_SIZE);
								currentSegment.posX++;
								if(currentSegment.posX == 128) {
									meshTask[currentSegment.lclX][currentSegment.lclY][0] = true;
									meshTask[currentSegment.lclX][currentSegment.lclY][1] = true;
								}
							}else{
								nonsense(__LINE__);
							}
						}
					}
				}
				disconnect:
				*pMenu = START_MENU;
				close(sockSF);
			}else{
				#ifdef _WIN32
				printf("%d\n", WSAGetLastError());
				#endif
				*pConnectButton = false;
				puts("[Out] Failed to establish Connection");
			}
		}
    }
    #ifdef _WIN32
    WSACleanup();
    #endif
}

int32_t main() {
	printf("\n–––– qDivClient-%d.%c ––––\n\n", QDIV_VERSION, QDIV_BRANCH);
	puts("[Out] Initialising GLFW");
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	int8_t windowVersion[32];
	sprintf(windowVersion, "qDiv-%d.%c", QDIV_VERSION, QDIV_BRANCH);
	window = glfwCreateWindow(715, 715, windowVersion, NULL, NULL);
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, callback_window);
	glfwSetKeyCallback(window, callback_keyboard);
	glfwSetMouseButtonCallback(window, callback_mouse);
	glfwSetCursorPosCallback(window, cursorCallback);
	glfwSetCharCallback(window, callback_typing);
	gladLoadGL();
	puts("[Out] Preparing Index Data");
	prepareIndexData();
	puts("[Out] Preparing Field Mesh");
	prepareMesh();
	puts("[Out] Preparing Menu Grid");
	prepareGrid();
	puts("[Out] Creating Buffers");
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
	glGenVertexArrays(18, (GLuint*)FVAO);
	glGenBuffers(18, (GLuint*)FVBO);
	int32_t bfrX = 0;
	int32_t bfrY = 0;
	int32_t bfrL = 0;
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
	puts("[Out] Loading Audio Device");
	audioDecoderConfig = ma_decoder_config_init(QDIV_AUDIO_FORMAT, QDIV_AUDIO_CHANNELS, QDIV_AUDIO_RATE);
	audioDeviceConfig = ma_device_config_init(ma_device_type_playback);
    audioDeviceConfig.playback.format = QDIV_AUDIO_FORMAT;
    audioDeviceConfig.playback.channels = QDIV_AUDIO_CHANNELS;
    audioDeviceConfig.sampleRate = QDIV_AUDIO_RATE;
    audioDeviceConfig.dataCallback = callback_audio;
    audioDeviceConfig.pUserData = NULL;
    ma_device_init(NULL, &audioDeviceConfig, &audioDevice);
    ma_device_start(&audioDevice);
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
	QDIV_MATRIX_RESET();
	QDIV_COLOR_RESET();
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
    QDIV_MKDIR("player");
    QDIV_MKDIR("player/identity");
    FILE* settingsFile = fopen("player/settings.dat", "rb");
    if(settingsFile == NULL) {
		memset(settings.name, 0x00, sizeof(settings.name));
		settings.nameCursor = 0;
		settings.vsync = true;
		settings.verboseDebug = false;
		glfwSwapInterval(1);
    }else{
    	fread(&settings, 1, sizeof(settings), settingsFile);
		fclose(settingsFile);
		settings.vsync ? glfwSwapInterval(1) : glfwSwapInterval(0);
    }
    artifactMenu.role = BUILDER;
    artifactMenu.page = 0;
    int8_t  textIQ[32];
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
				QDIV_DRAW(6, 0);
				QDIV_MATRIX_RESET();
				if(menuButton(0.1f, -0.4f, 0.15f, 8, "Play", NULL, 4)) currentMenu = SERVER_MENU;
				if(menuButton(0.1f, -0.4f, 0.f, 8, "Settings", NULL, 8)) currentMenu = SETTINGS_MENU;
				if(menuButton(0.1f, -0.4f, -0.15f, 8, "About qDiv", NULL, 10));
				renderText("Copyright 2023\nGabriel F. Hodges", 32,-0.98f, -0.9f, 0.05, TEXT_LEFT);
				break;
			case SERVER_MENU:
				int32_t serverSL = 0;
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
				int8_t* vsyncOPT = settings.vsync ? "Vsync On" : "Vsync Off";
				int8_t* cursorOPT = settings.cursorsync ? "Cursor: Synced" : "Cursor: Instant";
				int8_t* debugOPT = settings.verboseDebug ? "Verbose Debug" : "Hidden Debug";
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
				if(menuButton(0.1f, -0.4f, -0.15f, 8, debugOPT, NULL, strlen(debugOPT))) settings.verboseDebug = settings.verboseDebug ? false : true;
				if(Keyboard == GLFW_KEY_ESCAPE) {
					Keyboard = 0x00;
					currentMenu = (Connection == OFFLINE_NET) * START_MENU + (Connection == CONNECTED_NET) * INGAME_MENU;
				}
				break;
			case CONNECTING_MENU:
				break;
			case INGAME_MENU:
				glm_translate2d_to(matrix, (vec2){-qBlock * entitySelf -> posX + 2.0, -qBlock * entitySelf -> posY + 2.0}, motion);
				QDIV_MATRIX_UPDATE();
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
				QDIV_COLOR_RESET();
				QDIV_MATRIX_RESET();
				glBindVertexArray(MVAO);
				glBindTexture(GL_TEXTURE_2D, interfaceTex);
				QDIV_DRAW(120, 0);
				QDIV_DRAW(120, 9120);
				glBindVertexArray(VAO);
				sprintf(textIQ, "%llu", entitySelf -> qEnergy);
				renderText(textIQ, strlen(textIQ), -0.975, -0.975, 0.06, TEXT_LEFT);
				sprintf(textIQ, "%s", entitySelf -> name);
				renderText(textIQ, strlen(textIQ), -0.975, 0.925, 0.06, TEXT_LEFT);
				if(entitySelf -> type == PLAYER) renderSelectedArtifact();
				QDIV_MATRIX_RESET();
				if(Keyboard == GLFW_KEY_ESCAPE) {
					Keyboard = 0x00;
					currentMenu = SETTINGS_MENU;
				}
				break;
			case ARTIFACT_MENU:
				int32_t hoveredArtifact = screenToArtifact();
				glBindVertexArray(MVAO);
				glBindTexture(GL_TEXTURE_2D, interfaceTex);
				QDIV_DRAW(2400, 0);
				glBindVertexArray(VAO);
				sprintf(textIQ, "%llu", entitySelf -> qEnergy);
				renderText(textIQ, strlen(textIQ), -0.975, -0.975, 0.06, TEXT_LEFT);
				sprintf(textIQ, "%s", role[artifactMenu.role].name);
				QDIV_COLOR_UPDATE(role[artifactMenu.role].textColor.red, role[artifactMenu.role].textColor.green, role[artifactMenu.role].textColor.blue, 1.f);
				renderText(textIQ, strlen(textIQ), -0.975, 0.925, 0.06, TEXT_LEFT);
				QDIV_COLOR_RESET();
				sprintf(textIQ, "Page: %d/%d", artifactMenu.page + 1, role[artifactMenu.role].maxArtifact / 360 + 1);
				renderText(textIQ, strlen(textIQ), 0.975, -0.975, 0.06, TEXT_RIGHT);
				if(hoveredArtifact > -1 && hoveredArtifact < role[artifactMenu.role].maxArtifact && hoverCheck(-1.f, -0.9f, 1.f, 0.9f)) {
					float posX = (float)(floor(cursorX * 10) * 0.1f);
					float posY = (float)(floor(cursorY * 10) * 0.1f);
					setAtlasArea(0.f, 0.25f, 0.25f);
					setScalePos(0.1f, 0.1f, posX, posY);
					glBindTexture(GL_TEXTURE_2D, interfaceTex);
					QDIV_DRAW(6, 0);
					QDIV_MATRIX_RESET();
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
				int32_t artifactSL = 360 * artifactMenu.page;
				float slotX = -0.975f;
				float slotY = 0.825f;
				while(artifactSL < role[artifactMenu.role].maxArtifact && slotY > -0.9f) {
					setScalePos(0.05f, 0.05f, slotX, slotY);
					glBindTexture(GL_TEXTURE_2D, artifact[artifactMenu.role][artifactSL++].texture);
					QDIV_DRAW(6, 0);
					slotX += 0.1f;
					if(slotX >= 1.f) {
						slotX = -0.975f;
						slotY -= 0.1f;
					}
				}
				QDIV_MATRIX_RESET();
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
				int8_t  desc[256];
				glBindVertexArray(VAO);
				artifactSettings* artifactIQ = &artifact[artifactMenu.role][hoveredArtifact];
				sprintf(textIQ, "%s", artifactIQ -> name);
				renderText(textIQ, strlen(textIQ), -0.95, 0.85, 0.1, TEXT_LEFT);
				sprintf(desc, "%s", artifactIQ -> desc);
				renderText(desc, strlen(desc), -0.95, 0.7, 0.05, TEXT_LEFT);
				int32_t templateSL, criterionSL;
				if(artifactIQ -> qEnergy > 0) {
					criterionSL = 1;
					sprintf(textIQ, "%llu/%llu qEnergy", entitySelf -> qEnergy, artifactIQ -> qEnergy);
					renderText(textIQ, strlen(textIQ), 0.85, -0.95, 0.05, TEXT_RIGHT);
				}else{
					criterionSL = 0;
				}
				for(int32_t criterionSL = 0; criterionSL < 4; criterionSL++) {
					templateSL = artifactIQ -> criterion[criterionSL].Template;
					if(templateSL != NO_CRITERION) {
						sprintf(textIQ, "%d/%d %s", criterionSelf[templateSL], artifactIQ -> criterion[criterionSL].value, criterionTemplate[templateSL].desc);
						renderText(textIQ, strlen(textIQ), 0.85, -0.95 + (double)criterionSL * 0.1, 0.05, TEXT_RIGHT);
					}
				}
				glBindTexture(GL_TEXTURE_2D, artifactIQ -> texture);
				setScalePos(0.375, 0.375, -0.875, -0.875);
				QDIV_DRAW(6, 0);
				if(Keyboard == GLFW_KEY_ESCAPE) {
					Keyboard = 0x00;
					currentMenu = ARTIFACT_MENU;
				}
				break;
		}
		if(settings.verboseDebug) {
			int8_t textDG[64];
			sprintf(textDG, "FPS -> %d", (int32_t)(1 / qFactor));
			renderText(textDG, strlen(textDG), -0.975, 0.8, 0.05, TEXT_LEFT);
			sprintf(textDG, "muspf -> %f", qFactor);
			renderText(textDG, strlen(textDG), -0.975, 0.75, 0.05, TEXT_LEFT);
			sprintf(textDG, "Draw Calls -> %d", drawCalls);
			drawCalls = 0;
			renderText(textDG, strlen(textDG), -0.975, 0.7, 0.05, TEXT_LEFT);
		}
		if(Connection == CONNECTED_NET) {
			for(int32_t entitySL = 0; entitySL < 10000; entitySL++) {
				if(entityTable[entitySL]) {
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
					entityTable[entitySL] = entityInRange(entity + entitySL, entitySelf);
				}
			}
		}
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	settingsFile = fopen("player/settings.dat", "wb");
	fwrite(&settings, 1, sizeof(settings), settingsFile);
	fclose(settingsFile);
	Connection = OFFLINE_NET;
	qDivRun = false;
	pthread_join(usage_id, NULL);
	pthread_join(gate_id, NULL);
	glfwTerminate();
	ma_device_uninit(&audioDevice);
	for(int32_t decoderSL = 0; decoderSL < QDIV_AUDIO_DECODERS; decoderSL++) ma_decoder_uninit(audioDecoder + decoderSL);
	return 0;
}
