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
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
#include<math.h>
#define QDIV_PORT 38652
#define QDIV_MAX_PLAYERS 64
#define QDIV_MAX_ENTITIES 10000
#define QDIV_MAX_FIELDS 576
#define QDIV_PACKET_SIZE 1024
#define QDIV_UNIQUE_ENTITY 9
#define QDIV_ARTIFACT_CRITERIA 4
#define QDIV_B256 16
#define QDIV_B16 32
#define FOR_EVERY_ENTITY for(int32_t entitySL = 0; entitySL < 10000; entitySL++)
#define DIAGONAL_DOWN_FACTOR 0.7071
#define DIAGONAL_UP_FACTOR 1.4142
#define QDIV_HITBOX_UNIT 0.4
#ifdef _WIN32
#define QDIV_RANDOM(buffer, bufferSZ) for(int byteSL = 0; byteSL < bufferSZ / 4; byteSL++) rand_s(((unsigned int*)buffer) + byteSL)
#else
#include<sys/random.h>
#define QDIV_RANDOM(buffer, bufferSZ) getrandom((void*)buffer, bufferSZ, 0)
#endif
#define ECC_CURVE NIST_K571
#include "include/ecdh.h"
#define QDIV_AUTH

const int32_t QDIV_VERSION = 47;
const int8_t  QDIV_BRANCH = 'T';

int32_t clampInt(int32_t valMin, int32_t valIQ, int32_t valMax) {
	bool valBL = valMin <= valIQ && valIQ <= valMax;
	return valIQ * valBL + valMin * (valIQ < valMin) + valMax * (valMax < valIQ);
}

uint8_t uuidNull[QDIV_B256] = {0x00};

enum {
	OFFLINE_NET,
	WAITING_NET,
	CONNECTED_NET
} connectionEnum;

enum {
	FLOOR,
	WALL
} fieldLayer;

enum {
	NO_FLOOR,
	PLASTIC,
	SHALLAND_FLATGRASS,
	WATER
} floorEnum;

enum {
	NO_WALL,
	FILLED,
	LAMP,
	SHALLAND_GRASS,
	SHALLAND_BUSH
} wallEnum;

enum {
	NULL_ENTITY,
	PLAYER,
	SHALLAND_SNAIL,
	MAX_ENTITY_TYPE
} entityTypeEnum;

enum {
	NORTH,
	EAST,
	SOUTH,
	WEST,
	NORTH_STOP,
	EAST_STOP,
	SOUTH_STOP,
	WEST_STOP
} directionEnum;

enum {
	NO_CRITERION,
	MAX_CRITERION
} criterionEnum;

enum {
	OLD_SLICER,
	MAX_WARRIOR_ARTIFACT
} warriorEnum;
enum {
	MAX_EXPLORER_ARTIFACT
} explorerEnum;
enum {
	OLD_SWINGER,
	BLOCK,
	PLASTIC_ARTIFACT,
	LAMP_ARTIFACT,
	MAX_BUILDER_ARTIFACT
} builderEnum;
enum {
	MAX_GARDENER_ARTIFACT
} gardenerEnum;
enum {
	MAX_ENGINEER_ARTIFACT
} engineerEnum;
enum {
	MAX_WIZARD_ARTIFACT
} wizardEnum;

enum {
	WARRIOR,
	EXPLORER,
	BUILDER,
	GARDENER,
	ENGINEER,
	WIZARD
} roleEnum;

enum {
	NO_USAGE,
	PRIMARY_USAGE,
	SECONDARY_USAGE
} usageEnum;

typedef int32_t int24_t;

typedef struct {
	int32_t slot;
	int32_t active;
	int32_t edit;
	int32_t zone;
	int32_t fldX;
	int32_t fldY;
	int32_t block[128][128][2];
} fieldData;

typedef struct {
	int32_t fldX;
	int32_t fldY;
	int32_t posX;
	int32_t block[128][2];
} fieldSlice;

typedef struct {
	int32_t zone;
	int32_t fldX;
	int32_t fldY;
} fieldSelector;

typedef struct {
	bool traversable;
	bool transparent;
	bool illuminant;
	float texX;
	float texY;
	uint64_t qEnergy;
	bool qEnergyStatic;
} blockType;

typedef struct {
	float red;
	float green;
	float blue;
	float alpha;
} color;

typedef struct {
	int8_t desc[16];
	color textColor;
} criterionSettings;

typedef struct {
	int8_t name[16];
	color textColor;
	int32_t maxArtifact;
	double movement_speed;
	int32_t mining_speed;
} roleSettings;

