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
#include<limits.h>
#if(CHAR_BIT != 8)
	#error qDiv is not supported on this CPU architecture
#endif
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
#ifdef QDIV_CLIENT
#define QDIV_SPLIT(forClient, forServer) forClient
#endif
#ifdef QDIV_SERVER
#define QDIV_SPLIT(forClient, forServer) forServer
#endif
#ifdef _WIN32
#define QDIV_RANDOM(buffer, bufferSZ) for(int byteSL = 0; byteSL < bufferSZ / 4; byteSL++) rand_s(((unsigned int*)buffer) + byteSL)
#define QDIV_CLOSE(sockIQ) closesocket(sockIQ)
#else
#include<sys/time.h>
#include<sys/random.h>
#define QDIV_RANDOM(buffer, bufferSZ) getrandom((void*)buffer, bufferSZ, 0)
#define QDIV_CLOSE(sockIQ) close(sockIQ)
#endif
#define ECC_CURVE NIST_K571
#include "include/ecdh.h"
#define QDIV_AUTH
#include"elements.h"

const int32_t QDIV_VERSION = 56;
const int8_t QDIV_BRANCH = 'T';

int gettimeofday(struct timeval *__restrict __tv, void *__restrict __tz);
int close(int fd);

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
	OCEAN,
	SHALLAND,
	MAX_BIOME
} biomeEnum;

enum {
	NO_FLOOR,
	PLASTIC,
	SHALLAND_FLATGRASS,
	WATER,
	SAND
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
	NO_CRITERION = -1,
	KILL_SNAIL,
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
	int32_t iteration;
	int32_t frequency;
	int32_t stability;
	int32_t occasion;
} qrng_t;

typedef struct {
	int32_t slot;
	int32_t active;
	int32_t edit;
	int32_t zone;
	int32_t fldX;
	int32_t fldY;
	uint16_t biome[128][128];
	uint16_t block[128][128][2];
} field_t;

typedef struct {
	int32_t fldX;
	int32_t fldY;
	int32_t posX;
	int32_t block[128][2];
} fieldSlice;

typedef struct {
	int32_t zone;
	int32_t lclX;
	int32_t lclY;
	int32_t posX;
} segment_t;

typedef struct {
	bool traversable;
	bool transparent;
	float texX;
	float texY;
	float sizeX;
	float sizeY;
	int32_t illuminance;
	uint64_t qEnergy;
	bool qEnergyStatic;
} block_st;

typedef struct {
	float red;
	float green;
	float blue;
	float alpha;
} color;

typedef struct {
	int8_t desc[16];
	color textColor;
} criterion_st;

typedef struct {
	int8_t name[16];
	color textColor;
	int32_t maxArtifact;
	double movement_speed;
	double mining_factor;
} role_st;

typedef void(*entityAction)(void*);

typedef struct {
	int32_t maxHealth;
	bool persistant;
	double hitBox;
	bool noClip;
	double speed;
	uint64_t qEnergy;
	entityAction action;
} entity_st;

typedef struct {
	int32_t* socket;
	int32_t role;
	double roleTimer;
	int32_t artifact;
	int32_t currentUsage;
	double useTimer;
	double useRelX;
	double useRelY;
	uint32_t* criterion;
} player_t;

typedef struct {
	int8_t name[16];
	uint8_t uuid[16];
	int32_t slot;
	int32_t type;
	int32_t active;
	int32_t zone;
	int32_t fldX;
	int32_t fldY;
	field_t* local[3][3];
	double posX;
	double posY;
	double motX;
	double motY;
	int32_t health;
	double healthTimer;
	float mood;
	uint64_t qEnergy;
	union {
		player_t Player;
	} unique;
} entity_t;

typedef struct {
	int32_t represent;
	int32_t layer;
} placeBlock_st;
typedef struct {
	int32_t decay;
	int32_t range;
} sliceEntity_st;

typedef union {
	placeBlock_st place;
	sliceEntity_st slice;
} uniqueActionSettings;

typedef void(*artifactAction)(field_t*, double, double, entity_t*, player_t*, void*, uniqueActionSettings* settings);
#define def_ArtifactAction(name) void name(field_t* fieldIQ, double posX, double posY, entity_t* entityIQ, player_t* playerIQ, void* artifactVD, uniqueActionSettings* settings)

