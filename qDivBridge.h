#pragma once
#include<limits.h>
#if(CHAR_BIT != 8)
	#error qDiv is not supported on this CPU architecture
#endif
#include "qDivEnum.h"
#include "qDivType.h"
#include "qDivLang.h"
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
#define QDIV_SUBDIAG -0.3
#define QDIV_HITBOX_UNIT 0.25
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
#define QDIV_AUTH
#include "elements.h"
#include "QSM.h"

const int32_t QDIV_VERSION = 79;
const int8_t QDIV_BRANCH = 'T';

const size_t TRUNCATED_ENTITY = sizeof(entity_t) - ((sizeof(void*) * 11) + (sizeof(bool)) + (sizeof(float) * QDIV_MAX_EFFECT));

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

float qFactor;

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
// 0x0C Entity Update with full Field reloading
// 0x0D Effect Update

uint8_t currentHour = 0;

block_st block[2][16384];
criterion_st criterionTemplate[MAX_CRITERION];
role_st role[6];
entity_st entityType[MAX_ENTITY_TYPE];
artifact_st artifact[6][4000];
effect_st effect[QDIV_MAX_EFFECT];

entity_t entity[10000];
bool entityTable[10000] = {false};

const color_t RED = {1.f, 0.f, 0.f, 1.f};
const color_t ORANGE = {1.f, 0.5f, 0.f, 1.f};
const color_t YELLOW = {1.f, 1.f, 0.f, 1.f};
const color_t GREEN = {0.f, 0.75f, 0.f, 1.f};
const color_t BLUE = {0.f, 0.1f, 1.f, 1.f};
const color_t PURPLE = {0.75f, 0.f, 1.f, 1.f};
const color_t GRAY = {0.5f, 0.5f, 0.5f, 1.f};

const float blockT = 0.0078125f;

collection_st cropPlaceable = {1, {SOIL}};
collection_st aridisLootTable = {1, {ACQUIRE_WOOL}};
collection_st unbreakableBlock = {2, {FLUID, ZONE_PORTAL}};
collection_st woodCriteria = {4, {MINE_SHALLAND_BUSH, MINE_ARIDIS_BUSH, MINE_REDWOOD_TREE, MINE_COFFEE_BUSH}};

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

void derelativize(int* lclX, int* lclY, float* posX, float* posY) {
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

criterion_st makeCriterionTemplate(int8_t* inDesc, color_t inColor) {
	criterion_st TemplateIQ;
	strcpy(TemplateIQ.desc, inDesc);
	TemplateIQ.textColor = inColor;
	return TemplateIQ;
}

bool entityInRange(entity_t* entityIQ, entity_t* entityAS) {
	return entityIQ -> zone == entityAS -> zone && entityIQ -> fldX < entityAS -> fldX + 2 && entityIQ -> fldX > entityAS -> fldX - 2 && entityIQ -> fldY < entityAS -> fldY + 2 && entityIQ -> fldY > entityAS -> fldY - 2 && entityIQ -> posX >= -128 && entityIQ -> posX < 256 && entityIQ -> posY >= -128 && entityIQ -> posY < 256;
}

entity_st* typeEX;

void makeEntity(qint_t typeIQ, int32_t maxHealth, uint64_t qEnergy, bool persistent, int32_t hitBox, bool noClip, bool proj, float despawnTM, float speed, int32_t priority, entity_fp action) {
	typeEX = entityType + typeIQ;
	typeEX -> maxHealth = maxHealth;
	typeEX -> qEnergy = qEnergy;
	typeEX -> decay = 0;
	typeEX -> persistent = persistent;
	typeEX -> hitBox = (float)hitBox * QDIV_HITBOX_UNIT;
	typeEX -> noClip = noClip;
	typeEX -> proj = proj;
	typeEX -> despawnTM = despawnTM;
	typeEX -> speed = speed;
	typeEX -> priority = priority;
	memset(typeEX -> texture, 0x00, sizeof(typeEX -> texture));
	typeEX -> action = action;
}

// Renderers
void minimum(entity_t* entityVD);
void staticBobbing(entity_t* entityVD);
void flippingBobbing(entity_t* entityVD);
void sheepRenderer(entity_t* entityVD);
void rotatingWiggling(entity_t* entityVD);
void wispRenderer(entity_t* entityVD);
void starRenderer(entity_t* entityVD);

// Actions
void playerAction(entity_t* entityVD);
void snailAction(entity_t* entityVD);
void hostileAction(entity_t* entityVD);
void noClipHostileAction(entity_t* entityVD);
void friendAction(entity_t* entityVD);
void wispAction(entity_t* entityVD);
void damagerAction(entity_t* entityVD);

#ifdef QDIV_CLIENT
void loadTexture(uint32_t* texture, const uint8_t* file, size_t fileSZ);
#endif

#ifdef QDIV_CLIENT
	#define QDIV_ENTITY_TEXTURE(textureSL, file, fileSZ) loadTexture(typeEX -> texture + textureSL, file, fileSZ);
#else
	#define QDIV_ENTITY_TEXTURE(textureSL, file, fileSZ)
#endif

void makeEntities() {
	makeEntity(NULL_ENTITY, 0, 0, false, 0, false, false, -1, 0.0, 1, NULL);
	makeEntity(PLAYER, 5, 1, true, 3, false, false, -1, 2.0, 1, QDIV_SPLIT(&staticBobbing, &playerAction, NULL));
		QDIV_ENTITY_TEXTURE(0, player0_png, player0_pngSZ);
		QDIV_ENTITY_TEXTURE(1, player1_png, player1_pngSZ);
	makeEntity(SHALLAND_SNAIL, 5, 50, false, 6, false, false, 60, 0.25, 1, QDIV_SPLIT(&flippingBobbing, &snailAction, NULL));
		QDIV_ENTITY_TEXTURE(0, shalland_snail0_png, shalland_snail0_pngSZ);
		QDIV_ENTITY_TEXTURE(1, shalland_snail1_png, shalland_snail1_pngSZ);
	makeEntity(CALCIUM_CRAWLER, 10, 25, false, 3, false, false, 300, 1.9, 1, QDIV_SPLIT(&rotatingWiggling, &snailAction, NULL));
		QDIV_ENTITY_TEXTURE(0, calcium_crawler0_png, calcium_crawler0_pngSZ);
		QDIV_ENTITY_TEXTURE(1, calcium_crawler1_png, calcium_crawler1_pngSZ);
	makeEntity(KOBATINE_SNAIL, 5, 50, false, 6, false, false, 60, 0.25, 1, QDIV_SPLIT(&flippingBobbing, &snailAction, NULL));
		QDIV_ENTITY_TEXTURE(0, kobatine_snail0_png, kobatine_snail0_pngSZ);
		QDIV_ENTITY_TEXTURE(1, kobatine_snail1_png, kobatine_snail1_pngSZ);
	makeEntity(KOBATINE_BUG, 10, 50, false, 3, false, false, 300, 1.9, 1, QDIV_SPLIT(&rotatingWiggling, &noClipHostileAction, NULL));
		QDIV_ENTITY_TEXTURE(0, kobatine_bug0_png, kobatine_bug0_pngSZ);
		QDIV_ENTITY_TEXTURE(1, kobatine_bug1_png, kobatine_bug1_pngSZ);
	makeEntity(WILD_SHEEP, 5, 10, false, 3, false, false, 60, 1.5, 1, QDIV_SPLIT(&sheepRenderer, &friendAction, NULL));
		QDIV_ENTITY_TEXTURE(0, sheep0_png, sheep0_pngSZ);
		QDIV_ENTITY_TEXTURE(1, sheep1_png, sheep1_pngSZ);
	makeEntity(SHEEP, 5, 10, true, 3, false, false, 60, 1.5, 1, QDIV_SPLIT(&sheepRenderer, &friendAction, NULL));
		QDIV_ENTITY_TEXTURE(0, sheep0_png, sheep0_pngSZ);
		QDIV_ENTITY_TEXTURE(1, sheep1_png, sheep1_pngSZ);
	makeEntity(WISP, -1, 0, false, 6, false, false, 0, 0.0, 1, QDIV_SPLIT(&wispRenderer, &wispAction, NULL));
		QDIV_ENTITY_TEXTURE(0, wisp0_png, wisp0_pngSZ);
		QDIV_ENTITY_TEXTURE(1, wisp1_png, wisp1_pngSZ);
		QDIV_ENTITY_TEXTURE(2, wisp2_png, wisp2_pngSZ);
		QDIV_ENTITY_TEXTURE(3, wisp3_png, wisp3_pngSZ);
	makeEntity(ALUMINIUM_STAR, -1, 0, false, 1, false, true, 2, 24.0, 1, QDIV_SPLIT(&starRenderer, &damagerAction, NULL));
		QDIV_ENTITY_TEXTURE(0, artifacts_aluminium_star_png, artifacts_aluminium_star_pngSZ);
	makeEntity(NICKEL_STAR, -1, 0, false, 1, false, true, 2, 24.0, 1, QDIV_SPLIT(&starRenderer, &damagerAction, NULL));
		QDIV_ENTITY_TEXTURE(0, artifacts_nickel_star_png, artifacts_nickel_star_pngSZ);
	makeEntity(SILICON_STAR, -1, 0, false, 1, false, true, 2, 24.0, 1, QDIV_SPLIT(&starRenderer, &damagerAction, NULL));
		QDIV_ENTITY_TEXTURE(0, artifacts_silicon_star_png, artifacts_silicon_star_pngSZ);
}

int32_t getOccupiedLayer(uint16_t* blockPos) {
	if(blockPos[1] != NO_WALL) {
		return 1;
	}else if(blockPos[0] != NO_FLOOR) {
		return 0;
	}else return -1;
}

bool qEnergyRelevance(uint64_t playerEnergy, uint64_t value) {
	uint64_t actualEnergy = playerEnergy * (playerEnergy > 100) + 100 * (playerEnergy <= 100);
	return value <= actualEnergy && value > playerEnergy * 4 / 5;
}

bool qEnergyConsume(uint64_t energyIQ, entity_t* entityIQ) {
	if(energyIQ > entityIQ -> qEnergy -1) return false;
	entityIQ -> qEnergy -= energyIQ;
	return true;
}

int32_t layerEX;
block_st* blockEX;

void makeBlock(uint16_t blockIQ, float inFriction, bool inTransparent, int32_t inTexX, int32_t inTexY, int32_t inSizeX, int32_t inSizeY, const uint8_t* inMineSound, size_t inMineSoundSZ, uint64_t inEnergy, collection_st* inPlaceable, int32_t inType, blockAction inUsage, int32_t inMine) {
	blockEX = &block[layerEX][blockIQ];
	blockEX -> friction = inFriction;
	blockEX -> transparent = inTransparent;
	blockEX -> texX = (float)inTexX * blockT;
	blockEX -> texY = (float)inTexY * blockT;
	blockEX -> sizeX = (float)inSizeX * blockT;
	blockEX -> sizeY = (float)inSizeY * blockT;
	blockEX -> direction = NORTH;
	blockEX -> mine_sound = inMineSound;
	blockEX -> mine_soundSZ = inMineSoundSZ;
	blockEX -> light.range = 1;
	blockEX -> light.red = 0.f;
	blockEX -> light.green = 0.f;
	blockEX -> light.blue = 0.f;
	blockEX -> qEnergy = inEnergy;
	blockEX -> placeable = inPlaceable;
	blockEX -> subType = inType;
	blockEX -> usage = inUsage;
	blockEX -> stepOn = NULL;
	blockEX -> mine_criterion = inMine;
}

def_BlockAction(igniteLamp);
def_BlockAction(stepDamage);
def_BlockAction(harvestCrop);
def_BlockAction(stepTrap);

#define QDIV_SET_LIGHT(inRange, inRed, inGreen, inBlue) {\
	blockEX -> light.range = inRange;\
	blockEX -> light.red = inRed;\
	blockEX -> light.green = inGreen;\
	blockEX -> light.blue = inBlue;\
}

