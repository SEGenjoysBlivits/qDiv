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
#include<stdbool.h>
#include<string.h>
#include<math.h>
#define QDIV_PORT 38652
#define QDIV_MAX_PLAYERS 64
#define QDIV_MAX_FIELDS 576
#define QDIV_PACKET_SIZE 1500
#define QDIV_UNIQUE_ENTITY 9
#define QDIV_ARTIFACT_CRITERIA 4
#define FOR_EVERY_ENTITY for(int entitySL = 0; entitySL < 10000; entitySL++)
#define DIAGONAL_DOWN_FACTOR 0.7071
#define DIAGONAL_UP_FACTOR 1.4142
#define QDIV_HITBOX_UNIT 0.4

const int QDIV_VERSION = 37;
const char QDIV_BRANCH = 'T';

int clampInt(int valMin, int valIQ, int valMax) {
	bool valBL = valMin <= valIQ && valIQ <= valMax;
	return valIQ * valBL + valMin * (valIQ < valMin) + valMax * (valMax < valIQ);
}

enum {
	OFFLINE_NET,
	PROGRESS_NET,
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

typedef struct {
	int slot;
	int active;
	int edit;
	int zone;
	int fldX;
	int fldY;
	int block[128][128][2];
} fieldData;

typedef struct {
	int fldX;
	int fldY;
	int posX;
	int block[128][2];
} fieldSlice;

typedef struct {
	bool traversable;
	bool transparent;
	bool illuminant;
	float texX;
	float texY;
	unsigned long long qEnergy;
	bool qEnergyStatic;
} blockType;

typedef struct {
	float red;
	float green;
	float blue;
	float alpha;
} color;

typedef struct {
	char desc[16];
	color textColor;
} criterionSettings;

typedef struct {
	char name[16];
	color textColor;
	int maxArtifact;
	double movement_speed;
	int mining_speed;
} roleSettings;

typedef void(*entityAction)(void*);

typedef struct {
	int maxHealth;
	bool persistant;
	double hitBox;
	bool noClip;
	double speed;
	unsigned long long qEnergy;
	entityAction action;
} entitySettings;

typedef struct {
	char uuid[16];
	int socket;
	int role;
	double roleTimer;
	int artifact;
	int currentUsage;
	double useTimer;
	double useRelX;
	double useRelY;
	unsigned int* criterion;
} playerData;

typedef struct {
	int slot;
	int type;
	int active;
	int zone;
	int fldX;
	int fldY;
	fieldData* local[3][3];
	double posX;
	double posY;
	double motX;
	double motY;
	int health;
	double healthTimer;
	float mood;
	unsigned long long qEnergy;
	char name[16];
	union {
		playerData Player;
	} unique;
} entityData;

typedef struct {
	int block;
	int fldX;
	int fldY;
	int posX;
	int posY;
	int layer;
} blockData;

typedef struct {
	int usage;
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

char ReceivePacket[QDIV_PACKET_SIZE];
char SendPacket[QDIV_PACKET_SIZE];

int currentHour = 0;

blockType block[2][4096];
criterionSettings criterionTemplate[MAX_CRITERION];
roleSettings role[6];
entitySettings entityType[MAX_ENTITY_TYPE];

entityData entity[10000];
int entitySlot[10000] = {0};

const color RED = {1.f, 0.f, 0.f, 1.f};
const color ORANGE = {1.f, 0.5f, 0.f, 1.f};
const color YELLOW = {1.f, 1.f, 0.f, 1.f};
const color GREEN = {0.f, 1.f, 0.f, 1.f};
const color BLUE = {0.f, 0.f, 1.f, 1.f};
const color PURPLE = {1.f, 0.f, 1.f, 1.f};

const float blockT = 0.015625f;

void nonsense(int lineIQ) { printf("\033[0;31m[qDivLib] Nonsensical Operation at Line %d\033[0;37m\n", lineIQ); }
void debug(int lineIQ) { printf("\033[0;32m[qDivLib] Code Reached at Line %d\033[0;37m\n", lineIQ); }

// Packet
void makeBlockPacket(int inBlock, int inFX, int inFY, int inX, int inY, int inLayer) {
	SendPacket[0] = 0x07;
	blockData dataIQ;
	dataIQ.block = inBlock;
	dataIQ.fldX = inFX;
	dataIQ.fldY = inFY;
	dataIQ.posX = inX;
	dataIQ.posY = inY;
	dataIQ.layer = inLayer;
	memcpy(SendPacket+1, &dataIQ, sizeof(blockData));
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

blockType makeBlock(bool inTraverse, bool inTransparent, bool inLuminant, int inTexX, int inTexY, unsigned long long inEnergy, bool inStatic) {
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

criterionSettings makeCriterionTemplate(char* inDesc) {
	criterionSettings TemplateIQ;
	strcpy(TemplateIQ.desc, inDesc);
	//TemplateIQ.color = inColor;
	return TemplateIQ;
}

bool entityInRange(entityData* entityIQ, entityData* entityAS) {
	return entityIQ -> zone == entityAS -> zone && entityIQ -> fldX < entityAS -> fldX + 2 && entityIQ -> fldX > entityAS -> fldX - 2 && entityIQ -> fldY < entityAS -> fldY + 2 && entityIQ -> fldY > entityAS -> fldY - 2;
}

entitySettings makeEntityType(int inHealth, bool inPersist, int inBox, bool inClip, double inSpeed, unsigned long long inEnergy, entityAction inAction) {
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

int getOccupiedLayer(int* blockPos) {
	if(blockPos[1] != NO_WALL) {
		return 1;
	}else if(blockPos[0] != NO_FLOOR) {
		return 0;
	}else return -1;
}

bool qEnergyRelevance(unsigned long long playerEnergy, blockType* blockIQ) {
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
	puts("[qDivLib] Adding Block Types");
	makeBlockTypes();
	puts("[qDivLib] Adding Criterion Templates");
	makeCriterionTemplates();
	puts("[qDivLib] Adding Roles");
	makeRoles();
}