typedef struct {
	int32_t Template;
	uint32_t value;
} criterion_t;

typedef struct {
	int8_t name[16];
	int8_t desc[256];
	bool crossCriterial;
	uint32_t texture;
	artifactAction primary;
	artifactAction secondary;
	uint64_t primaryCost;
	uint64_t secondaryCost;
	double primaryUseTime;
	double secondaryUseTime;
	uniqueActionSettings primarySettings;
	uniqueActionSettings secondarySettings;
	uint64_t qEnergy;
	criterion_t criterion[QDIV_ARTIFACT_CRITERIA];
} artifactSettings;

typedef struct {
	uint16_t block;
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
} usage_t;

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
// 0x0C Public Key
// 0x0D Identity
// 0x0E Menu

uint8_t recvBF[QDIV_PACKET_SIZE];
uint8_t sendBF[QDIV_PACKET_SIZE];
uint8_t fieldBF[32768];

uint8_t currentHour = 0;

block_st block[2][16384];
criterion_st criterionTemplate[MAX_CRITERION];
role_st role[6];
entity_st entityType[MAX_ENTITY_TYPE];
artifactSettings artifact[6][4000];

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

void nonsense(int32_t lineIQ) { printf("\033[0;31m> Nonsensical Operation at Line %d\033[0;37m\n", lineIQ); }
void debug(int32_t lineIQ) { printf("\033[0;32m> Code Reached at Line %d\033[0;37m\n", lineIQ); }

bool qrngIterate(qrng_t* qrngIQ) {
	if(qrngIQ -> iteration == 0) qrngIQ -> occasion = qrngIQ -> frequency - (rand() % qrngIQ -> stability);
	qrngIQ -> iteration++;
	if(qrngIQ -> iteration >= qrngIQ -> frequency) qrngIQ -> iteration = 0;
	return qrngIQ -> iteration == qrngIQ -> occasion;
}

// Generalist
void sendPacket(uint8_t prefix, void* payload, size_t payloadSZ, int32_t sockIQ) {
	sendBF[4] = prefix;
	if(payload != NULL) memcpy(sendBF+5, payload, payloadSZ);
	send(sockIQ, sendBF, QDIV_PACKET_SIZE, 0);
}