void makeFloor(uint16_t blockIQ, float inFriction, int32_t inTexX, int32_t inTexY, const uint8_t* inMineSound, size_t inMineSoundSZ, uint64_t inEnergy, int32_t inType, int32_t inMine) {
	makeBlock(blockIQ, inFriction, false, inTexX, inTexY, 1, 1, inMineSound, inMineSoundSZ, inEnergy, NULL, inType, NULL, inMine);
}

void makePortal(uint16_t blockIQ, int32_t inDest, int8_t* inText) {
	makeBlock(blockIQ, 1.0, true, 18, 0, 2, 2, NULL, 0, 0, NULL, ZONE_PORTAL, NULL, NO_CRITERION);
	blockEX -> sub.portal.destination = inDest;
	strcpy(blockEX -> sub.portal.hoverText, inText);
		QDIV_SET_LIGHT(3, 1.f, 0.8f, 1.f);
}

void makeLootbox(uint16_t blockIQ, int8_t* inText, collection_st* inLootTable, int32_t inLootAmount) {
	makeBlock(blockIQ, 1.0, true, 26, 0, 1, 1, NULL, 0, 0, NULL, LOOTBOX, NULL, NO_CRITERION);
	strcpy(blockEX -> sub.lootbox.hoverText, inText);
	blockEX -> sub.lootbox.lootTable = inLootTable;
	blockEX -> sub.lootbox.lootAmount = inLootAmount;
}