typedef void(*entityAction)(void*);

typedef struct {
	int32_t maxHealth;
	bool persistant;
	double hitBox;
	bool noClip;
	double speed;
	uint64_t qEnergy;
	entityAction action;
} entitySettings;

typedef struct {
	bool filled;
	int8_t name[16];
	uint8_t uuid[16];
} identityData;

typedef struct {
	int* socketA;
	int32_t socket;
	int32_t role;
	double roleTimer;
	int32_t artifact;
	int32_t currentUsage;
	double useTimer;
	double useRelX;
	double useRelY;
	uint32_t* criterion;
} playerData;

typedef struct {
	int8_t name[16];
	uint8_t uuid[16];
	int32_t slot;
	int32_t type;
	int32_t active;
	int32_t zone;
	int32_t fldX;
	int32_t fldY;
	fieldData* local[3][3];
	double posX;
	double posY;
	double motX;
	double motY;
	int32_t health;
	double healthTimer;
	float mood;
	uint64_t qEnergy;
	union {
		playerData Player;
	} unique;
} entityData;

typedef struct {
	int32_t block;
	int32_t fldX;
	int32_t fldY;
	int32_t posX;
	int32_t posY;
	int32_t layer;
} blockData;

typedef struct {
	int32_t usage;
	double useRelX;
	double useRelY;
} usageData;

bool qDivRun = true;
bool* pRun = &qDivRun;

double qFactor;

// Network IDs

// 0x01 Close Connection
// 0x02 Movement
// 0x03 Entity Deletion
// 0x04 Entity Update
// 0x05 Slice
// 0x06 Initializor
// 0x07 Block Update
// 0x08 Select Artifact
// 0x09 Criterion Update
// 0x0A Usage Start
// 0x0B Light Task
// 0x0C Public Key
// 0x0D Identity

uint8_t ReceivePacket[QDIV_PACKET_SIZE];
uint8_t SendPacket[QDIV_PACKET_SIZE];

int32_t currentHour = 0;

blockType block[2][4096];
criterionSettings criterionTemplate[MAX_CRITERION];
roleSettings role[6];
entitySettings entityType[MAX_ENTITY_TYPE];

entityData entity[10000];
bool entityTable[10000] = {false};

const color RED = {1.f, 0.f, 0.f, 1.f};
const color ORANGE = {1.f, 0.5f, 0.f, 1.f};
const color YELLOW = {1.f, 1.f, 0.f, 1.f};
const color GREEN = {0.f, 1.f, 0.f, 1.f};
const color BLUE = {0.f, 0.f, 1.f, 1.f};
const color PURPLE = {1.f, 0.f, 1.f, 1.f};

const float blockT = 0.015625f;

void nonsense(int32_t lineIQ) { printf("\033[0;31m[qDivLib] Nonsensical Operation at Line %d\033[0;37m\n", lineIQ); }
void debug(int32_t lineIQ) { printf("\033[0;32m[qDivLib] Code Reached at Line %d\033[0;37m\n", lineIQ); }