// Packet
void makeBlockPacket(uint16_t inBlock, int32_t inFX, int32_t inFY, int32_t inX, int32_t inY, int32_t inLayer) {
	sendBF[4] = 0x07;
	blockData dataIQ;
	dataIQ.block = inBlock;
	dataIQ.fldX = inFX;
	dataIQ.fldY = inFY;
	dataIQ.posX = inX;
	dataIQ.posY = inY;
	dataIQ.layer = inLayer;
	memcpy(sendBF+5, &dataIQ, sizeof(blockData));
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

block_st makeBlock(bool inTraverse, bool inTransparent, int32_t inTexX, int32_t inTexY, int32_t inSizeX, int32_t inSizeY, int32_t inIlluminance, uint64_t inEnergy, bool inStatic) {
	block_st typeIQ;
	typeIQ.traversable = inTraverse;
	typeIQ.transparent = inTransparent;
	typeIQ.texX = (float)inTexX * blockT;
	typeIQ.texY = (float)inTexY * blockT;
	typeIQ.sizeX = (float)inSizeX * blockT;
	typeIQ.sizeY = (float)inSizeY * blockT;
	typeIQ.illuminance = inIlluminance;
	typeIQ.qEnergy = inEnergy;
	typeIQ.qEnergyStatic = inStatic;
	return typeIQ;
}

criterion_st makeCriterionTemplate(int8_t* inDesc, color inColor) {
	criterion_st TemplateIQ;
	strcpy(TemplateIQ.desc, inDesc);
	TemplateIQ.textColor = inColor;
	return TemplateIQ;
}

bool entityInRange(entity_t* entityIQ, entity_t* entityAS) {
	return entityIQ -> zone == entityAS -> zone && entityIQ -> fldX < entityAS -> fldX + 2 && entityIQ -> fldX > entityAS -> fldX - 2 && entityIQ -> fldY < entityAS -> fldY + 2 && entityIQ -> fldY > entityAS -> fldY - 2;
}

entity_st makeEntityType(int32_t inHealth, bool inPersist, int32_t inBox, bool inClip, double inSpeed, uint64_t inEnergy, entityAction inAction) {
	entity_st typeIQ;
	typeIQ.maxHealth = inHealth;
	typeIQ.persistant = inPersist;
	typeIQ.hitBox = (double)inBox * QDIV_HITBOX_UNIT;
	typeIQ.noClip = inClip;
	typeIQ.speed = inSpeed;
	typeIQ.qEnergy = inEnergy;
	typeIQ.action = inAction;
	return typeIQ;
}

int32_t getOccupiedLayer(uint16_t* blockPos) {
	if(blockPos[1] != NO_WALL) {
		return 1;
	}else if(blockPos[0] != NO_FLOOR) {
		return 0;
	}else return -1;
}

bool qEnergyRelevance(uint64_t playerEnergy, block_st* blockIQ) {
	return blockIQ -> qEnergy <= playerEnergy && blockIQ -> qEnergy > playerEnergy * 4 / 5;
}

void makeBlocks() {
	// Floor
	block[0][NO_FLOOR] = makeBlock(true, false, 0, 0, 1, 1, 0, 0, false);
	block[0][PLASTIC] = makeBlock(true, false, 1, 0, 1, 1, 0, 15, true);
	block[0][SHALLAND_FLATGRASS] = makeBlock(true, false, 2, 0, 1, 1, 0, 50, false);
	block[0][WATER] = makeBlock(true, false, 3, 0, 1, 1, 0, 0, false);
	block[0][SAND] = makeBlock(true, false, 4, 0, 1, 1, 0, 50, false);
	// Wall
	block[1][NO_WALL] = makeBlock(true, true, 0, 0, 1, 1, 0, 0, false);
	block[1][FILLED] = makeBlock(false, false, 1, 0, 1, 1, 0, 4, false);
	block[1][LAMP] = makeBlock(true, true, 2, 0, 1, 1, 8, 80, false);
	block[1][SHALLAND_GRASS] = makeBlock(true, true, 3, 0, 1, 1, 0, 40, false);
	block[1][SHALLAND_BUSH] = makeBlock(true, true, 4, 0, 1, 2, 0, 100, false);
}

void makeCriterionTemplates() {
	criterionTemplate[KILL_SNAIL] = makeCriterionTemplate("Snails Killed", RED);
}

int32_t roleEX;
artifactSettings* artifactEX;

#ifdef QDIV_CLIENT
void loadTexture(uint32_t* texture, const uint8_t* file, size_t fileSZ);
#endif

void makeArtifact4(int32_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, double inPrimaryTime, artifactAction inSecondary, uint64_t inSecondaryCost, double inSecondaryTime, uint64_t inEnergy, int32_t Template1, uint32_t value1, int32_t Template2, uint32_t value2, int32_t Template3, uint32_t value3, int32_t Template4, uint32_t value4) {
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

void makeArtifact3(int32_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, double inPrimaryTime, artifactAction inSecondary, uint64_t inSecondaryCost, double inSecondaryTime, uint64_t inEnergy, int32_t Template1, uint32_t value1, int32_t Template2, uint32_t value2, int32_t Template3, uint32_t value3) {
	makeArtifact4(artifactSL, inName, inDesc, inTexture, inTextureSZ, inCross, inPrimary, inPrimaryCost, inPrimaryTime, inSecondary, inSecondaryCost, inSecondaryTime, inEnergy, Template1, value1, Template2, value2, Template3, value3, NO_CRITERION, 0);
}

void makeArtifact2(int32_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, double inPrimaryTime, artifactAction inSecondary, uint64_t inSecondaryCost, double inSecondaryTime, uint64_t inEnergy, int32_t Template1, uint32_t value1, int32_t Template2, uint32_t value2) {
	makeArtifact4(artifactSL, inName, inDesc, inTexture, inTextureSZ, inCross, inPrimary, inPrimaryCost, inPrimaryTime, inSecondary, inSecondaryCost, inSecondaryTime, inEnergy, Template1, value1, Template2, value2, NO_CRITERION, 0, NO_CRITERION, 0);
}

void makeArtifact1(int32_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, double inPrimaryTime, artifactAction inSecondary, uint64_t inSecondaryCost, double inSecondaryTime, uint64_t inEnergy, int32_t Template1, uint32_t value1) {
	makeArtifact4(artifactSL, inName, inDesc, inTexture, inTextureSZ, inCross, inPrimary, inPrimaryCost, inPrimaryTime, inSecondary, inSecondaryCost, inSecondaryTime, inEnergy, Template1, value1, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0);
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

void makeArtifacts() {
	roleEX = WARRIOR;
	makeArtifact1(OLD_SLICER, "Old Slicer", "Use this to defeat your\nfirst few enemies.", QDIV_SPLIT(artifacts_old_slicer_png, NULL), QDIV_SPLIT(artifacts_old_slicer_pngSZ, 0), false, QDIV_SPLIT(&simpleSwing, &sliceEntity), 0, 0.5, NULL, 0, 0, 0, NO_CRITERION, 0);
	artifactEX -> primarySettings.slice.decay = 5;
	roleEX = EXPLORER;
	roleEX = BUILDER;
	makeArtifact1(OLD_SWINGER, "Old Swinger", "Use this to break your\nfirst few blocks and\nget your first qEnergy.", QDIV_SPLIT(artifacts_old_swinger_png, NULL), QDIV_SPLIT(artifacts_old_swinger_pngSZ, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock), 0, 10, NULL, 0, 0, 0, NO_CRITERION, 0);
	artifactEX -> primarySettings.slice.decay = 0;
	makeArtifact1(BLOCK, "Block", "First ever artifact of qDiv!\nCan be placed.", QDIV_SPLIT(artifacts_block_png, NULL), QDIV_SPLIT(artifacts_block_pngSZ, 0), false, QDIV_SPLIT(NULL, &placeBlock), 0, 0.1, NULL, 0, 0, 5, NO_CRITERION, 0);
	artifactEX -> primarySettings.place.represent = FILLED;
	artifactEX -> primarySettings.place.layer = 1;
	makeArtifact2(PLASTIC_ARTIFACT, "Plastic", "Just a test.", QDIV_SPLIT(artifacts_plastic_png, NULL), QDIV_SPLIT(artifacts_plastic_pngSZ, 0), false, QDIV_SPLIT(NULL, &placeBlock), 0, 0.1, NULL, 0, 0, 15, NO_CRITERION, 0, KILL_SNAIL, 4);
	artifactEX -> primarySettings.place.represent = PLASTIC;
	artifactEX -> primarySettings.place.layer = 0;
	makeArtifact1(LAMP_ARTIFACT, "Block", "Just a test.", QDIV_SPLIT(artifacts_lamp_png, NULL), QDIV_SPLIT(artifacts_lamp_pngSZ, 0), false, QDIV_SPLIT(NULL, &placeBlock), 0, 0.1, NULL, 0, 0, 80, NO_CRITERION, 0);
	artifactEX -> primarySettings.place.represent = LAMP;
	artifactEX -> primarySettings.place.layer = 1;
	roleEX = GARDENER;
	roleEX = ENGINEER;
	roleEX = WIZARD;
}

bool isArtifactUnlocked(int32_t roleSL, int32_t artifactSL, entity_t* entityIQ) {
	if(entityIQ -> qEnergy < artifact[roleSL][artifactSL].qEnergy ||
	(entityIQ -> unique.Player.role != roleSL && entityIQ -> unique.Player.roleTimer > 0)) return false;
	for(int32_t criterionSL = 0; criterionSL < QDIV_ARTIFACT_CRITERIA; criterionSL++) {
		int32_t TemplateSL = artifact[roleSL][artifactSL].criterion[criterionSL].Template;
		if(TemplateSL != NO_CRITERION && artifact[roleSL][artifactSL].criterion[criterionSL].value > entityIQ -> unique.Player.criterion[TemplateSL]) return false;
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