void makeBlocks() {
	// Floor
	layerEX = 0;
	makeFloor(NO_FLOOR, 1.0, 0, 0, NULL, 0, 0, NO_TYPE_ST, NO_CRITERION);
	makeFloor(SHALLAND_FLOOR, 1.0, 1, 0, NULL, 0, 60, NO_TYPE_ST, NO_CRITERION);
	makeFloor(SHALLAND_FLATGRASS, 1.0, 2, 0, NULL, 0, 40, FERTILE, NO_CRITERION);
	makeFloor(WATER, 0.5, 3, 0, NULL, 0, 0, FLUID, NO_CRITERION);
	makeFloor(SAND, 0.9, 4, 0, NULL, 0, 42, NO_TYPE_ST, MINE_SAND);
	makeFloor(ARIDIS_FLATGRASS, 1.0,  5, 0, NULL, 0, 40, FERTILE, NO_CRITERION);
	makeFloor(ARIDIS_FLOOR, 1.0, 6, 0, NULL, 0, 60, NO_TYPE_ST, NO_CRITERION);
	makeFloor(REDWOOD_FLATGRASS, 1.0, 7, 0, NULL, 0, 40, FERTILE, NO_CRITERION);
	makeFloor(REDWOOD_FLOOR, 1.0, 8, 0, NULL, 0, 60, NO_TYPE_ST, NO_CRITERION);
	makeFloor(SOIL, 1.0, 9, 0, NULL, 0, 35, NO_TYPE_ST, NO_CRITERION);
	makeFloor(LIMESTONE_FLOOR, 1.0, 10, 0, NULL, 0, 100, NO_TYPE_ST, MINE_LIMESTONE);
	makeFloor(SANDSTONE_FLOOR, 1.0, 11, 0, NULL, 0, 100, NO_TYPE_ST, NO_CRITERION);
	makeFloor(KOBATINE_FLOOR, 1.0, 12, 0, NULL, 0, 110, NO_TYPE_ST, MINE_KOBATINE_ROCK);
	makeFloor(KOBATINE_TILES, 1.0, 13, 0, NULL, 0, 110, NO_TYPE_ST, NO_CRITERION);
	makeFloor(TROPICAL_FLATGRASS, 1.0, 14, 0, NULL, 0, 40, FERTILE, NO_CRITERION);
	makeFloor(COFFEE_FLOOR, 1.0, 15, 0, NULL, 0, 60, NO_TYPE_ST, NO_CRITERION);
	makeFloor(LAVA, 1.0, 16, 0, NULL, 0, 0, FLUID, NO_CRITERION);
		QDIV_SET_LIGHT(6, 1.f, 0.5f, 0.f);
		blockEX -> sub.decay = 10;
		blockEX -> stepOn = QDIV_SPLIT(NULL, &stepDamage, NULL);
	makeFloor(BASALT_FLOOR, 1.0, 17, 0, NULL, 0, 0, NO_TYPE_ST, MINE_BASALT);
	makeFloor(CLAY, 1.0, 18, 0, NULL, 0, 0, NO_TYPE_ST, MINE_CLAY);
	// Wall
	layerEX = 1;
	makeBlock(NO_WALL, 1.0, true, 0, 0, 1, 1, NULL, 0, 0, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(SHALLAND_WALL, 0.9, false, 1, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 60, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(SHALLAND_GRASS, 1.0, true, 3, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 40, NULL, NO_TYPE_ST, NULL, MINE_SHALLAND_GRASS);
	makeBlock(SHALLAND_BUSH, 1.0, true, 4, 0, 1, 2, NULL, 0, 75, NULL, NO_TYPE_ST, NULL, MINE_SHALLAND_BUSH);
	makeBlock(ARIDIS_GRASS, 1.0, true, 5, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 40, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(ARIDIS_BUSH, 1.0, true, 6, 0, 1, 3, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 75, NULL, NO_TYPE_ST, NULL, MINE_ARIDIS_BUSH);
	makeBlock(ARIDIS_WALL, 0.9, false, 7, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 60, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(AGAVE, 0.9, true, 8, 0, 2, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 250, NULL, NO_TYPE_ST, NULL, MINE_AGAVE);
	makeBlock(REDWOOD_WEEDS, 1.0, true, 10, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 40, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(REDWOOD_TREE, 0.9, true, 11, 0, 2, 4, NULL, 0, 90, NULL, NO_TYPE_ST, NULL, MINE_REDWOOD_TREE);
	makeBlock(REDWOOD_LOG, 1.0, true, 13, 0, 2, 1, NULL, 0, 60, NULL, NO_TYPE_ST, NULL, MINE_REDWOOD_LOG);
	makeBlock(REDWOOD_WALL, 0.9, false, 15, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 60, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(IMMINENT_POTATO, 1.0, true, 16, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 35, &cropPlaceable, TIME_CHECK, NULL, NO_CRITERION);
		blockEX -> sub.timeCheck.duration = 900;
		blockEX -> sub.timeCheck.successor = POTATO;
	makeBlock(POTATO, 1.0, true, 17, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 80, NULL, DECOMPOSING, QDIV_SPLIT(NULL, &harvestCrop, NULL), NO_CRITERION);
		blockEX -> sub.decomposite = IMMINENT_POTATO;
	makePortal(OVERWORLD_PORTAL, OVERWORLD, "Overworld");
	makePortal(CAVERN_PORTAL, CAVERN, "Cavern");
	makeBlock(LIMESTONE_WALL, 0.9, false, 20, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 100, NULL, NO_TYPE_ST, NULL, MINE_LIMESTONE);
	makeBlock(STALAGMITE, 1.0, true, 21, 0, 1, 1, NULL, 0, 90, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(LIMESTONE_BRICKS, 0.9, false, 22, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 100, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(BRONZE_CLUMP, 0.9, true, 23, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 120, NULL, NO_TYPE_ST, NULL, MINE_BRONZE);
	makeBlock(ALUMINIUM_CLUMP, 0.9, true, 24, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 140, NULL, NO_TYPE_ST, NULL, MINE_ALUMINIUM);
	makeBlock(IRON_CLUMP, 0.9, true, 25, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 175, NULL, NO_TYPE_ST, NULL, MINE_IRON);
	makeLootbox(ARIDIS_LOOTBOX, "Iron Key", &aridisLootTable, 4);
	makeBlock(COBALT_CLUMP, 0.9, true, 27, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 210, NULL, NO_TYPE_ST, NULL, MINE_COBALT);
	makeBlock(NICKEL_CLUMP, 0.9, true, 28, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 250, NULL, NO_TYPE_ST, NULL, MINE_NICKEL);
	makeBlock(SANDSTONE_WALL, 0.9, false, 29, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 100, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(KOBATINE_WALL, 0.9, false, 30, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 110, NULL, NO_TYPE_ST, NULL, MINE_KOBATINE_ROCK);
	makeBlock(KOBATINE_MUSHROOM, 1.0, true, 31, 0, 1, 1, NULL, 0, 50, NULL, NO_TYPE_ST, NULL, MINE_KOBATINE_MUSHROOM);
		QDIV_SET_LIGHT(3, 0.25f, 0.25f, 1.f);
	makeBlock(LIMESTONE_LAMP, 0.9, true, 32, 0, 1, 1, NULL, 0, 100, NULL, NO_TYPE_ST, QDIV_SPLIT(NULL, &igniteLamp, NULL), NO_CRITERION);
		blockEX -> sub.litSelf = LIT_LIMESTONE_LAMP;
	makeBlock(LIT_LIMESTONE_LAMP, 0.9, true, 33, 0, 1, 1, NULL, 0, 100, NULL, TIME_CHECK, NULL, NO_CRITERION);
		QDIV_SET_LIGHT(5, 1.f, 1.f, 1.f);
		blockEX -> sub.timeCheck.duration = 1800;
		blockEX -> sub.timeCheck.successor = LIMESTONE_LAMP;
	makeBlock(SANDSTONE_LAMP, 0.9, true, 34, 0, 1, 1, NULL, 0, 42, NULL, NO_TYPE_ST, QDIV_SPLIT(NULL, &igniteLamp, NULL), NO_CRITERION);
		blockEX -> sub.litSelf = LIT_SANDSTONE_LAMP;
	makeBlock(LIT_SANDSTONE_LAMP, 0.9, true, 35, 0, 1, 1, NULL, 0, 42, NULL, TIME_CHECK, NULL, NO_CRITERION);
		QDIV_SET_LIGHT(5, 1.f, 1.f, 0.75f);
		blockEX -> sub.timeCheck.duration = 1800;
		blockEX -> sub.timeCheck.successor = SANDSTONE_LAMP;
	makeBlock(KOBATINE_LAMP, 0.9, true, 36, 0, 1, 1, NULL, 0, 110, NULL, NO_TYPE_ST, QDIV_SPLIT(NULL, &igniteLamp, NULL), NO_CRITERION);
		blockEX -> sub.litSelf = LIT_KOBATINE_LAMP;
	makeBlock(LIT_KOBATINE_LAMP, 0.9, true, 37, 0, 1, 1, NULL, 0, 110, NULL, TIME_CHECK, NULL, NO_CRITERION);
		QDIV_SET_LIGHT(5, 0.5f, 0.5f, 1.f);
		blockEX -> sub.timeCheck.duration = 1800;
		blockEX -> sub.timeCheck.successor = KOBATINE_LAMP;
	makeBlock(IMMINENT_BLUEBERRY_BUSH, 1.0, true, 4, 0, 1, 2, NULL, 0, 75, NULL, TIME_CHECK, NULL, MINE_SHALLAND_BUSH);
		blockEX -> sub.timeCheck.duration = 1200;
		blockEX -> sub.timeCheck.successor = BLUEBERRY_BUSH;
	makeBlock(BLUEBERRY_BUSH, 1.0, true, 38, 0, 1, 2, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 95, NULL, DECOMPOSING, QDIV_SPLIT(NULL, &harvestCrop, NULL), NO_CRITERION);
		blockEX -> sub.decomposite = IMMINENT_BLUEBERRY_BUSH;
	makeBlock(SHALLAND_FENCE, 0.9, true, 39, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 60, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(ARIDIS_FENCE, 0.9, true, 40, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 60, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(REDWOOD_FENCE, 0.9, true, 41, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 60, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(SILICON_CLUMP, 0.9, true, 42, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 260, NULL, NO_TYPE_ST, NULL, MINE_SILICON);
	makeBlock(TUNGSTEN_CLUMP, 0.9, true, 43, 0, 1, 1, QDIV_SPLIT(bronze_flac, NULL, NULL), QDIV_SPLIT(bronze_flacSZ, 0, 0), 270, NULL, NO_TYPE_ST, NULL, MINE_TUNGSTEN);
	makeBlock(WISP_LAMP, 0.9, true, 44, 0, 1, 1, NULL, 0, 60, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
		QDIV_SET_LIGHT(5, 0.4f, 1.f, 0.4f);
	makeBlock(TINY_WISP, 1.0, true, 45, 0, 1, 1, NULL, 0, 0, NULL, TIME_CHECK, NULL, NO_CRITERION);
		QDIV_SET_LIGHT(8, 0.4f, 1.f, 0.4f);
		blockEX -> sub.timeCheck.duration = 120;
		blockEX -> sub.timeCheck.successor = NO_WALL;
	makeBlock(SPIDER_WEB, 1.0, true, 46, 0, 1, 1, NULL, 0, 90, NULL, NO_TYPE_ST, NULL, ACQUIRE_WOOL);
	makeBlock(COFFEE_BUSH, 1.0, true, 47, 0, 1, 3, NULL, 0, 75, NULL, NO_TYPE_ST, NULL, MINE_COFFEE_BUSH);
	makeBlock(FERN, 1.0, true, 48, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 50, NULL, NO_TYPE_ST, NULL, MINE_COFFEE_BUSH);
	makeBlock(VIVO_ORCHID, 1.0, true, 49, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 180, NULL, NO_TYPE_ST, NULL, MINE_VIVO_ORCHID);
	makeBlock(COFFEE_WALL, 0.9, false, 50, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 60, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(BASALT_WALL, 0.9, false, 51, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 110, NULL, NO_TYPE_ST, NULL, MINE_BASALT);
	makeBlock(BASALT_LAMP, 0.9, true, 52, 0, 1, 1, NULL, 0, 110, NULL, NO_TYPE_ST, QDIV_SPLIT(NULL, &igniteLamp, NULL), NO_CRITERION);
		blockEX -> sub.litSelf = LIT_BASALT_LAMP;
	makeBlock(LIT_BASALT_LAMP, 0.9, true, 53, 0, 1, 1, NULL, 0, 110, NULL, TIME_CHECK, NULL, NO_CRITERION);
		QDIV_SET_LIGHT(6, 1.f, 0.6f, 0.2f);
		blockEX -> sub.timeCheck.duration = 1800;
		blockEX -> sub.timeCheck.successor = BASALT_LAMP;
		blockEX -> direction = EAST;
	makeBlock(IMMINENT_SPINACH, 1.0, true, 54, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 35, &cropPlaceable, TIME_CHECK, NULL, NO_CRITERION);
		blockEX -> sub.timeCheck.duration = 900;
		blockEX -> sub.timeCheck.successor = SPINACH;
	makeBlock(SPINACH, 1.0, true, 55, 0, 1, 1, QDIV_SPLIT(foliage_flac, NULL, NULL), QDIV_SPLIT(foliage_flacSZ, 0, 0), 320, NULL, DECOMPOSING, QDIV_SPLIT(NULL, &harvestCrop, NULL), HARVEST_SPINACH);
		blockEX -> sub.decomposite = IMMINENT_SPINACH;
	makeBlock(POTTED_KOBATINE_MUSHROOM, 0.9, true, 58, 0, 1, 2, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 35, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
		QDIV_SET_LIGHT(2, 0.25f, 0.25f, 1.f);
	makeBlock(POTTED_AGAVE, 0.9, true, 56, 0, 1, 2, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 35, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(POTTED_SPINACH, 0.9, true, 57, 0, 1, 2, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 35, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
	makeBlock(CACTUS_STEPTRAP, 1.0, true, 59, 0, 1, 1, QDIV_SPLIT(generic_flac, NULL, NULL), QDIV_SPLIT(generic_flacSZ, 0, 0), 0, NULL, NO_TYPE_ST, NULL, NO_CRITERION);
		blockEX -> sub.decay = 10;
		blockEX -> stepOn = QDIV_SPLIT(NULL, &stepTrap, NULL);
}

void makeCriterionTemplates() {
	criterionTemplate[ACQUIRE_WOOL] = makeCriterionTemplate("Wool acquired", GREEN);
	criterionTemplate[MINE_SHALLAND_BUSH] = makeCriterionTemplate("Shalland Bush Mined", YELLOW);
	criterionTemplate[MINE_ARIDIS_BUSH] = makeCriterionTemplate("Aridis Bush Mined", YELLOW);
	criterionTemplate[MINE_REDWOOD_TREE] = makeCriterionTemplate("Redwood Trees Mined", YELLOW);
	criterionTemplate[MINE_REDWOOD_LOG] = makeCriterionTemplate("Redwood Logs Mined", YELLOW);
	criterionTemplate[MINE_SHALLAND_GRASS] = makeCriterionTemplate("Shalland Grass Mined", YELLOW);
	criterionTemplate[MINE_BRONZE] = makeCriterionTemplate("Bronze Mined", YELLOW);
	criterionTemplate[MINE_ALUMINIUM] = makeCriterionTemplate("Aluminium Mined", YELLOW);
	criterionTemplate[MINE_IRON] = makeCriterionTemplate("Iron Mined", YELLOW);
	criterionTemplate[FERTILIZE_LAND] = makeCriterionTemplate("Blocks Fertilized", GREEN);
	criterionTemplate[MINE_COBALT] = makeCriterionTemplate("Cobalt Mined", YELLOW);
	criterionTemplate[MINE_NICKEL] = makeCriterionTemplate("Nickel Mined", YELLOW);
	criterionTemplate[MINE_SAND] = makeCriterionTemplate("Sand Mined", YELLOW);
	criterionTemplate[MINE_LIMESTONE] = makeCriterionTemplate("Limestone Mined", YELLOW);
	criterionTemplate[MINE_KOBATINE_ROCK] = makeCriterionTemplate("Kobatine Rock Mined", YELLOW);
	criterionTemplate[ENCOUNTER_WISP] = makeCriterionTemplate("Wisps Encountered", PURPLE);
	criterionTemplate[MINE_COFFEE_BUSH] = makeCriterionTemplate("Coffee Bush Mined", YELLOW);
	criterionTemplate[MINE_VIVO_ORCHID] = makeCriterionTemplate("Vivo Orchids Mined", YELLOW);
	criterionTemplate[MINE_BASALT] = makeCriterionTemplate("Basalt Mined", YELLOW);
	criterionTemplate[HARVEST_SPINACH] = makeCriterionTemplate("Spinach Harvested", GREEN);
	criterionTemplate[KILL_KOBATINE_BUG] = makeCriterionTemplate("Kobatine Bugs Killed", RED);
	criterionTemplate[MINE_KOBATINE_MUSHROOM] = makeCriterionTemplate("Kobatine Mushrooms Mined", YELLOW);
	criterionTemplate[MINE_AGAVE] = makeCriterionTemplate("Agaves Mined", YELLOW);
	criterionTemplate[MINE_CLAY] = makeCriterionTemplate("Clay Mined", YELLOW);
	criterionTemplate[COLLECT_WOOD] = makeCriterionTemplate("Wood Collected", YELLOW);
}

int32_t roleEX;
artifact_st* artifactEX;

void makeArtifact(qint_t artifactSL, int8_t* inName, int8_t* inDesc, const uint8_t* inTexture, size_t inTextureSZ, bool inCross, artifactAction inPrimary, uint64_t inPrimaryCost, float inPrimaryTime, bool inPrimaryCancel, float inPrimaryRange, artifactAction inSecondary, uint64_t inSecondaryCost, float inSecondaryTime, bool inSecondaryCancel, float inSecondaryRange) {
	artifactEX = &artifact[roleEX][artifactSL];
	strcpy(artifactEX -> name, inName);
	strcpy(artifactEX -> desc, inDesc);
	artifactEX -> crossCriterial = inCross;
	#ifdef QDIV_CLIENT
	if(inTexture == NULL) {
		artifactEX -> texture = 0;
	}else{
		loadTexture(&artifactEX -> texture, inTexture, inTextureSZ);
	}
	#endif
	artifactEX -> primary.action = inPrimary;
	artifactEX -> secondary.action = inSecondary;
	artifactEX -> primary.cost = inPrimaryCost;
	artifactEX -> secondary.cost = inSecondaryCost;
	artifactEX -> primary.useTM = inPrimaryTime;
	artifactEX -> secondary.useTM = inSecondaryTime;
	artifactEX -> primary.cancelable = inPrimaryCancel;
	artifactEX -> secondary.cancelable = inSecondaryCancel;
	artifactEX -> primary.range = inPrimaryRange;
	artifactEX -> secondary.range = inSecondaryRange;
	artifactEX -> qEnergy = 0;
	for(int32_t criterionSL = 0; criterionSL < QDIV_ARTIFACT_CRITERIA; criterionSL++) {
		artifactEX -> criterion[criterionSL].Template = NO_CRITERION;
	}
}

#define QDIV_SET_CRITERION(slot, temp, val) {\
	artifactEX -> criterion[slot].Template = temp;\
	artifactEX -> criterion[slot].value = val;\
}

#ifdef QDIV_CLIENT

void playSound(const uint8_t* file, size_t fileSZ);
void qDivAudioInit();
void qDivAudioStop();

#endif

def_ArtifactAction(simpleSwing);
def_ArtifactAction(pulsatingSpell);

def_ArtifactAction(sliceEntity);
def_ArtifactAction(swordSprint);
def_ArtifactAction(mineBlock);
def_ArtifactAction(placeBlock);
def_ArtifactAction(fertilizeBlock);
def_ArtifactAction(scoopWater);
def_ArtifactAction(buildAdmin);
def_ArtifactAction(openLootBox);
def_ArtifactAction(shootProj);
def_ArtifactAction(vivoSpell);
def_ArtifactAction(shearSheep);

void makePlaceable(qint_t artifactSL, qint_t represent, int8_t* name, int8_t* desc, bool cross, uint64_t cost, qint_t layer) {
	makeArtifact(artifactSL, name, desc, NULL, 0, cross, QDIV_SPLIT(NULL, &placeBlock, NULL), cost, 0.1, true, 4, NULL, 0, 0, true, 4);
	artifactEX -> primary.sub.place.represent = represent;
	artifactEX -> primary.sub.place.layer = layer;
}

void makePlaceableWall(qint_t artifactSL, qint_t inRepresent, int8_t* inName, bool inCross) {
	makeArtifact(artifactSL, inName, "Placeable Wall", NULL, 0, inCross, QDIV_SPLIT(NULL, &placeBlock, NULL), 1, 0.1, true, 4, NULL, 0, 0, true, 4);
	artifactEX -> primary.sub.place.represent = inRepresent;
	artifactEX -> primary.sub.place.layer = 1;
}

void makePlaceableFloor(qint_t artifactSL, qint_t inRepresent, int8_t* inName, bool inCross) {
	makeArtifact(artifactSL, inName, "Placeable Floor", NULL, 0, inCross, QDIV_SPLIT(NULL, &placeBlock, NULL), 1, 0.1, true, 4, NULL, 0, 0, true, 4);
	artifactEX -> primary.sub.place.represent = inRepresent;
	artifactEX -> primary.sub.place.layer = 0;
}

void makeArtifacts() {
	
	roleEX = WARRIOR;
	makeArtifact(BRONZE_SHORTSWORD, "Bronze Shortsword", "Slices with a decay of 5.", QDIV_SPLIT(artifacts_bronze_shortsword_png, NULL, NULL), QDIV_SPLIT(artifacts_bronze_shortsword_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &sliceEntity, NULL), 0, 0.25, false, 32, QDIV_SPLIT(NULL, &swordSprint, NULL), 5, 0, true, 32);
		QDIV_SET_CRITERION(0, MINE_BRONZE, 20);
		artifactEX -> primary.sub.slice.decay = 5;
		artifactEX -> primary.sub.slice.range = QDIV_SPLIT(2, 4, 0);
	makeArtifact(ALUMINIUM_STAR_ARTIFACT, "Aluminium Star", "Slices with a decay of 5.", QDIV_SPLIT(artifacts_aluminium_star_png, NULL, NULL), QDIV_SPLIT(artifacts_aluminium_star_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &shootProj, NULL), 0, 1.25, false, 32, NULL, 0, 0, true, 32);
		artifactEX -> qEnergy = 140;
		QDIV_SET_CRITERION(0, MINE_ALUMINIUM, 20);
		artifactEX -> primary.sub.shoot.represent = ALUMINIUM_STAR;
		artifactEX -> primary.sub.shoot.decay = 4;
		artifactEX -> primary.sub.shoot.pierce = 1;
	makeArtifact(IRON_SHORTSWORD, "Iron Shortsword", "Slices with a decay of 6.", QDIV_SPLIT(artifacts_iron_shortsword_png, NULL, NULL), QDIV_SPLIT(artifacts_iron_shortsword_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &sliceEntity, NULL), 0, 0.25, false, 32, QDIV_SPLIT(NULL, &swordSprint, NULL), 5, 0, true, 32);
		QDIV_SET_CRITERION(0, MINE_IRON, 20);
		artifactEX -> primary.sub.slice.decay = 6;
		artifactEX -> primary.sub.slice.range = QDIV_SPLIT(2, 4, 0);
	makeArtifact(COBATINE_BROADSWORD, "Cobalt Broadsword", "Slices with a decay of 8.", QDIV_SPLIT(artifacts_cobatine_broadsword_png, NULL, NULL), QDIV_SPLIT(artifacts_cobatine_broadsword_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &sliceEntity, NULL), 0, 0.5, false, 32, QDIV_SPLIT(NULL, &swordSprint, NULL), 10, 0, true, 32);
		artifactEX -> qEnergy = 80;
		QDIV_SET_CRITERION(0, KILL_KOBATINE_BUG, 100);
		artifactEX -> primary.sub.slice.decay = 8;
		artifactEX -> primary.sub.slice.range = QDIV_SPLIT(2, 6, 0);
	makeArtifact(COBALT_SHORTSWORD, "Cobalt Shortsword", "Slices with a decay of 8.", QDIV_SPLIT(artifacts_cobalt_shortsword_png, NULL, NULL), QDIV_SPLIT(artifacts_cobalt_shortsword_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &sliceEntity, NULL), 0, 0.25, false, 32, QDIV_SPLIT(NULL, &swordSprint, NULL), 5, 0, true, 32);
		artifactEX -> qEnergy = 210;
		QDIV_SET_CRITERION(0, MINE_COBALT, 20);
		artifactEX -> primary.sub.slice.decay = 8;
		artifactEX -> primary.sub.slice.range = QDIV_SPLIT(2, 4, 0);
	makeArtifact(NICKEL_STAR_ARTIFACT, "Nickel Star", "Slices with a decay of 5.", QDIV_SPLIT(artifacts_nickel_star_png, NULL, NULL), QDIV_SPLIT(artifacts_nickel_star_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &shootProj, NULL), 1, 1.25, false, 32, NULL, 0, 0, true, 32);
		artifactEX -> qEnergy = 210;
		QDIV_SET_CRITERION(0, MINE_NICKEL, 20);
		artifactEX -> primary.sub.shoot.represent = NICKEL_STAR;
		artifactEX -> primary.sub.shoot.decay = 7;
		artifactEX -> primary.sub.shoot.pierce = 1;
	makeArtifact(TROPICAL_BROADSWORD, "Tropical Broadsword", "Slices with a decay of 7.", QDIV_SPLIT(artifacts_tropical_broadsword_png, NULL, NULL), QDIV_SPLIT(artifacts_tropical_broadsword_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &sliceEntity, NULL), 0, 0.5, false, 32, QDIV_SPLIT(NULL, &swordSprint, NULL), 10, 0, true, 32);
		artifactEX -> qEnergy = 210;
		QDIV_SET_CRITERION(0, MINE_COFFEE_BUSH, 50);
		QDIV_SET_CRITERION(1, MINE_AGAVE, 20);
		artifactEX -> primary.sub.slice.decay = 7;
		artifactEX -> primary.sub.slice.range = QDIV_SPLIT(2, 6, 0);
	makePlaceable(CACTUS_STEPTRAP_ARTIFACT, CACTUS_STEPTRAP, "Cactus Steptrap", "Placeable Steptrap.", false, 4, 1);
		artifactEX -> qEnergy = 210;
		QDIV_SET_CRITERION(0, MINE_COFFEE_BUSH, 50);
		QDIV_SET_CRITERION(1, MINE_AGAVE, 50);
	makeArtifact(SILICON_STAR_ARTIFACT, "Silicon Star", "Slices with a decay of 5.", QDIV_SPLIT(artifacts_silicon_star_png, NULL, NULL), QDIV_SPLIT(artifacts_silicon_star_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &shootProj, NULL), 3, 1.25, false, 32, NULL, 0, 0, true, 32);
		artifactEX -> qEnergy = 210;
		QDIV_SET_CRITERION(0, MINE_SILICON, 20);
		artifactEX -> primary.sub.shoot.represent = SILICON_STAR;
		artifactEX -> primary.sub.shoot.decay = 10;
		artifactEX -> primary.sub.shoot.pierce = 1;
	makeArtifact(TUNGSTEN_LONGSWORD, "Tungsten Longsword", "Slices with a decay of 7.", QDIV_SPLIT(artifacts_tungsten_longsword_png, NULL, NULL), QDIV_SPLIT(artifacts_tungsten_longsword_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &sliceEntity, NULL), 0, 0.5, false, 32, QDIV_SPLIT(NULL, &swordSprint, NULL), 10, 0, true, 32);
		artifactEX -> qEnergy = 210;
		QDIV_SET_CRITERION(0, MINE_TUNGSTEN, 40);
		artifactEX -> primary.sub.slice.decay = 10;
		artifactEX -> primary.sub.slice.range = QDIV_SPLIT(2, 8, 0);
	
	roleEX = EXPLORER;
	makeArtifact(OLD_CHOPPER, "Old Chopper", "Mines a Block every 10 seconds.", QDIV_SPLIT(artifacts_old_chopper_png, NULL, NULL), QDIV_SPLIT(artifacts_old_chopper_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 10, true, 3, NULL, 0, 0, true, 3);
		artifactEX -> primary.sub.slice.decay = 0;
	makeArtifact(IRON_KEY, "Iron Key", "Opens Aridis Lootboxes", QDIV_SPLIT(artifacts_iron_key_png, NULL, NULL), QDIV_SPLIT(artifacts_iron_key_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &openLootBox, NULL), 0, 0.5, true, 4, NULL, 0, 0, true, 4);
		QDIV_SET_CRITERION(0, MINE_IRON, 20);
		artifactEX -> primary.sub.lootBox = ARIDIS_LOOTBOX;
	makeArtifact(SEQUOIA_RAFT_ARTIFACT, "Sequoia Raft", "Aquatic Vehicle.", QDIV_SPLIT(artifacts_sequoia_raft_png, NULL, NULL), QDIV_SPLIT(artifacts_sequoia_raft_pngSZ, 0, 0), false, NULL, 0, 0, true, 32, NULL, 0, 0, true, 32);
		artifactEX -> qEnergy = 50;
		QDIV_SET_CRITERION(0, MINE_REDWOOD_LOG, 16);
		QDIV_SET_CRITERION(1, ACQUIRE_WOOL, 16);
		
	roleEX = BUILDER;
	makeArtifact(OLD_SWINGER, "Old Swinger", "Mines a Block every 5 seconds.", QDIV_SPLIT(artifacts_old_swinger_png, NULL, NULL), QDIV_SPLIT(artifacts_old_swinger_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 5, true, 4, QDIV_SPLIT(NULL, &buildAdmin, NULL), 0, 0.1, true, 4);
		artifactEX -> primary.sub.slice.decay = 0;
		artifactEX -> primary.sub.slice.range = 1;
	makePlaceableWall(SHALLAND_WALL_ARTIFACT, SHALLAND_WALL, "Shalland Wood Wall", false);
		QDIV_SET_CRITERION(0, MINE_SHALLAND_BUSH, 50);
	makePlaceableFloor(SHALLAND_FLOOR_ARTIFACT, SHALLAND_FLOOR, "Shalland Wood Floor", false);
		QDIV_SET_CRITERION(0, MINE_SHALLAND_BUSH, 50);
	makePlaceableWall(ARIDIS_WALL_ARTIFACT, ARIDIS_WALL, "Aridis Wood Wall", false);
		QDIV_SET_CRITERION(0, MINE_ARIDIS_BUSH, 50);
	makePlaceableFloor(ARIDIS_FLOOR_ARTIFACT, ARIDIS_FLOOR, "Aridis Wood Floor", false);
		QDIV_SET_CRITERION(0, MINE_ARIDIS_BUSH, 50);
	makePlaceableWall(REDWOOD_WALL_ARTIFACT, REDWOOD_WALL, "Redwood Wall", false);
		QDIV_SET_CRITERION(0, MINE_REDWOOD_TREE, 50);
	makePlaceableFloor(REDWOOD_FLOOR_ARTIFACT, REDWOOD_FLOOR, "Redwood Floor", false);
		QDIV_SET_CRITERION(0, MINE_REDWOOD_TREE, 50);
	makePlaceableWall(COFFEE_WALL_ARTIFACT, COFFEE_WALL, "Coffee Wood Wall", false);
		QDIV_SET_CRITERION(0, MINE_COFFEE_BUSH, 50);
	makePlaceableFloor(COFFEE_FLOOR_ARTIFACT, COFFEE_FLOOR, "Coffee Wood Floor", false);
		QDIV_SET_CRITERION(0, MINE_COFFEE_BUSH, 50);
	makePlaceableWall(SANDSTONE_WALL_ARTIFACT, SANDSTONE_WALL, "Sandstone Wall", false);
		QDIV_SET_CRITERION(0, MINE_SAND, 50);
	makePlaceableFloor(SANDSTONE_FLOOR_ARTIFACT, SANDSTONE_FLOOR, "Sandstone Floor", false);
		QDIV_SET_CRITERION(0, MINE_SAND, 50);
	makePlaceableWall(SANDSTONE_LAMP_ARTIFACT, LIT_SANDSTONE_LAMP, "Sandstone Lamp", false);
		QDIV_SET_CRITERION(0, MINE_SAND, 50);
		QDIV_SET_CRITERION(1, COLLECT_WOOD, 50);
	makePlaceableWall(LIMESTONE_WALL_ARTIFACT, LIMESTONE_WALL, "Limestone Wall", false);
		QDIV_SET_CRITERION(0, MINE_LIMESTONE, 50);
	makePlaceableWall(LIMESTONE_BRICKS_ARTIFACT, LIMESTONE_BRICKS, "Limestone Bricks", false);
		QDIV_SET_CRITERION(0, MINE_LIMESTONE, 50);
	makePlaceableFloor(LIMESTONE_FLOOR_ARTIFACT, LIMESTONE_FLOOR, "Limestone Floor", false);
		QDIV_SET_CRITERION(0, MINE_LIMESTONE, 50);
	makePlaceableWall(LIMESTONE_LAMP_ARTIFACT, LIT_LIMESTONE_LAMP, "Limestone Lamp", false);
		QDIV_SET_CRITERION(0, MINE_LIMESTONE, 50);
		QDIV_SET_CRITERION(1, COLLECT_WOOD, 50);
	makePlaceableWall(KOBATINE_WALL_ARTIFACT, KOBATINE_WALL, "Kobatine Wall", false);
		QDIV_SET_CRITERION(0, MINE_KOBATINE_ROCK, 50);
	makePlaceableFloor(KOBATINE_FLOOR_ARTIFACT, KOBATINE_FLOOR, "Kobatine Floor", false);
		QDIV_SET_CRITERION(0, MINE_KOBATINE_ROCK, 50);
	makePlaceableFloor(KOBATINE_TILES_ARTIFACT, KOBATINE_TILES, "Kobatine Tiles", false);
		QDIV_SET_CRITERION(0, MINE_KOBATINE_ROCK, 50);
	makePlaceableWall(KOBATINE_LAMP_ARTIFACT, LIT_KOBATINE_LAMP, "Kobatine Lamp", false);
		QDIV_SET_CRITERION(0, MINE_KOBATINE_ROCK, 50);
		QDIV_SET_CRITERION(1, COLLECT_WOOD, 50);
	makePlaceableWall(BASALT_WALL_ARTIFACT, BASALT_WALL, "Basalt Wall", false);
		QDIV_SET_CRITERION(0, MINE_BASALT, 50);
	makePlaceableFloor(BASALT_FLOOR_ARTIFACT, BASALT_FLOOR, "Basalt Floor", false);
		QDIV_SET_CRITERION(0, MINE_BASALT, 50);
	makePlaceableWall(BASALT_LAMP_ARTIFACT, LIT_BASALT_LAMP, "Basalt Lamp", false);
		QDIV_SET_CRITERION(0, MINE_BASALT, 50);
		QDIV_SET_CRITERION(1, COLLECT_WOOD, 50);
	makePlaceableWall(WISP_LAMP_ARTIFACT, WISP_LAMP, "Wisp Lamp", false);
		QDIV_SET_CRITERION(0, MINE_REDWOOD_TREE, 50);
		QDIV_SET_CRITERION(1, ENCOUNTER_WISP, 8);
	makeArtifact(FERRA, "Ferra", "Mines a Block every 2.5 seconds.", QDIV_SPLIT(artifacts_ferra_png, NULL, NULL), QDIV_SPLIT(artifacts_ferra_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 2.5, true, 4, NULL, 0, 0, true, 4);
		artifactEX -> qEnergy = 200;
		QDIV_SET_CRITERION(0, MINE_ALUMINIUM, 100);
		QDIV_SET_CRITERION(1, MINE_IRON, 5);
		QDIV_SET_CRITERION(2, MINE_ARIDIS_BUSH, 5);
		artifactEX -> primary.sub.slice.decay = 0;
		artifactEX -> primary.sub.slice.range = 1;
	makeArtifact(COBALT_PICKAXE, "Cobalt Pickaxe", "Mines a Block every 3 seconds.", QDIV_SPLIT(artifacts_cobalt_pickaxe_png, NULL, NULL), QDIV_SPLIT(artifacts_cobalt_pickaxe_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 3, true, 5, NULL, 0, 0, true, 4);
		artifactEX -> qEnergy = 210;
		QDIV_SET_CRITERION(0, MINE_COBALT, 20);
		QDIV_SET_CRITERION(1, MINE_REDWOOD_TREE, 5);
		artifactEX -> primary.sub.slice.decay = 0;
		artifactEX -> primary.sub.slice.range = 1;
	
	roleEX = GARDENER;
	makeArtifact(SIMPLE_HOE, "Simple Hoe", "Mines a Block every 7.5 seconds.\nFertilizes Soil.", QDIV_SPLIT(artifacts_simple_hoe_png, NULL, NULL), QDIV_SPLIT(artifacts_simple_hoe_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 7.5, true, 4, QDIV_SPLIT(&simpleSwing, &fertilizeBlock, NULL), 0, 1, true, 4);
		artifactEX -> primary.sub.slice.decay = 0;
		artifactEX -> primary.sub.slice.range = 1;
	makeArtifact(POTATO_ARTIFACT, "Shalland Potato", "Plantable Crop.", QDIV_SPLIT(artifacts_potato_png, NULL, NULL), QDIV_SPLIT(artifacts_potato_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, 4, NULL, 0, 1, true, 4);
		QDIV_SET_CRITERION(0, FERTILIZE_LAND, 50);
		QDIV_SET_CRITERION(1, MINE_SHALLAND_GRASS, 5);
		artifactEX -> primary.sub.place.represent = IMMINENT_POTATO;
		artifactEX -> primary.sub.place.layer = 1;
	makePlaceableWall(SHALLAND_FENCE_ARTIFACT, SHALLAND_FENCE, "Shalland Wood Fence", false);
		QDIV_SET_CRITERION(0, MINE_SHALLAND_BUSH, 50);
	makePlaceableWall(ARIDIS_FENCE_ARTIFACT, ARIDIS_FENCE, "Aridis Wood Fence", false);
		QDIV_SET_CRITERION(0, MINE_ARIDIS_BUSH, 50);
	makePlaceableWall(REDWOOD_FENCE_ARTIFACT, REDWOOD_FENCE, "Shalland Wood Fence", false);
		QDIV_SET_CRITERION(0, MINE_REDWOOD_TREE, 50);
	makePlaceableWall(POTTED_KOBATINE_MUSHROOM_ARTIFACT, POTTED_KOBATINE_MUSHROOM, "Potted Kobatine Mushroom", false);
		artifactEX -> qEnergy = 100;
		QDIV_SET_CRITERION(0, MINE_CLAY, 50);
		QDIV_SET_CRITERION(1, MINE_KOBATINE_MUSHROOM, 50);
	makePlaceableWall(POTTED_AGAVE_ARTIFACT, POTTED_AGAVE, "Potted Agave", false);
		artifactEX -> qEnergy = 250;
		QDIV_SET_CRITERION(0, MINE_CLAY, 50);
		QDIV_SET_CRITERION(1, MINE_AGAVE, 50);
	makeArtifact(SCISSORS, "Scissors", "Used to shear sheep.", QDIV_SPLIT(artifacts_scissors_png, NULL, NULL), QDIV_SPLIT(artifacts_scissors_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &shearSheep, NULL), 0, 30, true, 4, NULL, 0, 1, true, 4);
		artifactEX -> qEnergy = 175;
		QDIV_SET_CRITERION(0, MINE_IRON, 20);
	makeArtifact(COBALT_HOE, "Cobalt Hoe", "Mines a Block every 5 seconds.\nFertilizes Soil.", QDIV_SPLIT(artifacts_cobalt_hoe_png, NULL, NULL), QDIV_SPLIT(artifacts_cobalt_hoe_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 5, true, 5, QDIV_SPLIT(&simpleSwing, &fertilizeBlock, NULL), 0, 1, true, 4);
		artifactEX -> qEnergy = 210;
		QDIV_SET_CRITERION(0, MINE_COBALT, 20);
		QDIV_SET_CRITERION(1, MINE_REDWOOD_TREE, 5);
		artifactEX -> primary.sub.slice.decay = 0;
		artifactEX -> primary.sub.slice.range = 1;
	makeArtifact(SPINACH_ARTIFACT, "Spinach", "Plantable Crop.", QDIV_SPLIT(artifacts_spinach_png, NULL, NULL), QDIV_SPLIT(artifacts_spinach_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.1, true, 4, NULL, 0, 1, true, 4);
		artifactEX -> qEnergy = 320;
		QDIV_SET_CRITERION(0, FERTILIZE_LAND, 200);
		QDIV_SET_CRITERION(1, HARVEST_SPINACH, 5);
		artifactEX -> primary.sub.place.represent = IMMINENT_SPINACH;
		artifactEX -> primary.sub.place.layer = 1;
	makePlaceableWall(POTTED_SPINACH_ARTIFACT, POTTED_SPINACH, "Potted Spinach", false);
		artifactEX -> qEnergy = 320;
		QDIV_SET_CRITERION(0, MINE_CLAY, 50);
		QDIV_SET_CRITERION(1, HARVEST_SPINACH, 50);
	
	roleEX = ENGINEER;
	makeArtifact(IRON_WRENCH, "Iron Wrench", "Mines a Block every 7.5 seconds.", QDIV_SPLIT(artifacts_iron_wrench_png, NULL, NULL), QDIV_SPLIT(artifacts_iron_wrench_pngSZ, 0, 0), false, QDIV_SPLIT(&simpleSwing, &mineBlock, NULL), 0, 1, true, 4, NULL, 0, 0, true, 4);
		artifactEX -> primary.sub.slice.decay = 0;
	
	roleEX = WIZARD;
	makeArtifact(BREAKER_SPELL, "Breaker Spell", "Mines a Block every 7.5 seconds.", QDIV_SPLIT(artifacts_breaker_spell_png, NULL, NULL), QDIV_SPLIT(artifacts_breaker_spell_pngSZ, 0, 0), false, QDIV_SPLIT(&pulsatingSpell, &mineBlock, NULL), 0, 7.5, true, 8, NULL, 0, 0.1, true, 4);
	makeArtifact(VIVO_SPELL, "Vivo Spell", "Grows all kinds of Vegetation.", QDIV_SPLIT(artifacts_vivo_spell_png, NULL, NULL), QDIV_SPLIT(artifacts_vivo_spell_pngSZ, 0, 0), false, QDIV_SPLIT(&pulsatingSpell, &vivoSpell, NULL), 1, 1, true, 8, NULL, 0, 0, true, 4);
		QDIV_SET_CRITERION(0, MINE_VIVO_ORCHID, 20);
	makeArtifact(WISP_SPELL, "Wisp Spell", "Lights up a large area for 60 seconds.", QDIV_SPLIT(wisp0_png, NULL, NULL), QDIV_SPLIT(wisp0_pngSZ, 0, 0), false, QDIV_SPLIT(NULL, &placeBlock, NULL), 0, 0.5, true, 32, NULL, 0, 0, true, 32);
		artifactEX -> qEnergy = 100;
		QDIV_SET_CRITERION(0, ENCOUNTER_WISP, 2);
		artifactEX -> primary.sub.place.represent = TINY_WISP;
		artifactEX -> primary.sub.place.layer = 1;
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

void makeEffect(qint_t tag, int8_t* name, uint32_t texture, float potency, bool visible, effect_fp action, entity_fp init) {
	strcpy(effect[tag].name, name);
	effect[tag].texture = texture;
	effect[tag].potency = potency;
	effect[tag].visible = visible;
	effect[tag].action = action;
	effect[tag].init = init;
}

void makeEffects() {
	makeEffect(SWORD_SPRINT, "Sword Sprint", 0, 0, false, NULL, NULL);
}

bool inCollection(int32_t elementIQ, collection_st* collectionIQ) {
	for(size_t elementSL = 0; elementSL < collectionIQ -> length; elementSL++) {
		if(collectionIQ -> elements[elementSL] == elementIQ) return true;
	}
	return false;
}

void libMain() {
	printf("> Loading qDivLib-%d.%c\n", QDIV_VERSION, QDIV_BRANCH);
	puts("> Adding Block Types");
	makeArtifacts();
	makeBlocks();
	makeEntities();
	puts("> Adding Criterion Templates");
	makeCriterionTemplates();
	puts("> Adding Roles");
	makeRoles();
	makeEffects();
}
