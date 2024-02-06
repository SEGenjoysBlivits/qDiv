/*
qDivLib - The Common building blocks of the 2D sandbox game "qDiv" 
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
#pragma once
#include<limits.h>
#if(CHAR_BIT != 8)
	#error qDiv is not supported on this CPU architecture
#endif
#include "qDivEnum.h"
#include "qDivType.h"
#include<string.h>
#include<math.h>
#define QDIV_PORT 38652
#define QDIV_MAX_PLAYERS 64
#define QDIV_MAX_ENTITIES 10000
#define QDIV_MAX_FIELDS 576
#define QDIV_UNIQUE_ENTITY 9
#define QDIV_B256 16
#define QDIV_B16 32
#define FOR_EVERY_ENTITY for(int32_t entitySL = 0; entitySL < 10000; entitySL++)
#define DIAGONAL_DOWN_FACTOR 0.7071
#define DIAGONAL_UP_FACTOR 1.4142
#define QDIV_HITBOX_UNIT 0.4
#ifdef QDIV_CLIENT
#define QDIV_SPLIT(forClient, forServer, forOther) forClient
#else
#ifdef QDIV_SERVER
#define QDIV_SPLIT(forClient, forServer, forOther) forServer
#else
#define QDIV_SPLIT(forClient, forServer, forOther) forOther
#endif
#endif
#ifdef _WIN32
	#pragma comment(lib, "wsock32.lib")
	#pragma comment(lib, "Ws2_32.lib")
	#include<winsock2.h>
	#include<windows.h>
	#include<iphlpapi.h>
	#include<ws2tcpip.h>
	#define QDIV_MKDIR(folder) mkdir(folder)
	#define _GLFW_WIN32
	#define QDIV_RANDOM(buffer, bufferSZ) for(int byteSL = 0; byteSL < bufferSZ / 4; byteSL++) rand_s(((unsigned int*)buffer) + byteSL)
	#define QDIV_CLOSE(sockIQ) closesocket(sockIQ)
#else
	#include<arpa/inet.h>
	#include<netdb.h>
	#include<sys/types.h>
	#include<sys/socket.h>
	#include<sys/stat.h>
	#include<unistd.h>
	#include<signal.h>
	#include<sys/time.h>
	#include<sys/random.h>
	#define QDIV_MKDIR(folder) mkdir(folder, S_IRWXU);
	#define _GLFW_X11
	#define QDIV_RANDOM(buffer, bufferSZ) getrandom((void*)buffer, bufferSZ, 0)
	#define QDIV_CLOSE(sockIQ) close(sockIQ)
#endif
#include "include/aes.h"
#define ECC_CURVE NIST_K571
#include "include/ecdh.h"
#define QDIV_AUTH
#include "elements.h"
#include "QSM.h"

const int32_t QDIV_VERSION = 68;
const int8_t QDIV_BRANCH = 'S';

int32_t* sockSF;

typedef struct client_s {
	entity_t* entity;
	bool connected;
	int8_t name[16];
	uint8_t uuid[16];
	struct sockaddr_storage addr;
	socklen_t addrSZ;
	struct AES_ctx AES_CTX;
	uint8_t challenge[QDIV_PACKET_SIZE];
} client_t;

int gettimeofday(struct timeval *__restrict __tv, void *__restrict __tz);
int close(int fd);

int32_t clampInt(int32_t valMin, int32_t valIQ, int32_t valMax) {
	bool valBL = valMin <= valIQ && valIQ <= valMax;
	return valIQ * valBL + valMin * (valIQ < valMin) + valMax * (valMax < valIQ);
}

uint8_t uuidNull[QDIV_B256] = {0x00};
bool qDivRun = true;
bool* pRun = &qDivRun;

double qFactor;

// Network IDs

// 0x01 Close Connection
// 0x02 Movement
// 0x03 Entity Deletion
// 0x04 Entity Update
// 0x05 Field Request
// 0x06 Initializor
// 0x07 Block Update
// 0x08 Select Artifact
// 0x09 Criterion Update
// 0x0A Usage Start
// 0x0B Light Task

uint8_t recvBF[QDIV_PACKET_SIZE];
uint8_t sendBF[QDIV_PACKET_SIZE];
uint8_t fieldBF[32768];

uint8_t currentHour = 0;

block_st block[2][16384];
criterion_st criterionTemplate[MAX_CRITERION];
role_st role[6];
entity_st entityType[MAX_ENTITY_TYPE];
artifact_st artifact[6][4000];

entity_t entity[10000];
bool entityTable[10000] = {false};

const color RED = {1.f, 0.f, 0.f, 1.f};
const color ORANGE = {1.f, 0.5f, 0.f, 1.f};
const color YELLOW = {1.f, 1.f, 0.f, 1.f};
const color GREEN = {0.f, 0.75f, 0.f, 1.f};
const color BLUE = {0.f, 0.1f, 1.f, 1.f};
const color PURPLE = {0.75f, 0.f, 1.f, 1.f};
const color GRAY = {0.5f, 0.5f, 0.5f, 1.f};

const float blockT = 0.0078125f;

collection_st cropPlaceable = {1, {SOIL}};

void nonsense(int32_t lineIQ) { printf("\033[0;31m> Nonsensical Operation at Line %d\033[0;37m\n", lineIQ); }
void debug(int32_t lineIQ) { printf("\033[0;32m> Code Reached at Line %d\033[0;37m\n", lineIQ); }

bool qrngIterate(qrng_t* qrngIQ) {
	if(qrngIQ -> iteration == 0) qrngIQ -> occasion = qrngIQ -> frequency - (rand() % qrngIQ -> stability);
	qrngIQ -> iteration++;
	if(qrngIQ -> iteration >= qrngIQ -> frequency) qrngIQ -> iteration = 0;
	return qrngIQ -> iteration == qrngIQ -> occasion;
}

size_t sizeMin(size_t valA, size_t valB) {
	return (valA < valB) * valA + (valA >= valB) * valB;
}

#if defined(QDIV_CLIENT) || defined(QDIV_SERVER)
void send_encrypted(uint8_t prefix, void* payload, size_t payloadSZ, client_t* clientIQ) {
	encryptable_t packetIQ;
	QDIV_RANDOM(packetIQ.Iv, AES_BLOCKLEN);
	QDIV_RANDOM(packetIQ.payload, QDIV_PACKET_SIZE);
	packetIQ.payload[0] = prefix;
	if(payload != NULL) memcpy(packetIQ.payload + 1, payload, payloadSZ);
	struct AES_ctx AES_IQ = clientIQ -> AES_CTX;
	AES_ctx_set_iv(&AES_IQ, packetIQ.Iv);
	AES_CBC_encrypt_buffer(&AES_IQ, packetIQ.payload, QDIV_PACKET_SIZE);
	sendto(*sockSF, (char*)&packetIQ, sizeof(encryptable_t), 0, (struct sockaddr*)&clientIQ -> addr, clientIQ -> addrSZ);
}
#endif

// Packet
void makeBlockPacket(uint16_t inBlock, int32_t inFX, int32_t inFY, int32_t inX, int32_t inY, int32_t inLayer) {
	sendBF[4] = 0x07;
	block_l dataIQ;
	dataIQ.block = inBlock;
	dataIQ.fldX = inFX;
	dataIQ.fldY = inFY;
	dataIQ.posX = inX;
	dataIQ.posY = inY;
	dataIQ.layer = inLayer;
	memcpy(sendBF+5, &dataIQ, sizeof(block_l));
}

void uuidToHex(uint8_t* uuid, int8_t* hex) {
	for(int32_t byteSL = 0; byteSL < QDIV_B256 + 1; byteSL++) {
		if(uuid[byteSL] >= 0x10) {
			sprintf(hex + (byteSL * 2),"%x", uuid[byteSL]);
		}else{
			sprintf(hex + (byteSL * 2),"0%x", uuid[byteSL]);
		}
	}
	hex[QDIV_B16] = 0x00;
}

void derelativize(int* lclX, int* lclY, double* posX, double* posY) {
	*lclX = 1;
	*lclY = 1;
	if(*posX < 0) {
		*posX += 128;
		*lclX = 0;
	}else if(*posX >= 128) {
		*posX -= 128;
		*lclX = 2;
	}
	if(*posY < 0) {
		*posY += 128;
		*lclY = 0;
	}else if(*posY >= 128) {
		*posY -= 128;
		*lclY = 2;
	}
}

void initGenerator(int32_t seed);
void useGenerator(field_t* fieldIQ);

criterion_st makeCriterionTemplate(int8_t* inDesc, color inColor) {
	criterion_st TemplateIQ;
	strcpy(TemplateIQ.desc, inDesc);
	TemplateIQ.textColor = inColor;
	return TemplateIQ;
}

bool entityInRange(entity_t* entityIQ, entity_t* entityAS) {
	return entityIQ -> zone == entityAS -> zone && entityIQ -> fldX < entityAS -> fldX + 2 && entityIQ -> fldX > entityAS -> fldX - 2 && entityIQ -> fldY < entityAS -> fldY + 2 && entityIQ -> fldY > entityAS -> fldY - 2 && entityIQ -> posX >= -128 && entityIQ -> posX < 256 && entityIQ -> posY >= -128 && entityIQ -> posY < 256;
}

entity_st makeEntityType(int32_t inHealth, bool inPersist, int32_t inBox, bool inClip, double inDespawn, double inSpeed, uint64_t inEnergy, entityAction inAction, int32_t inKill) {
	entity_st typeIQ;
	typeIQ.maxHealth = inHealth;
	typeIQ.persistant = inPersist;
	typeIQ.hitBox = (double)inBox * QDIV_HITBOX_UNIT;
	typeIQ.noClip = inClip;
	typeIQ.despawnTM = inDespawn;
	typeIQ.speed = inSpeed;
	typeIQ.qEnergy = inEnergy;
	typeIQ.action = inAction;
	typeIQ.kill_criterion = inKill;
	return typeIQ;
}

// Renderers
void basicPlayer(void* entityVD);
void basicSnail(void* entityVD);
void rotatingBug(void* entityVD);

// Actions
void playerAction(void* entityVD);
void snailAction(void* entityVD);

void makeEntityTypes() {
	entityType[NULL_ENTITY] = makeEntityType(0, false, 0, false, -1, 0.0, 1, NULL, NO_CRITERION);
	entityType[PLAYER] = makeEntityType(5, true, 2, false, -1, 1.5, 1, QDIV_SPLIT(&basicPlayer, &playerAction, NULL), NO_CRITERION);
	entityType[SHALLAND_SNAIL] = makeEntityType(5, false, 4, false, 60, 0.25, 1, QDIV_SPLIT(&basicSnail, &snailAction, NULL), NO_CRITERION);
	entityType[CALCIUM_CRAWLER] = makeEntityType(8, false, 2, false, 300, 1.5, 1, QDIV_SPLIT(&rotatingBug, &snailAction, NULL), NO_CRITERION);
}

int32_t getOccupiedLayer(uint16_t* blockPos) {
	if(blockPos[1] != NO_WALL) {
		return 1;
	}else if(blockPos[0] != NO_FLOOR) {
		return 0;
	}else return -1;
}

bool qEnergyRelevance(uint64_t playerEnergy, block_st* blockIQ) {
	uint64_t actualEnergy = playerEnergy * (playerEnergy > 100) + 100 * (playerEnergy <= 100);
	return blockIQ -> qEnergy <= actualEnergy && blockIQ -> qEnergy > playerEnergy * 4 / 5;
}

bool qEnergyRelevanceByValue(uint64_t playerEnergy, uint64_t value) {
	uint64_t actualEnergy = playerEnergy * (playerEnergy > 100) + 100 * (playerEnergy <= 100);
	return value <= actualEnergy && value > playerEnergy * 4 / 5;
}

int32_t layerEX;
block_st* blockEX;

void makeBlock(uint16_t blockIQ, double inFriction, bool inTransparent, int32_t inTexX, int32_t inTexY, int32_t inSizeX, int32_t inSizeY, const uint8_t* inMineSound, size_t inMineSoundSZ, int32_t inIlluminance, uint64_t inEnergy, collection_st* inPlaceable, int32_t inType, int32_t inMine) {
	blockEX = &block[layerEX][blockIQ];
	blockEX -> friction = inFriction;
	blockEX -> transparent = inTransparent;
	blockEX -> texX = (float)inTexX * blockT;
	blockEX -> texY = (float)inTexY * blockT;
	blockEX -> sizeX = (float)inSizeX * blockT;
	blockEX -> sizeY = (float)inSizeY * blockT;
	blockEX -> mine_sound = inMineSound;
	blockEX -> mine_soundSZ = inMineSoundSZ;
	blockEX -> illuminance = inIlluminance;
	blockEX -> qEnergy = inEnergy;
	blockEX -> placeable = inPlaceable;
	blockEX -> type = inType;
	blockEX -> mine_criterion = inMine;
}

void makeFloor(uint16_t blockIQ, double inFriction, int32_t inTexX, int32_t inTexY, const uint8_t* inMineSound, size_t inMineSoundSZ, int32_t inIlluminance, uint64_t inEnergy, int32_t inType, int32_t inMine) {
	makeBlock(blockIQ, inFriction, false, inTexX, inTexY, 1, 1, inMineSound, inMineSoundSZ, inIlluminance, inEnergy, NULL, inType, inMine);
}

void makePortal(uint16_t blockIQ, int32_t inDest, int8_t* inText) {
	makeBlock(blockIQ, 1.0, true, 18, 0, 2, 2, NULL, 0, 3, 0, NULL, ZONE_PORTAL, NO_CRITERION);
	blockEX -> unique.portal.destination = inDest;
	strcpy(blockEX -> unique.portal.hoverText, inText);
}

void makeBlocks() {
	// Floor
	layerEX = 0;
	makeFloor(NO_FLOOR, 1.0, 0, 0, NULL, 0, 0, 0, NO_TYPE_ST, NO_CRITERION);
	makeFloor(SHALLAND_FLOOR, 1.0, 1, 0, NULL, 0, 0, 60, NO_TYPE_ST, NO_CRITERION);
	makeFloor(SHALLAND_FLATGRASS, 1.0, 2, 0, NULL, 0, 0, 40, FERTILE, NO_CRITERION);
	makeFloor(WATER, 0.5, 3, 0, NULL, 0, 0, 0, NO_TYPE_ST, NO_CRITERION);
	makeFloor(SAND, 0.9, 4, 0, NULL, 0, 0, 42, NO_TYPE_ST, NO_CRITERION);
	makeFloor(ARIDIS_FLATGRASS, 1.0,  5, 0, NULL, 0, 0, 40, FERTILE, NO_CRITERION);
	makeFloor(ARIDIS_FLOOR, 1.0, 6, 0, NULL, 0, 0, 60, NO_TYPE_ST, NO_CRITERION);
	makeFloor(REDWOOD_FLATGRASS, 1.0, 7, 0, NULL, 0, 0, 40, FERTILE, NO_CRITERION);
	makeFloor(REDWOOD_FLOOR, 1.0, 8, 0, NULL, 0, 0, 60, NO_TYPE_ST, NO_CRITERION);
	makeFloor(SOIL, 1.0, 9, 0, NULL, 0, 0, 35, NO_TYPE_ST, NO_CRITERION);
	makeFloor(LIMESTONE_FLOOR, 1.0, 10, 0, NULL, 0, 0, 100, NO_TYPE_ST, NO_CRITERION);
	// Wall
	layerEX = 1;
	makeBlock(NO_WALL, 1.0, true, 0, 0, 1, 1, NULL, 0, 0, 0, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(SHALLAND_WALL, 0.9, false, 1, 0, 1, 1, NULL, 0, 0, 60, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(LAMP, 1.0, true, 2, 0, 1, 1, NULL, 0, 8, 80, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(SHALLAND_GRASS, 1.0, true, 3, 0, 1, 1, NULL, 0, 0, 40, NULL, NO_TYPE_ST, MINE_SHALLAND_GRASS);
	makeBlock(SHALLAND_BUSH, 1.0, true, 4, 0, 1, 2, NULL, 0, 0, 75, NULL, NO_TYPE_ST, MINE_SHALLAND_BUSH);
	makeBlock(ARIDIS_GRASS, 1.0, true, 5, 0, 1, 1, NULL, 0, 0, 40, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(ARIDIS_BUSH, 1.0, true, 6, 0, 1, 3, NULL, 0, 0, 75, NULL, NO_TYPE_ST, MINE_ARIDIS_BUSH);
	makeBlock(ARIDIS_WALL, 0.9, false, 7, 0, 1, 1, NULL, 0, 0, 60, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(AGAVE, 0.9, true, 8, 0, 2, 1, NULL, 0, 0, 200, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(REDWOOD_WEEDS, 1.0, true, 10, 0, 1, 1, NULL, 0, 0, 40, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(REDWOOD_TREE, 0.9, true, 11, 0, 2, 4, NULL, 0, 0, 90, NULL, NO_TYPE_ST, MINE_REDWOOD_TREE);
	makeBlock(REDWOOD_LOG, 1.0, true, 13, 0, 2, 1, NULL, 0, 0, 60, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(REDWOOD_WALL, 0.9, false, 15, 0, 1, 1, NULL, 0, 0, 60, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(IMMINENT_POTATO, 1.0, true, 16, 0, 1, 1, NULL, 0, 0, 35, &cropPlaceable, CROP, NO_CRITERION);
	blockEX -> unique.crop.growthDuration = 4;
	blockEX -> unique.crop.successor = POTATO;
	makeBlock(POTATO, 1.0, true, 17, 0, 1, 1, NULL, 0, 0, 80, NULL, DECOMPOSING, NO_CRITERION);
	blockEX -> unique.decomposite = IMMINENT_POTATO;
	makePortal(OVERWORLD_PORTAL, OVERWORLD, "Overworld");
	makePortal(CAVERN_PORTAL, CAVERN, "Cavern");
	makeBlock(LIMESTONE_WALL, 0.9, false, 20, 0, 1, 1, NULL, 0, 0, 100, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(STALAGMITE, 1.0, true, 21, 0, 1, 1, NULL, 0, 0, 90, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(LIMESTONE_BRICKS, 0.9, false, 22, 0, 1, 1, NULL, 0, 0, 100, NULL, NO_TYPE_ST, NO_CRITERION);
	makeBlock(BRONZE_CLUMP, 0.9, true, 23, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 0, 120, NULL, NO_TYPE_ST, MINE_BRONZE);
	makeBlock(ALUMINIUM_CLUMP, 0.9, true, 24, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 0, 140, NULL, NO_TYPE_ST, MINE_ALUMINIUM);
	makeBlock(IRON_CLUMP, 0.9, true, 25, 0, 1, 1, NULL, 0, 0, 175, NULL, NO_TYPE_ST, MINE_IRON);
}

void makeCriterionTemplates() {
	criterionTemplate[MINE_SHALLAND_BUSH] = makeCriterionTemplate("Shalland Bush Mined", YELLOW);
	criterionTemplate[MINE_ARIDIS_BUSH] = makeCriterionTemplate("Aridis Bush Mined", YELLOW);
	criterionTemplate[MINE_REDWOOD_TREE] = makeCriterionTemplate("Redwood Trees Mined", YELLOW);
	criterionTemplate[FERTILIZE_LAND] = makeCriterionTemplate("Blocks Fertilized", GREEN);
	criterionTemplate[MINE_SHALLAND_GRASS] = makeCriterionTemplate("Shalland Grass Mined", YELLOW);
	criterionTemplate[MINE_BRONZE] = makeCriterionTemplate("Bronze Mined", YELLOW);
	criterionTemplate[MINE_ALUMINIUM] = makeCriterionTemplate("Aluminium Mined", YELLOW);
	criterionTemplate[MINE_IRON] = makeCriterionTemplate("Iron Mined", YELLOW);
}

int32_t roleEX;
artifact_st* artifactEX;

#ifdef QDIV_CLIENT
void loadTexture(uint32_t* texture, const uint8_t* file, size_t fileSZ);
#endif

void makeArtifact4(int32_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, double inPrimaryTime, bool inPrimaryCancel, artifactAction inSecondary, uint64_t inSecondaryCost, double inSecondaryTime, bool inSecondaryCancel, uint64_t inEnergy, int32_t Template1, uint32_t value1, int32_t Template2, uint32_t value2, int32_t Template3, uint32_t value3, int32_t Template4, uint32_t value4) {
	artifactEX = &artifact[roleEX][artifactSL];
	strcpy(artifactEX -> name, inName);
	strcpy(artifactEX -> desc, inDesc);
	artifactEX -> crossCriterial = inCross;
	#ifdef QDIV_CLIENT
	loadTexture(&artifactEX -> texture, inTexture, inTextureSZ);
	#endif
	artifactEX -> primary = inPrimary;
	artifactEX -> secondary = inSecondary;
	artifactEX -> primaryCost = inPrimaryCost;
	artifactEX -> secondaryCost = inSecondaryCost;
	artifactEX -> primaryUseTime = inPrimaryTime;
	artifactEX -> secondaryUseTime = inSecondaryTime;
	artifactEX -> primaryCancelable = inPrimaryCancel;
	artifactEX -> secondaryCancelable = inSecondaryCancel;
	if(inEnergy > 0) {
		artifactEX -> qEnergy = inEnergy;
		artifactEX -> criterion[0].Template = NO_CRITERION;
		artifactEX -> criterion[0].value = 0;
	}else{
		artifactEX -> qEnergy = 0;
		artifactEX -> criterion[0].Template = Template1;
		artifactEX -> criterion[0].value = value1;
	}
	artifactEX -> criterion[0].Template = Template1;
	artifactEX -> criterion[0].value = value1;
	artifactEX -> criterion[1].Template = Template2;
	artifactEX -> criterion[1].value = value2;
	artifactEX -> criterion[2].Template = Template3;
	artifactEX -> criterion[2].value = value3;
	artifactEX -> criterion[3].Template = Template4;
	artifactEX -> criterion[3].value = value4;
}

void makeArtifact3(int32_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, double inPrimaryTime, bool inPrimaryCancel, artifactAction inSecondary, uint64_t inSecondaryCost, double inSecondaryTime, bool inSecondaryCancel, uint64_t inEnergy, int32_t Template1, uint32_t value1, int32_t Template2, uint32_t value2, int32_t Template3, uint32_t value3) {
	makeArtifact4(artifactSL, inName, inDesc, inTexture, inTextureSZ, inCross, inPrimary, inPrimaryCost, inPrimaryTime, inPrimaryCancel, inSecondary, inSecondaryCost, inSecondaryTime, inSecondaryCancel, inEnergy, Template1, value1, Template2, value2, Template3, value3, NO_CRITERION, 0);
}

void makeArtifact2(int32_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, double inPrimaryTime, bool inPrimaryCancel, artifactAction inSecondary, uint64_t inSecondaryCost, double inSecondaryTime, bool inSecondaryCancel, uint64_t inEnergy, int32_t Template1, uint32_t value1, int32_t Template2, uint32_t value2) {
	makeArtifact4(artifactSL, inName, inDesc, inTexture, inTextureSZ, inCross, inPrimary, inPrimaryCost, inPrimaryTime, inPrimaryCancel, inSecondary, inSecondaryCost, inSecondaryTime, inSecondaryCancel, inEnergy, Template1, value1, Template2, value2, NO_CRITERION, 0, NO_CRITERION, 0);
}

void makeArtifact1(int32_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, double inPrimaryTime, bool inPrimaryCancel, artifactAction inSecondary, uint64_t inSecondaryCost, double inSecondaryTime, bool inSecondaryCancel, uint64_t inEnergy, int32_t Template1, uint32_t value1) {
	makeArtifact4(artifactSL, inName, inDesc, inTexture, inTextureSZ, inCross, inPrimary, inPrimaryCost, inPrimaryTime, inPrimaryCancel, inSecondary, inSecondaryCost, inSecondaryTime, inSecondaryCancel, inEnergy, Template1, value1, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0);
}

#ifdef QDIV_CLIENT

void playSound(const uint8_t* file, size_t fileSZ);
void qDivAudioInit();
void qDivAudioStop();

#endif

def_ArtifactAction(simpleSwing);
def_ArtifactAction(sliceEntity);
def_ArtifactAction(mineBlock);
def_ArtifactAction(placeBlock);
def_ArtifactAction(fertilizeBlock);
def_ArtifactAction(scoopWater);

void makeArtifacts() {
	roleEX = WARRIOR;
	makeArtifact1(OLD_SLICER, "Old Slicer", "Use this to defeat your\nfirst few enemies.", QDIV_SPLIT(artifacts_old_slicer_png, NULL, NULL), QDIV_SPLIT(artifacts_old_slicer_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &sliceEntity, NULL), 0, 0.5, false, NULL, 0, 0, true, 0, NO_CRITERION, 0);
	artifactEX -> primarySettings.slice.decay = 5;
	roleEX = EXPLORER;
	roleEX = BUILDER;
	makeArtifact1(OLD_SWINGER, "Old Swinger", "Use this to break your\nfirst few blocks and\nget your first qEnergy.", QDIV_SPLIT(artifacts_old_swinger_png, NULL, NULL), QDIV_SPLIT(artifacts_old_swinger_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 5, true, NULL, 0, 0, true, 0, NO_CRITERION, 0);
	artifactEX -> primarySettings.slice.decay = 0;
	makeArtifact1(SHALLAND_FLOOR_ARTIFACT, "Shalland Wood Wall", "Placeable Wall.", QDIV_SPLIT(artifacts_shalland_wall_png, NULL, NULL), QDIV_SPLIT(artifacts_shalland_wall_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, NULL, 0, 0, true, 0, MINE_SHALLAND_BUSH, 5);
	artifactEX -> primarySettings.place.represent = SHALLAND_WALL;
	artifactEX -> primarySettings.place.layer = 1;
	makeArtifact1(SHALLAND_WALL_ARTIFACT, "Shalland Wood Floor", "Placeable Floor.", QDIV_SPLIT(artifacts_shalland_floor_png, NULL, NULL), QDIV_SPLIT(artifacts_shalland_floor_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, NULL, 0, 0, true, 0, MINE_SHALLAND_BUSH, 5);
	artifactEX -> primarySettings.place.represent = SHALLAND_FLOOR;
	artifactEX -> primarySettings.place.layer = 0;
	makeArtifact1(LAMP_ARTIFACT, "Block", "Just a test.", QDIV_SPLIT(artifacts_lamp_png, NULL, NULL), QDIV_SPLIT(artifacts_lamp_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, NULL, 0, 0, true, 20, NO_CRITERION, 0);
	artifactEX -> primarySettings.place.represent = LAMP;
	artifactEX -> primarySettings.place.layer = 1;
	makeArtifact1(ARIDIS_FLOOR_ARTIFACT, "Aridis Wood Wall", "Placeable Wall.", QDIV_SPLIT(artifacts_aridis_wall_png, NULL, NULL), QDIV_SPLIT(artifacts_aridis_wall_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, NULL, 0, 0, true, 0, MINE_ARIDIS_BUSH, 5);
	artifactEX -> primarySettings.place.represent = ARIDIS_WALL;
	artifactEX -> primarySettings.place.layer = 1;
	makeArtifact1(ARIDIS_WALL_ARTIFACT, "Aridis Wood Floor", "Placeable Floor.", QDIV_SPLIT(artifacts_aridis_floor_png, NULL, NULL), QDIV_SPLIT(artifacts_aridis_floor_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, NULL, 0, 0, true, 0, MINE_ARIDIS_BUSH, 5);
	artifactEX -> primarySettings.place.represent = ARIDIS_FLOOR;
	artifactEX -> primarySettings.place.layer = 0;
	makeArtifact1(REDWOOD_FLOOR_ARTIFACT, "Redwood Wood Wall", "Placeable Wall.", QDIV_SPLIT(artifacts_redwood_wall_png, NULL, NULL), QDIV_SPLIT(artifacts_redwood_wall_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, NULL, 0, 0, true, 0, MINE_REDWOOD_TREE, 5);
	artifactEX -> primarySettings.place.represent = REDWOOD_WALL;
	artifactEX -> primarySettings.place.layer = 1;
	makeArtifact1(REDWOOD_WALL_ARTIFACT, "Redwood Wood Floor", "Placeable Floor.", QDIV_SPLIT(artifacts_redwood_floor_png, NULL, NULL), QDIV_SPLIT(artifacts_redwood_floor_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, NULL, 0, 0, true, 0, MINE_REDWOOD_TREE, 5);
	artifactEX -> primarySettings.place.represent = REDWOOD_FLOOR;
	artifactEX -> primarySettings.place.layer = 0;
	makeArtifact4(FERRA, "Ferra", "This pickaxe will make\nall that time in the\ncaverns worthwhile.", QDIV_SPLIT(artifacts_ferra_png, NULL, NULL), QDIV_SPLIT(artifacts_ferra_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 3, true, NULL, 0, 0, true, 200, NO_CRITERION, 0, MINE_ALUMINIUM, 100, MINE_IRON, 5, MINE_ARIDIS_BUSH, 5);
	artifactEX -> primarySettings.slice.decay = 0;
	roleEX = GARDENER;
	makeArtifact1(SIMPLE_HOE, "Simple Hoe", "Use this to make your\nfirst field.", QDIV_SPLIT(artifacts_simple_hoe_png, NULL, NULL), QDIV_SPLIT(artifacts_simple_hoe_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 7.5, true, QDIV_SPLIT(&simpleSwing, &fertilizeBlock, NULL), 0, 1, true, 0, NO_CRITERION, 0);
	artifactEX -> primarySettings.slice.decay = 0;
	makeArtifact2(POTATO_ARTIFACT, "Shalland Potato", "Plantable Crop.", QDIV_SPLIT(artifacts_potato_png, NULL, NULL), QDIV_SPLIT(artifacts_potato_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, NULL, 0, 1, true, 0, FERTILIZE_LAND, 25, MINE_SHALLAND_GRASS, 5);
	artifactEX -> primarySettings.place.represent = IMMINENT_POTATO;
	artifactEX -> primarySettings.place.layer = 1;
	roleEX = ENGINEER;
	makeArtifact1(OLD_SCOOPER, "Old Scooper", "Use this to get qEnergy\nfrom Water or place\nWater with qEnergy.", QDIV_SPLIT(artifacts_old_scooper_png, NULL, NULL), QDIV_SPLIT(artifacts_old_scooper_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &scoopWater, NULL), 0, 1, true, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0, true, 0, NO_CRITERION, 0);
	artifactEX -> secondarySettings.place.represent = WATER;
	artifactEX -> secondarySettings.place.layer = 0;
	roleEX = WIZARD;
}

bool isArtifactUnlocked(int32_t roleSL, int32_t artifactSL, player_t* playerIQ) {
	if(playerIQ -> qEnergyMax < artifact[roleSL][artifactSL].qEnergy ||
	(playerIQ -> role != roleSL && playerIQ -> roleTM > 0)) return false;
	for(int32_t criterionSL = 0; criterionSL < QDIV_ARTIFACT_CRITERIA; criterionSL++) {
		int32_t TemplateSL = artifact[roleSL][artifactSL].criterion[criterionSL].Template;
		if(TemplateSL != NO_CRITERION && artifact[roleSL][artifactSL].criterion[criterionSL].value > playerIQ -> criterion[TemplateSL]) return false;
	}
	return true;
}

void makeRoles() {
	strcpy(role[WARRIOR].name, "Warrior");
	role[WARRIOR].textColor = RED;
	role[WARRIOR].maxArtifact = MAX_WARRIOR_ARTIFACT;
	role[WARRIOR].movement_speed = 1.0;
	role[WARRIOR].mining_factor = 1.0;
	strcpy(role[EXPLORER].name, "Explorer");
	role[EXPLORER].textColor = ORANGE;
	role[EXPLORER].maxArtifact = MAX_EXPLORER_ARTIFACT;
	role[EXPLORER].movement_speed = 1.5;
	role[EXPLORER].mining_factor = 1.0;
	strcpy(role[BUILDER].name, "Builder");
	role[BUILDER].textColor = YELLOW;
	role[BUILDER].maxArtifact = MAX_BUILDER_ARTIFACT;
	role[BUILDER].movement_speed = 1.0;
	role[BUILDER].mining_factor = 0.5;
	strcpy(role[GARDENER].name, "Gardener");
	role[GARDENER].textColor = GREEN;
	role[GARDENER].maxArtifact = MAX_GARDENER_ARTIFACT;
	role[GARDENER].movement_speed = 1.0;
	role[GARDENER].mining_factor = 1.0;
	strcpy(role[ENGINEER].name, "Engineer");
	role[ENGINEER].textColor = BLUE;
	role[ENGINEER].maxArtifact = MAX_ENGINEER_ARTIFACT;
	role[ENGINEER].movement_speed = 1.0;
	role[ENGINEER].mining_factor = 1.0;
	strcpy(role[WIZARD].name, "Wizard");
	role[WIZARD].textColor = PURPLE;
	role[WIZARD].maxArtifact = MAX_WIZARD_ARTIFACT;
	role[WIZARD].movement_speed = 1.0;
	role[WIZARD].mining_factor = 1.0;
}

bool inCollection(int32_t elementIQ, collection_st* collectionIQ) {
	for(size_t elementSL = 0; elementSL < collectionIQ -> length; elementSL++) {
		if(collectionIQ -> elements[elementSL] == elementIQ) return true;
	}
	return false;
}

void libMain() {
	printf("> Loading qDivLib-%d.%c\n", QDIV_VERSION, QDIV_BRANCH);
	memset(sendBF, 0xFF, 4 * sizeof(uint8_t));
	puts("> Adding Block Types");
	makeArtifacts();
	makeBlocks();
	puts("> Adding Criterion Templates");
	makeCriterionTemplates();
	puts("> Adding Roles");
	makeRoles();
}