// Packet
void makeBlockPacket(int32_t inBlock, int32_t inFX, int32_t inFY, int32_t inX, int32_t inY, int32_t inLayer) {
	SendPacket[4] = 0x07;
	blockData dataIQ;
	dataIQ.block = inBlock;
	dataIQ.fldX = inFX;
	dataIQ.fldY = inFY;
	dataIQ.posX = inX;
	dataIQ.posY = inY;
	dataIQ.layer = inLayer;
	memcpy(SendPacket+5, &dataIQ, sizeof(blockData));
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

blockType makeBlock(bool inTraverse, bool inTransparent, bool inLuminant, int32_t inTexX, int32_t inTexY, uint64_t inEnergy, bool inStatic) {
	blockType typeIQ;
	typeIQ.traversable = inTraverse;
	typeIQ.transparent = inTransparent;
	typeIQ.illuminant = inLuminant;
	typeIQ.texX = (float)inTexX * blockT;
	typeIQ.texY = (float)inTexY * blockT;
	typeIQ.qEnergy = inEnergy;
	typeIQ.qEnergyStatic = inStatic;
	return typeIQ;
}

criterionSettings makeCriterionTemplate(int8_t * inDesc) {
	criterionSettings TemplateIQ;
	strcpy(TemplateIQ.desc, inDesc);
	//TemplateIQ.color = inColor;
	return TemplateIQ;
}

bool entityInRange(entityData* entityIQ, entityData* entityAS) {
	return entityIQ -> zone == entityAS -> zone && entityIQ -> fldX < entityAS -> fldX + 2 && entityIQ -> fldX > entityAS -> fldX - 2 && entityIQ -> fldY < entityAS -> fldY + 2 && entityIQ -> fldY > entityAS -> fldY - 2;
}

entitySettings makeEntityType(int32_t inHealth, bool inPersist, int32_t inBox, bool inClip, double inSpeed, uint64_t inEnergy, entityAction inAction) {
	entitySettings typeIQ;
	typeIQ.maxHealth = inHealth;
	typeIQ.persistant = inPersist;
	typeIQ.hitBox = (double)inBox * QDIV_HITBOX_UNIT;
	typeIQ.noClip = inClip;
	typeIQ.speed = inSpeed;
	typeIQ.qEnergy = inEnergy;
	typeIQ.action = inAction;
	return typeIQ;
}

int32_t getOccupiedLayer(int* blockPos) {
	if(blockPos[1] != NO_WALL) {
		return 1;
	}else if(blockPos[0] != NO_FLOOR) {
		return 0;
	}else return -1;
}

bool qEnergyRelevance(uint64_t playerEnergy, blockType* blockIQ) {
	return blockIQ -> qEnergy <= playerEnergy && blockIQ -> qEnergy > playerEnergy * 4 / 5;
}

void makeBlockTypes() {
	// Floor
	block[0][NO_FLOOR] = makeBlock(true, false, false, 0, 0, 0, false);
	block[0][PLASTIC] = makeBlock(true, false, false, 1, 0, 15, true);
	block[0][SHALLAND_FLATGRASS] = makeBlock(true, false, false, 2, 0, 50, false);
	block[0][WATER] = makeBlock(true, false, false, 3, 0, 0, false);
	// Wall
	block[1][NO_WALL] = makeBlock(true, true, false, 0, 0, 0, false);
	block[1][FILLED] = makeBlock(false, false, false, 1, 0, 4, false);
	block[1][LAMP] = makeBlock(true, true, true, 2, 0, 80, false);
	block[1][SHALLAND_GRASS] = makeBlock(true, true, false, 3, 0, 40, false);
	block[1][SHALLAND_BUSH] = makeBlock(true, true, false, 4, 0, 100, false);
}

void makeCriterionTemplates() {
}

void makeRoles() {
	strcpy(role[WARRIOR].name, "Warrior");
	role[WARRIOR].textColor = RED;
	role[WARRIOR].maxArtifact = MAX_WARRIOR_ARTIFACT;
	role[WARRIOR].movement_speed = 1.0;
	role[WARRIOR].mining_speed = 1;
	strcpy(role[EXPLORER].name, "Explorer");
	role[EXPLORER].textColor = ORANGE;
	role[EXPLORER].maxArtifact = MAX_EXPLORER_ARTIFACT;
	role[EXPLORER].movement_speed = 1.5;
	role[EXPLORER].mining_speed = 1;
	strcpy(role[BUILDER].name, "Builder");
	role[BUILDER].textColor = YELLOW;
	role[BUILDER].maxArtifact = MAX_BUILDER_ARTIFACT;
	role[BUILDER].movement_speed = 1.0;
	role[BUILDER].mining_speed = 2;
	strcpy(role[GARDENER].name, "Gardener");
	role[GARDENER].textColor = GREEN;
	role[GARDENER].maxArtifact = MAX_GARDENER_ARTIFACT;
	role[GARDENER].movement_speed = 1.0;
	role[GARDENER].mining_speed = 1;
	strcpy(role[ENGINEER].name, "Engineer");
	role[ENGINEER].textColor = BLUE;
	role[ENGINEER].maxArtifact = MAX_ENGINEER_ARTIFACT;
	role[ENGINEER].movement_speed = 1.0;
	role[ENGINEER].mining_speed = 1;
	strcpy(role[WIZARD].name, "Wizard");
	role[WIZARD].textColor = PURPLE;
	role[WIZARD].maxArtifact = MAX_WIZARD_ARTIFACT;
	role[WIZARD].movement_speed = 1.0;
	role[WIZARD].mining_speed = 1;
}

void libMain() {
	printf("[qDivLib] Loading qDivLib-%d.%c\n", QDIV_VERSION, QDIV_BRANCH);
	memset(SendPacket, 0xFF, 4 * sizeof(int8_t));
	puts("[qDivLib] Adding Block Types");
	makeBlockTypes();
	puts("[qDivLib] Adding Criterion Templates");
	makeCriterionTemplates();
	puts("[qDivLib] Adding Roles");
	makeRoles();
}
