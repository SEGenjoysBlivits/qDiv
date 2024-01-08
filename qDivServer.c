/*
qDivServer - The Logical side of the 2D sandbox game "qDiv"
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
#define QDIV_SERVER
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<pthread.h>
#include<time.h>
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#include<ws2tcpip.h>
#include<winsock2.h>
#include<iphlpapi.h>
#include<windows.h>
#define QDIV_MKDIR(folder) mkdir(folder)
#else
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<unistd.h>
#include<signal.h>
#define QDIV_MKDIR(folder) mkdir(folder, S_IRWXU)
#endif
#include "include/open-simplex-noise.h"
#include "qDivLib.h"

int32_t qShutdown = 0;
int32_t* pShutdown = &qShutdown;

int32_t qTickTime;
int32_t* pTickTime = &qTickTime;
bool TickQuery = false;
bool* pTickQuery = &TickQuery;

field_t field[QDIV_MAX_FIELDS];

typedef struct {
	int32_t socket;
	entity_t* entity;
	int8_t name[16];
	uint8_t uuid[16];
	uint8_t privateKey[ECC_PRV_KEY_SIZE];
	uint8_t internalKey[ECC_PUB_KEY_SIZE];
	uint8_t externalKey[ECC_PUB_KEY_SIZE];
	uint8_t sharedKey[ECC_PRV_KEY_SIZE];
	bool keyAvailable;
} client_t;

client_t client[QDIV_MAX_PLAYERS];

// Synchronizer
void syncEntityRemoval(int32_t slot) {
	sendBF[4] = 0x03;
	memcpy(sendBF+5, &slot, sizeof(int));
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].type == PLAYER && entityInRange(entity + slot, entity + entitySL)) send(*entity[entitySL].unique.Player.socket, sendBF, QDIV_PACKET_SIZE, 0);
	}
}

// Synchronizer
void syncEntity(entity_t* entityIQ) {
	sendBF[4] = 0x04;
	memcpy(sendBF+5, entityIQ, sizeof(entity_t));
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].type == PLAYER && entityInRange(entityIQ, entity + entitySL)) send(*entity[entitySL].unique.Player.socket, sendBF, QDIV_PACKET_SIZE, 0);
	}
}

// Synchronizer
void syncEntityField(field_t* fieldIQ, int32_t sockIQ) {
	entity_t* entityIQ;
	FOR_EVERY_ENTITY {
		if(entityTable[entitySL]) {
			entityIQ = entity + entitySL;
			if(entityIQ -> zone == fieldIQ -> zone && entityIQ -> fldX == fieldIQ -> fldX && entityIQ -> fldY == fieldIQ -> fldY) {
				sendPacket(0x04, (void*)entityIQ, sizeof(entity_t), sockIQ);
			}
		}
	}
}

// Synchronizer
/*void selectAndSend(field_t* fieldIQ, int32_t sockIQ) {
	uint8_t fieldBF[32768];
	sendBF[4] = 0x05;
	fieldSelector selectorIQ = {fieldIQ -> zone, fieldIQ -> fldX, fieldIQ -> fldY};
	memcpy(sendBF+5, &selectorIQ, sizeof(fieldSelector));
	send(sockIQ, sendBF, QDIV_PACKET_SIZE, 0);
	for(int32_t segmentSL = 0; segmentSL < 4; segmentSL++) {
		memcpy(fieldBF, fieldIQ -> block[segmentSL * 32], 32768);
		send(sockIQ, fieldBF, 32768, 0);
	}
}*/

// Synchronizer
void syncBlock(uint16_t blockIQ, field_t* fieldIQ, int32_t posX, int32_t posY, int32_t layer) {
	makeBlockPacket(blockIQ, fieldIQ -> fldX, fieldIQ -> fldY, posX, posY, layer);
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entity[entitySL].type == PLAYER && (entity[entitySL].local[0][0] == fieldIQ || entity[entitySL].local[0][1] == fieldIQ || entity[entitySL].local[0][2] == fieldIQ || entity[entitySL].local[1][0] == fieldIQ || entity[entitySL].local[1][1] == fieldIQ || entity[entitySL].local[1][2] == fieldIQ || entity[entitySL].local[2][0] == fieldIQ || entity[entitySL].local[2][1] == fieldIQ || entity[entitySL].local[2][2] == fieldIQ)) send(*entity[entitySL].unique.Player.socket, sendBF, QDIV_PACKET_SIZE, 0);
	}
}

// Synchronizer
void syncTime() {
	sendBF[4] = 0x0B;
	sendBF[5] = (int8_t)currentHour;
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].type == PLAYER) send(*entity[entitySL].unique.Player.socket, sendBF, QDIV_PACKET_SIZE, 0);
	}
}

void unloadField(int32_t inZone, int32_t inFldX, int32_t inFldY) {
	for(int32_t fieldSL = 0; fieldSL < QDIV_MAX_FIELDS; fieldSL++) {
		field_t* fieldIQ = field + fieldSL;
		if(fieldIQ -> zone == inZone && fieldIQ -> fldX == inFldX && fieldIQ -> fldY == inFldY) {
			int32_t entitySL = 0;
			entity_t* entityIQ = entity + entitySL;
			while((entityIQ -> type != PLAYER ||
			entityIQ -> active != 1 ||
			entityIQ -> zone != fieldIQ -> zone ||
			entityIQ -> fldX > fieldIQ -> fldX+1 ||
			entityIQ -> fldX < fieldIQ -> fldX-1 || 
			entityIQ -> fldY > fieldIQ -> fldY+1 ||
			entityIQ -> fldY < fieldIQ -> fldY-1) &&
			entitySL < QDIV_MAX_ENTITIES) {
				entitySL++;
				entityIQ = entity + entitySL;
			}
			if(entitySL == QDIV_MAX_ENTITIES) {
				for(entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
					entityIQ = entity + entitySL;
					if(entityTable[entitySL] &&
					entityIQ -> zone == inZone &&
					entityIQ -> fldX == inFldX &&
					entityIQ -> fldY == inFldY) {
						entityIQ -> active = 0;
					}
				}
				fieldIQ -> active = 0;
				if(fieldIQ -> edit == 1) {
					int8_t fileName[64];
					sprintf(fileName, "world/fld.%dz.%dx.%dy.dat", inZone, inFldX, inFldY);
					FILE* fileData = fopen(fileName, "wb");
					fwrite(fieldIQ -> block, 1, sizeof(fieldIQ -> block), fileData);
					fclose(fileData);
				}
			}
			break;
		}
	}
}

void removeEntity(int32_t slot) {
	entity[slot].active = 0;
	entityTable[slot] = false;
	if(entity[slot].type == PLAYER) {
		unloadField(entity[slot].zone, entity[slot].fldX, entity[slot].fldY);
		unloadField(entity[slot].zone, entity[slot].fldX+1, entity[slot].fldY);
		unloadField(entity[slot].zone, entity[slot].fldX-1, entity[slot].fldY);
		unloadField(entity[slot].zone, entity[slot].fldX, entity[slot].fldY+1);
		unloadField(entity[slot].zone, entity[slot].fldX+1, entity[slot].fldY+1);
		unloadField(entity[slot].zone, entity[slot].fldX-1, entity[slot].fldY+1);
		unloadField(entity[slot].zone, entity[slot].fldX, entity[slot].fldY-1);
		unloadField(entity[slot].zone, entity[slot].fldX+1, entity[slot].fldY-1);
		unloadField(entity[slot].zone, entity[slot].fldX-1, entity[slot].fldY-1);
		int8_t fileName[32];
		sprintf(fileName, "criterion/CR.%s.dat", entity[slot].name);
		FILE* criterionFile = fopen(fileName, "wb");
		if(criterionFile != NULL) {
			fwrite(entity[slot].unique.Player.criterion, sizeof(uint32_t), MAX_CRITERION, criterionFile);
			fclose(criterionFile);
		}
		free(entity[slot].unique.Player.criterion);
	}
	if(entityType[entity[slot].type].persistant) {
		FILE* entityFile;
		int8_t hex[QDIV_B16 + 1];
		int8_t fileName[QDIV_B16 + 11];
		uuidToHex(entity[slot].uuid, hex);
		sprintf(fileName, "entity/%s.dat", hex);
		entityFile = fopen(fileName, "wb");
		if(entityFile != NULL) {
			fwrite(entity + slot, 1, sizeof(entity_t), entityFile);
			fclose(entityFile);
		}
	}
}

bool damageEntity(entity_t* entityIQ, uint64_t decay) {
	entityIQ -> healthTimer = 0;
	entityIQ -> health--;
	if(entityIQ -> health == 0 || decay >= entityIQ -> qEnergy) {
		if(entityIQ -> type == PLAYER) {
			entityIQ -> health = 5;
			entityIQ -> qEnergy = 1;
			entityIQ -> healthTimer = 60.0;
			sendPacket(0x04, (void*)entityIQ, sizeof(entity_t), *entityIQ -> unique.Player.socket);
		}else{
			removeEntity(entityIQ -> slot);
			syncEntityRemoval(entityIQ -> slot);
		}
		return true;
	}else{
		if(entityIQ -> type == PLAYER) {
			sendPacket(0x04, (void*)entityIQ, sizeof(entity_t), *entityIQ -> unique.Player.socket);
		}
		entityIQ -> qEnergy -= decay;
		return false;
	}
}

bool isEntityPresent(entity_t* entityIQ, int32_t fldX, int32_t fldY, double posX, double posY) {
	double boxIQ = entityType[entityIQ -> type].hitBox;
	return abs(entityIQ -> posX - posX + (double)((entityIQ -> fldX - fldX) * 128)) < boxIQ && abs(entityIQ -> posY - posY + (double)((entityIQ -> fldY - fldY) * 128)) < boxIQ;
}

bool isEntityPresentInRange(entity_t* entityIQ, int32_t fldX, int32_t fldY, double centerX, double centerY, double range) {
	double boxIQ = entityType[entityIQ -> type].hitBox;
	double posRelX = (double)((entityIQ -> fldX - fldX) * 128);
	double posRelY = (double)((entityIQ -> fldY - fldY) * 128);
	return entityIQ -> posX + boxIQ + posRelX > centerX - range && entityIQ -> posX - boxIQ + posRelX < centerX + range && entityIQ -> posY + boxIQ + posRelY > centerY - range && entityIQ -> posY - boxIQ + posRelY < centerY + range;
}

// Artifact Action
def_ArtifactAction(placeBlock) {
	if(playerIQ -> useTimer >= 0.1) {
		artifactSettings* artifactIQ = (artifactSettings*)artifactVD;
		playerIQ -> useTimer = 0;
		int32_t blockX = (int)posX;
		int32_t blockY = (int)posY;
		int32_t layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		if(layerSL < settings -> place.layer && entityIQ -> qEnergy -1 >= 1) {
			entityIQ -> qEnergy -= 1;
			fieldIQ -> block[blockX][blockY][settings -> place.layer] = settings -> place.represent;
			fieldIQ -> edit = 1;
			syncBlock(settings -> place.represent, fieldIQ, blockX, blockY, settings -> place.layer);
			sendPacket(0x04, (void*)entityIQ, sizeof(entity_t), *playerIQ -> socket);
		}
	}
}

// Artifact Action
def_ArtifactAction(mineBlock) {
	if(playerIQ -> useTimer >= ((artifactSettings*)artifactVD) -> primaryUseTime * role[playerIQ -> role].mining_factor) {
		playerIQ -> useTimer = 0;
		int32_t blockX = (int32_t)posX;
		int32_t blockY = (int32_t)posY;
		int32_t layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		if(layerSL != -1) {
			entityIQ -> qEnergy += qEnergyRelevance(entityIQ -> qEnergy, &block[layerSL][fieldIQ -> block[blockX][blockY][layerSL]]);
			fieldIQ -> block[blockX][blockY][layerSL] = NO_WALL;
			fieldIQ -> edit = 1;
			syncBlock(NO_WALL, fieldIQ, blockX, blockY, layerSL);
			sendPacket(0x04, (void*)entityIQ, sizeof(entity_t), *playerIQ -> socket);
		}
	}
}

// Artifact Action
def_ArtifactAction(sliceEntity) {
	artifactSettings* artifactIQ = (artifactSettings*)artifactVD;
	FOR_EVERY_ENTITY {
		if(entityTable[entitySL] && entityInRange(entity + entitySL, entityIQ) && entity[entitySL].healthTimer > 1.0 && entityType[entity[entitySL].type].maxHealth != 0 && entity + entitySL != entityIQ) {
			double scale = QDIV_HITBOX_UNIT;
			while(scale <= 0.4) {
				if(isEntityPresent(entity + entitySL, entityIQ -> fldX, entityIQ -> fldY, entityIQ -> posX + cos(playerIQ -> useTimer * 6.28 / artifactIQ -> primaryUseTime) * scale, entityIQ -> posY + sin(playerIQ -> useTimer * 6.28 / artifactIQ -> primaryUseTime) * scale)) {
					if(damageEntity(entity + entitySL, settings -> slice.decay) && entity[entitySL].type == SHALLAND_SNAIL) {
						criterion_t criterionIQ = {KILL_SNAIL, ++playerIQ -> criterion[KILL_SNAIL]};
						sendPacket(0x09, (void*)&criterionIQ, sizeof(criterion_t), *playerIQ -> socket);
					}
					break;
				}
				scale += QDIV_HITBOX_UNIT;
			}
		}
	}
	if(playerIQ -> useTimer >= artifactIQ -> primaryUseTime) playerIQ -> useTimer = 0;
}

bool isEntityObstructed(entity_t* entityIQ, int32_t direction) {
	entity_st typeIQ = entityType[entityIQ -> type];
	if(!typeIQ.noClip && direction < NORTH_STOP) {
		double boxIQ = typeIQ.hitBox;
		double minX = 0;
		double minY = 0;
		double maxX = entityIQ -> posX + boxIQ * 0.5;
		double maxY = entityIQ -> posY + boxIQ * 0.5;
		int32_t lclX = 1;
		int32_t lclY = 1;
		switch(direction) {
			case NORTH:
				minY = entityIQ -> posY + boxIQ * 0.5;
				if(minY >= 128) {
					minY -= 128;
					lclY = 2;
				}
				minX = entityIQ -> posX - boxIQ * 0.5;
				while(minX <= maxX) {
					if(minX >= 128) {
						minX -= 128;
						maxX -= 128;
						lclX = 2;
					}
					if(!block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].traversable) {
						return true;
					}
					minX += QDIV_HITBOX_UNIT;
				}
				break;
			case EAST:
				minX = entityIQ -> posX + boxIQ * 0.5;
				if(minX >= 128) {
					minX -= 128;
					lclX = 2;
				}
				minY = entityIQ -> posY - boxIQ * 0.5;
				while(minY <= maxY) {
					if(minY >= 128) {
						minY -= 128;
						maxY -= 128;
						lclY = 2;
					}
					if(!block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].traversable) {
						return true;
					}
					minY += QDIV_HITBOX_UNIT;
				}
				break;
			case SOUTH:
				minY = entityIQ -> posY - boxIQ * 0.5;
				if(minY < 0) {
					minY += 128;
					lclY = 0;
				}
				minX = entityIQ -> posX - boxIQ * 0.5;
				while(minX <= maxX) {
					if(minX >= 128) {
						minX -= 128;
						maxX -= 128;
						lclX = 2;
					}
					if(!block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].traversable) {
						return true;
					}
					minX += QDIV_HITBOX_UNIT;
				}
				break;
			case WEST:
				minX = entityIQ -> posX - boxIQ * 0.5;
				if(minX < 0) {
					minX += 128;
					lclX = 0;
				}
				minY = entityIQ -> posY - boxIQ * 0.5;
				while(minY <= maxY) {
					if(minY >= 128) {
						minY -= 128;
						maxY -= 128;
						lclY = 2;
					}
					if(!block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].traversable) {
						return true;
					}
					minY += QDIV_HITBOX_UNIT;
				}
				break;
		}
	}
	return false;
}

double getMinSpeed(entity_t* entityIQ, int32_t direction) {
	return 0;
}

double getMaxSpeed(entity_t* entityIQ, int32_t direction) {
	return entityType[entityIQ -> type].speed;
}

bool changeDirection(entity_t* entityIQ, int32_t direction) {
	entity_st typeIQ = entityType[entityIQ -> type];
	double* inMotX = &entityIQ -> motX;
	double* inMotY = &entityIQ -> motY;
	entity_t entityCK = *entityIQ;
	switch(direction) {
		case NORTH:
			entityCK.posY += typeIQ.speed * 0.02;
			break;
		case EAST:
			entityCK.posX += typeIQ.speed * 0.02;
			break;
		case SOUTH:
			entityCK.posY -= typeIQ.speed * 0.02;
			break;
		case WEST:
			entityCK.posX -= typeIQ.speed * 0.02;
			break;
	}
	if(isEntityObstructed(&entityCK, direction)) return false;
	switch(direction) {
		case NORTH:
			if(*inMotX == 0) {
				*inMotY = typeIQ.speed;
			}else{
				*inMotY = typeIQ.speed * DIAGONAL_DOWN_FACTOR;
				*inMotX = (*inMotX) * DIAGONAL_DOWN_FACTOR;
			}
			break;
		case EAST:
			if(*inMotY == 0) {
				*inMotX = typeIQ.speed;
			}else{
				*inMotX = typeIQ.speed * DIAGONAL_DOWN_FACTOR;
				*inMotY = (*inMotY) * DIAGONAL_DOWN_FACTOR;
			}
			break;
		case SOUTH:
			if(*inMotX == 0) {
				*inMotY = typeIQ.speed * -1;
			}else{
				*inMotY = typeIQ.speed * DIAGONAL_DOWN_FACTOR * -1;
				*inMotX = (*inMotX) * DIAGONAL_DOWN_FACTOR;
			}
			break;
		case WEST:
			if(*inMotY == 0) {
				*inMotX = typeIQ.speed * -1;
			}else{
				*inMotX = typeIQ.speed * DIAGONAL_DOWN_FACTOR * -1;
				*inMotY = (*inMotY) * DIAGONAL_DOWN_FACTOR;
			}
			break;
		case NORTH_STOP:
			if(*inMotY > 0) {
				*inMotY = 0;
				if(*inMotX != 0) *inMotX = (*inMotX) * DIAGONAL_UP_FACTOR;
			}
			break;
		case EAST_STOP:
			if(*inMotX > 0) {
				*inMotX = 0;
				if(*inMotY != 0) *inMotY = (*inMotY) * DIAGONAL_UP_FACTOR;
			}
			break;
		case SOUTH_STOP:
			if(*inMotY < 0) {
				*inMotY = 0;
				if(*inMotX != 0) *inMotX = (*inMotX) * DIAGONAL_UP_FACTOR;
			}
			break;
		case WEST_STOP:
			if(*inMotX < 0) {
				*inMotX = 0;
				if(*inMotY != 0) *inMotY = (*inMotY) * DIAGONAL_UP_FACTOR;
			}
			break;
	}
	return true;
}

// Entity Action
void basicSnail(void* entityVD) {
	int32_t rng = rand();
	entity_t* entityIQ = (entity_t*)entityVD;
	if(rng % 1600 < 8) {
		changeDirection(entityIQ, rng % 8);
		syncEntity(entityIQ);
	}
}

void makeEntityTypes() {
	entityType[NULL_ENTITY] = makeEntityType(0, false, 0, false, 0.0, 1, NULL);
	entityType[PLAYER] = makeEntityType(5, true, 2, false, 32.0, 1, NULL);
	entityType[SHALLAND_SNAIL] = makeEntityType(5, false, 4, false, 0.25, 1, &basicSnail);
}

int32_t getFieldSlot(int32_t inZone, int32_t inFldX, int32_t inFldY) {
	int32_t fieldSL = 0;
	field_t fieldIQ;
	do{
		fieldIQ = field[fieldSL];
		fieldSL++;
	}while((fieldIQ.zone != inZone || fieldIQ.fldX != inFldX || fieldIQ.fldY != inFldY) && fieldSL < QDIV_MAX_FIELDS);
	if(fieldSL-- == QDIV_MAX_FIELDS) {
		return -1;
	}else if(fieldIQ.active == 1) {
		return fieldSL;
	}else{
		return fieldSL + QDIV_MAX_FIELDS;
	}
}

void setLocals(entity_t* entityIQ) {
	int32_t lclX = 0;
	int32_t lclY = 0;
	while(lclY < 3) {
		entityIQ -> local[lclX][lclY] = &field[getFieldSlot(entityIQ -> zone, entityIQ -> fldX + lclX -1, entityIQ -> fldY + lclY -1)];
		lclX++;
		if(lclX == 3) {
			lclX = 0;
			lclY++;
		}
	}
}

struct {
	struct osn_context* isOceanD;
	double IS_OCEAN_D_FACTOR;
	struct osn_context* isOceanC;
	double IS_OCEAN_C_FACTOR;
	struct osn_context* isOceanB;
	double IS_OCEAN_B_FACTOR;
	struct osn_context* isOceanA;
	double IS_OCEAN_A_FACTOR;
	struct osn_context* riverWebB;
	double RIVER_WEB_B_FACTOR;
	struct osn_context* riverWebA;
	double RIVER_WEB_A_FACTOR;
	struct osn_context* patching;
	double PATCHING_FACTOR;
	struct osn_context* crunch;
	double CRUNCH_FACTOR;
} mags;

void prepareGenerator() {
	open_simplex_noise(rand(), &mags.isOceanC);
	mags.IS_OCEAN_D_FACTOR = 0.0008;
	open_simplex_noise(rand(), &mags.isOceanC);
	mags.IS_OCEAN_C_FACTOR = 0.0009;
	open_simplex_noise(rand(), &mags.isOceanB);
	mags.IS_OCEAN_B_FACTOR = 0.0011;
	open_simplex_noise(rand(), &mags.isOceanA);
	mags.IS_OCEAN_A_FACTOR = 0.0012;
	open_simplex_noise(rand(), &mags.riverWebB);
	mags.RIVER_WEB_B_FACTOR = 0.02;
	open_simplex_noise(rand(), &mags.riverWebA);
	mags.RIVER_WEB_A_FACTOR = 0.022;
	open_simplex_noise(rand(), &mags.patching);
	mags.PATCHING_FACTOR = 0.1;
	open_simplex_noise(rand(), &mags.crunch);
	mags.CRUNCH_FACTOR = 1;
}

#define QDIV_NOISE(ctx, factor) open_simplex_noise2(ctx, (double)(fieldIQ -> fldX * 128 + posX) * factor, (double)(fieldIQ -> fldY * 128 + posY) * factor)

void useGenerator(field_t* fieldIQ) {
	memset(fieldIQ -> biome, 0x00, sizeof(fieldIQ -> biome));
	memset(fieldIQ -> block, 0x00, sizeof(fieldIQ -> block));
	int32_t posX = 0;
	int32_t posY = 0;
	uint16_t biomeIQ, floorIQ, wallIQ;
	double isOcean, riverWeb, patching, crunch;
	while(posY < 128) {
		isOcean = (QDIV_NOISE(mags.isOceanA, mags.IS_OCEAN_A_FACTOR) + QDIV_NOISE(mags.isOceanB, mags.IS_OCEAN_B_FACTOR) + QDIV_NOISE(mags.isOceanB, mags.IS_OCEAN_C_FACTOR) + QDIV_NOISE(mags.isOceanB, mags.IS_OCEAN_D_FACTOR)) * 0.25;
		riverWeb = (QDIV_NOISE(mags.riverWebA, mags.RIVER_WEB_A_FACTOR) + QDIV_NOISE(mags.riverWebB, mags.RIVER_WEB_B_FACTOR)) * 0.5;
		patching = QDIV_NOISE(mags.patching, mags.PATCHING_FACTOR);
		crunch = QDIV_NOISE(mags.crunch, mags.CRUNCH_FACTOR);
		floorIQ = NO_FLOOR;
		wallIQ = NO_WALL;
		// WORLDGEN
		if(isOcean < 0.19 || (isOcean < 0.2 && (riverWeb < -0.08 - pow((isOcean - 0.19) * 10, 2) * 0.92 || riverWeb > 0.08 + pow((isOcean - 0.19) * 10, 2) * 0.92))) {
			biomeIQ = SHALLAND;
			if(riverWeb < -0.08 || riverWeb > 0.08) {
				floorIQ = SHALLAND_FLATGRASS;
				if(patching > 0.25) {
					if(crunch > 0.75) {
						wallIQ = SHALLAND_BUSH;
					}else if(crunch > 0.25){
						wallIQ = SHALLAND_GRASS;
					}
				}
			}else{
				floorIQ = WATER;
			}
		}else{
			biomeIQ = OCEAN;
			floorIQ = WATER;
		}
		fieldIQ -> biome[posX][posY] = biomeIQ;
		fieldIQ -> block[posX][posY][0] = floorIQ;
		fieldIQ -> block[posX][posY][1] = wallIQ;
		// END OF WORLDGEN
		posX++;
		if(posX == 128) {
			posX = 0;
			posY++;
		}
	}
}

int32_t loadField(int32_t inZone, int32_t inFldX, int32_t inFldY) {
	int32_t fieldSL = getFieldSlot(inZone, inFldX, inFldY);
	if(fieldSL == -1 || fieldSL >= QDIV_MAX_FIELDS) {
		field_t fieldIQ;
		fieldIQ.active = 1;
		fieldIQ.edit = 0;
		fieldIQ.zone = inZone;
		fieldIQ.fldX = inFldX;
		fieldIQ.fldY = inFldY;
		if(fieldSL >= QDIV_MAX_FIELDS && inZone != 0) {
			fieldSL -= QDIV_MAX_FIELDS;
			field[fieldSL].active = 1;
		}else{
			for(fieldSL = 0; fieldSL < QDIV_MAX_FIELDS; fieldSL++) {
				if(field[fieldSL].active == 0) {
					int8_t fileName[64];
					sprintf(fileName, "world/fld.%dz.%dx.%dy.dat", inZone, inFldX, inFldY);
					FILE* fileData = fopen(fileName, "rb");
					if(fileData == NULL) {
						useGenerator(&fieldIQ);
					}else{
						fread(fieldIQ.block, 1, sizeof(fieldIQ.block), fileData);
						fclose(fileData);
					}
					fieldIQ.slot = fieldSL;
					field[fieldSL] = fieldIQ;
					break;
				}
			}
		}
		entity_t* entityIQ;
		for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
			entityIQ = entity + entitySL;
			if(entityTable[entitySL] &&
			entityIQ -> zone == inZone &&
			entityIQ -> fldX == inFldX &&
			entityIQ -> fldY == inFldY) {
				entityIQ -> active = 1;
			}
		}
	}
	return fieldSL;
}

int32_t spawnEntity(int8_t* name, uint8_t* uuid, int32_t inType, int32_t inZone, int32_t inFldX, int32_t inFldY, double inPosX, double inPosY) {
	entity_t entityIQ;
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(!entityTable[entitySL] && entity[entitySL].type != NULL_ENTITY) {
			memset(entity + entitySL, 0x00, sizeof(entity_t));
			FILE* entityFile;
			int8_t hex[QDIV_B16 + 1];
			int8_t fileName[QDIV_B16 + 11];
			if(uuid != NULL) {
				uuidToHex(uuid, hex);
				sprintf(fileName, "entity/%s.dat", hex);
				entityFile = fopen(fileName, "rb");
				if(entityFile != NULL) {
					fread(&entityIQ, 1, sizeof(entity_t), entityFile);
					fclose(entityFile);
					if(!entityType[inType].persistant) nonsense(__LINE__);
					goto oldEntity;
				}
			}else{
				nonsense(__LINE__);
				return -1;
			}
			strcpy(entityIQ.name, name);
			memcpy(entityIQ.uuid, uuid, QDIV_B256);
			entityIQ.slot = entitySL;
			entityIQ.active = 1;
			entityIQ.zone = inZone;
			entityIQ.fldX = inFldX;
			entityIQ.fldY = inFldY;
			entityIQ.posX = inPosX;
			entityIQ.posY = inPosY;
			entityIQ.motX = 0;
			entityIQ.motY = 0;
			entityIQ.health = entityType[inType].maxHealth;
			entityIQ.healthTimer = 60.0;
			entityIQ.qEnergy = 40;
			entityIQ.type = inType;
			switch(inType) {
				case PLAYER:
					player_t* playerIQ = &entityIQ.unique.Player;
					playerIQ -> role = BUILDER;
					playerIQ -> roleTimer = 0;
					playerIQ -> artifact = OLD_SWINGER;
					playerIQ -> useTimer = 0;
					break;
			}
			oldEntity:
			entityIQ.slot = entitySL;
			if(entityIQ.type != PLAYER) setLocals(&entityIQ);
			entityIQ.active = 1;
			entity[entitySL] = entityIQ;
			entityTable[entitySL] = true;
			return entitySL;
		}
	}
	return -1;
}

void disconnect(int32_t sockSL) {
	QDIV_CLOSE(client[sockSL].socket);
	client[sockSL].socket = 0;
}

// Thread
void* thread_gate() {
	#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	#else
	signal(SIGPIPE, SIG_IGN);
	#endif
    struct sockaddr_in6 address;
    int32_t addrlen = sizeof(address);
    int32_t sockSF = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    int32_t sockIQ, sockMX, sockSL, sockAM, sockCK;
    fd_set sockRD;
    struct timeval qTimeOut;
    bool sockTRUE = true;
    bool sockFALSE = false;
    #ifdef _WIN32
    setsockopt(sockSF, SOL_SOCKET, SO_REUSEADDR, (const char*)&sockTRUE, sizeof(sockTRUE));
    setsockopt(sockSF, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&sockFALSE, sizeof(sockFALSE));
    #else
    setsockopt(sockSF, SOL_SOCKET, SO_REUSEADDR, &sockTRUE, sizeof(sockTRUE));
    setsockopt(sockSF, IPPROTO_IPV6, IPV6_V6ONLY, &sockFALSE, sizeof(sockFALSE));
    #endif
    memset(&address, 0x00, sizeof(address));
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_any;
    address.sin6_port = htons(QDIV_PORT);
    bind(sockSF, (struct sockaddr*)&address, sizeof(address));
    listen(sockSF, 3);
    while(*pShutdown != 1) {
        FD_ZERO(&sockRD);
        FD_SET(sockSF, &sockRD);
        sockMX = sockSF;
        for(sockSL = 0; sockSL < QDIV_MAX_PLAYERS; sockSL++) {
            if(client[sockSL].socket == 0) {
            	if(client[sockSL].entity != NULL) memset(client + sockSL, 0x00, sizeof(client_t));
            }else{
            	sockIQ = client[sockSL].socket;
		        FD_SET(sockIQ, &sockRD);
		        if(sockIQ > sockMX) sockMX = sockIQ;
		    }
        }
		qTimeOut.tv_sec = 1;
        if(select(sockMX+1, &sockRD, NULL, NULL, &qTimeOut) > 0) {
        	if(FD_ISSET(sockSF, &sockRD)) {
        		sockSL = 0;
        		while(client[sockSL].socket != 0) {
        			if(sockSL == QDIV_MAX_PLAYERS) goto failed;
        			sockSL++;
        		}
        		client[sockSL].socket = accept(sockSF, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        		client[sockSL].entity = NULL;
			}
			failed:
        	for(sockSL = 0; sockSL < QDIV_MAX_PLAYERS; sockSL++) {
            	if(client[sockSL].socket != 0 && FD_ISSET(client[sockSL].socket, &sockRD)) {
            	    if(recv(client[sockSL].socket, recvBF, QDIV_PACKET_SIZE, 0) < 1) {
            	        QDIV_CLOSE(client[sockSL].socket);
            	        client[sockSL].socket = 0;
            	        removeEntity(sockSL);
            	        syncEntityRemoval(sockSL);
            	        printf("> %s disconnected\n", client[sockSL].name);
            	    }else{
            	        entity_t* entityIQ = client[sockSL].entity;
            	        player_t* playerIQ = &entityIQ -> unique.Player;
            	        if(entityIQ == NULL) {
            	        	switch(recvBF[4]) {
            	        		case 0x0C:
							    	memcpy(client[sockSL].externalKey, recvBF+5, ECC_PUB_KEY_SIZE);
							    	QDIV_RANDOM(client[sockSL].privateKey, ECC_PRV_KEY_SIZE);
							    	ecdh_generate_keys(client[sockSL].internalKey, client[sockSL].privateKey);
									ecdh_shared_secret(client[sockSL].privateKey, client[sockSL].externalKey, client[sockSL].sharedKey);
									sendPacket(0x0C, (void*)client[sockSL].internalKey, ECC_PUB_KEY_SIZE, client[sockSL].socket);
									client[sockSL].keyAvailable = true;
									break;
								case 0x0D:
									if(client[sockSL].keyAvailable) {
										memcpy(client[sockSL].name, recvBF+5, 16);
										memcpy(client[sockSL].uuid, recvBF+21, 16);
										for(int32_t cryptSL = 0; cryptSL < ECC_PRV_KEY_SIZE; cryptSL++) client[sockSL].uuid[cryptSL % 16] ^= client[sockSL].sharedKey[cryptSL];
										int8_t fileName[35];
										sprintf(fileName, "player/identity/%s.qid", client[sockSL].name);
										FILE* identityFile = fopen(fileName, "rb");
										if(identityFile == NULL) {
											disconnect(sockSL);
											printf("> %s rejected: Not whitelisted\n", client[sockSL].name);
											break;
										}
										uint8_t uuidIQ[16];
										fread(uuidIQ, 1, 16, identityFile);
										fclose(identityFile);
										if(memcmp(client[sockSL].uuid, uuidIQ, 16)) {
											disconnect(sockSL);
											printf("> %s rejected: Name not authentic\n", client[sockSL].name);
											break;
										}
										sockIQ = client[sockSL].socket;
										int32_t entitySL = spawnEntity(client[sockSL].name, client[sockSL].uuid, PLAYER, 0, 0, 0, 0, 0);
										entity_t* entityIQ = entity + entitySL;
										client[sockSL].entity = entityIQ;
										player_t* playerIQ = &entityIQ -> unique.Player;
										playerIQ -> socket = &client[sockSL].socket;
										playerIQ -> criterion = (uint32_t*)malloc(MAX_CRITERION * sizeof(uint32_t));
										sprintf(fileName, "criterion/CR.%s.dat", client[sockSL].name);
										FILE* criterionFile = fopen(fileName, "rb");
										if(criterionFile == NULL) {
											memset(playerIQ -> criterion, 0x00, MAX_CRITERION * sizeof(uint32_t));
										}else{
											fread(playerIQ -> criterion, sizeof(uint32_t), MAX_CRITERION, criterionFile);
											fclose(criterionFile);
										}
										loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY);
										loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY);
										loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY);
										loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY+1);
										loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY+1);
										loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY+1);
										loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY-1);
										loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY-1);
										loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY-1);
										setLocals(entityIQ);
										sendPacket(0x06, (void*)entityIQ, sizeof(entity_t), sockIQ);
										sendPacket(0x0B, (void*)&currentHour, sizeof(uint8_t), sockIQ);
										syncEntityField(entityIQ -> local[0][0], sockIQ);
										syncEntityField(entityIQ -> local[1][0], sockIQ);
										syncEntityField(entityIQ -> local[2][0], sockIQ);
										syncEntityField(entityIQ -> local[0][1], sockIQ);
										syncEntityField(entityIQ -> local[1][1], sockIQ);
										syncEntityField(entityIQ -> local[2][1], sockIQ);
										syncEntityField(entityIQ -> local[0][2], sockIQ);
										syncEntityField(entityIQ -> local[1][2], sockIQ);
										syncEntityField(entityIQ -> local[2][2], sockIQ);
										printf("> %s connected\n", client[sockSL].name);
									}else{
										int8_t nameIQ[16];
										memcpy(nameIQ, recvBF+5, 16);
										disconnect(sockSL);
										printf("> %s rejected: Encryption not ready\n", nameIQ);
									}
									break;
								default:
									disconnect(sockSL);
									printf("> %s rejected: Unexpected Packet\n", client[sockSL].name);
									break;
							}
            	        }else{
		        	        switch(recvBF[4]) {
		        	        	case 0x02:
		        	        		int32_t direction;
		        	        		memcpy(&direction, recvBF+5, sizeof(int));
		        	        		if(changeDirection(entityIQ, direction)) syncEntity(entityIQ);
		        	        		break;
		        	        	case 0x05:
		        	        		uint8_t fieldBF[32768];
		        	        		segment_t segmentIQ;
		        	        		memcpy(&segmentIQ, recvBF+5, sizeof(segment_t));
		        	        		memcpy(fieldBF, entityIQ -> local[segmentIQ.lclX][segmentIQ.lclY] -> block[segmentIQ.posX], 32768);
									send(*playerIQ -> socket, fieldBF, 32768, 0);
									break;
		        	        	case 0x08:
		        	        		int32_t roleIQ, artifactIQ;
		        	        		memcpy(&roleIQ, recvBF+5, sizeof(int));
		        	        		memcpy(&artifactIQ, recvBF+9, sizeof(int));
		        	        		if(isArtifactUnlocked(roleIQ, artifactIQ, entityIQ)) {
		        	        			playerIQ -> roleTimer = 3600.0;
		        	        			playerIQ -> role = roleIQ;
		        	        			playerIQ -> artifact = artifactIQ;
		        	        			sendPacket(0x04, (void*)entityIQ, sizeof(entity_t), *playerIQ -> socket);
		        	        		}
		        	        		break;
		        	        	case 0x0A:
		        	        		usage_t usageIQ;
		        	        		memcpy(&usageIQ, recvBF+5, sizeof(usage_t));
		    	        			playerIQ -> currentUsage = usageIQ.usage;
			    	        		playerIQ -> useRelX = usageIQ.useRelX;
			    	        		playerIQ -> useRelY = usageIQ.useRelY;
			    	        		syncEntity(entityIQ);
				    	        	break;
				    	        case 0x0E:
				    	        	entityIQ -> motX = 0;
				    	        	entityIQ -> motY = 0;
				    	        	playerIQ -> currentUsage = NO_USAGE;
				    	        	syncEntity(entityIQ);
				    	        	break;
							}
            	        }
            	    }
            	}
            }
        }
    }
    for(sockSL = 0; sockSL < QDIV_MAX_PLAYERS; sockSL++) {
        if(client[sockSL].socket > 0) {
            disconnect(sockSL);
            removeEntity(client[sockSL].entity -> slot);
            printf("> %s disconnected", client[sockSL].name);
        }
    }
    #ifdef _WIN32
    WSACleanup();
    #endif
}

// Thread
void* thread_console() {
	int8_t prompt[64];
	while(1) {
		scanf("%64s", prompt);
		if(strcmp(prompt, "shutdown") == 0) {
			*pShutdown = 1;
			break;
		}
		if(strcmp(prompt, "report") == 0) {
			int32_t repClk;
			printf("Tick Time: %d µs / 20000 µs\n\n", *pTickTime);
			puts("–– Entity Table ––");
			for(repClk = 0; repClk < 400; repClk++) {
				printf("%d ", entityTable[repClk]);
				if((repClk+1) % 20 == 0) {
					printf("\n");
				}
			}
			printf("\n");
			puts("–– Client Table ––");
			for(repClk = 0; repClk < 64; repClk++) {
				printf("%d ", entity[repClk].type == PLAYER ? *entity[repClk].unique.Player.socket : 0);
				if((repClk+1) % 8 == 0) {
					printf("\n");
				}
			}
			puts("–– Field Table ––");
			int32_t fieldTable[9][9] = {0};
			for(repClk = 0; repClk < QDIV_MAX_FIELDS; repClk++) {
				if(field[repClk].active && field[repClk].fldX > -5 && field[repClk].fldX < 5 && field[repClk].fldY > -5 && field[repClk].fldY < 5) {
					fieldTable[field[repClk].fldX+4][field[repClk].fldY+4] = 1;
				}
			}
			int32_t hlpClk;
			for(hlpClk = 9; hlpClk >= 0; hlpClk--) {
				for(repClk = 0; repClk < 9; repClk++) {
					printf("%d ", fieldTable[repClk][hlpClk]);
				}
				printf("\n");
			}
			printf("X: %f, Y: %f, FX: %d, FY: %d\n", entity[0].posX, entity[0].posY, entity[0].fldX, entity[0].fldY);
			memset(prompt, '\0', 32 * sizeof(int8_t));
		}
		if(strcmp(prompt, "query") == 0) {
			if(*pTickQuery) {
				puts("[Query] Disabled");
				*pTickQuery = false;
			}else{
				puts("[Query] Enabled");
				*pTickQuery = true;
			}
			memset(prompt, '\0', 32 * sizeof(int8_t));
		}
		if(strcmp(prompt, "snail") == 0) {
			spawnEntity("Snail", uuidNull, SHALLAND_SNAIL, entity[0].zone, entity[0].fldX, entity[0].fldY, entity[0].posX, entity[0].posY);
		}
	}
}

int32_t main() {
	printf("\n–––– qDivServer-%d.%c ––––\n\n", QDIV_VERSION, QDIV_BRANCH);
	memset(entity, 0x00, sizeof(entity));
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) entity[entitySL].type = -1;
	memset(client, 0x00, sizeof(client));
	QDIV_MKDIR("criterion");
	QDIV_MKDIR("entity");
	QDIV_MKDIR("player");
	QDIV_MKDIR("player/identity");
	QDIV_MKDIR("world");
	libMain();
	makeEntityTypes();
	puts("> Starting Threads");
	pthread_t console_id, gate_id;
	pthread_create(&gate_id, NULL, thread_gate, NULL);
	pthread_create(&console_id, NULL, thread_console, NULL);
	puts("> Initializing OpenSimplex");
	srand(3284394);
	prepareGenerator();
	srand(time(NULL));
	puts("> Setting up Tick Stamper");
	struct timeval qTickStart;
	struct timeval qTickEnd;
	while(qShutdown != 1) {
		gettimeofday(&qTickEnd, NULL);
		if(qTickStart.tv_usec > qTickEnd.tv_usec) qTickEnd.tv_usec += 1000000;
		qTickTime = qTickEnd.tv_usec - qTickStart.tv_usec;
		qFactor = qTickTime < 20000 ? 0.02 : (double)qTickTime * 0.000001;
		time_t serverTime = time(NULL);
		if(localtime(&serverTime) -> tm_hour != currentHour) currentHour = localtime(&serverTime) -> tm_hour;
		if(qTickTime < 20000) usleep(20000 - qTickTime);
		gettimeofday(&qTickStart, NULL);
		if(TickQuery) printf("> Tick: %d\n", qTickStart.tv_usec);
		for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
			if(entityTable[entitySL] && entity[entitySL].active == 1) {
				entity_t* entityIQ = entity + entitySL;
				entityIQ -> posX += entityIQ -> motX * qFactor;
				if(entityIQ -> motX > 0 && isEntityObstructed(entityIQ, EAST)) {
					while(isEntityObstructed(entityIQ, EAST)) {
						entityIQ -> posX -= 0.01;
					}
					entityIQ -> motX = 0;
					syncEntity(entityIQ);
				}
				if(entityIQ -> motX < 0 && isEntityObstructed(entityIQ, WEST)) {
					while(isEntityObstructed(entityIQ, WEST)) {
						entityIQ -> posX += 0.01;
					}
					entityIQ -> motX = 0;
					syncEntity(entityIQ);
				}
				entityIQ -> posY += entityIQ -> motY * qFactor;
				if(entityIQ -> motY > 0 && isEntityObstructed(entityIQ, NORTH)) {
					while(isEntityObstructed(entityIQ, NORTH)) {
						entityIQ -> posY -= 0.01;
					}
					entityIQ -> motY = 0;
					syncEntity(entityIQ);
				}
				if(entityIQ -> motY < 0 && isEntityObstructed(entityIQ, SOUTH)) {
					while(isEntityObstructed(entityIQ, SOUTH)) {
						entityIQ -> posY += 0.01;
					}
					entityIQ -> motY = 0;
					syncEntity(entityIQ);
				}
				int32_t sockIQ;
				if(entityIQ -> type == PLAYER) sockIQ = *entityIQ -> unique.Player.socket;
				if(entityIQ -> posX >= 128) {
					entityIQ -> posX = -128 + entityIQ -> posX;
					entityIQ -> fldX++;
					if(entityIQ -> type == PLAYER) {
						unloadField(entityIQ -> zone, entityIQ -> fldX-2, entityIQ -> fldY);
						unloadField(entityIQ -> zone, entityIQ -> fldX-2, entityIQ -> fldY+1);
						unloadField(entityIQ -> zone, entityIQ -> fldX-2, entityIQ -> fldY-1);
						loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY);
						loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY+1);
						loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY-1);
					}
					setLocals(entityIQ);
					syncEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						syncEntityField(entityIQ -> local[2][0], sockIQ);
						syncEntityField(entityIQ -> local[2][1], sockIQ);
						syncEntityField(entityIQ -> local[2][2], sockIQ);
					}
				}
				if(entityIQ -> posX < 0) {
					entityIQ -> posX = 128 + entityIQ -> posX;
					entityIQ -> fldX--;
					if(entityIQ -> type == PLAYER) {
						unloadField(entityIQ -> zone, entityIQ -> fldX+2, entityIQ -> fldY);
						unloadField(entityIQ -> zone, entityIQ -> fldX+2, entityIQ -> fldY+1);
						unloadField(entityIQ -> zone, entityIQ -> fldX+2, entityIQ -> fldY-1);
						loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY);
						loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY+1);
						loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY-1);
					}
					setLocals(entityIQ);
					syncEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						syncEntityField(entityIQ -> local[0][0], sockIQ);
						syncEntityField(entityIQ -> local[0][1], sockIQ);
						syncEntityField(entityIQ -> local[0][2], sockIQ);
					}
				}
				if(entityIQ -> posY >= 128) {
					entityIQ -> posY = -128 + entityIQ -> posY;
					entityIQ -> fldY++;
					if(entityIQ -> type == PLAYER) {
						unloadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY-2);
						unloadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY-2);
						unloadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY-2);
						loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY+1);
						loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY+1);
						loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY+1);
					}
					setLocals(entityIQ);
					syncEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						syncEntityField(entityIQ -> local[0][2], sockIQ);
						syncEntityField(entityIQ -> local[1][2], sockIQ);
						syncEntityField(entityIQ -> local[2][2], sockIQ);
					}
				}
				if(entityIQ -> posY < 0) {
					entityIQ -> posY = 128 + entityIQ -> posY;
					entityIQ -> fldY--;
					if(entityIQ -> type == PLAYER) {
						unloadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY+2);
						unloadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY+2);
						unloadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY+2);
						loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY-1);
						loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY-1);
						loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY-1);
					}
					setLocals(entityIQ);
					syncEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						syncEntityField(entityIQ -> local[0][0], sockIQ);
						syncEntityField(entityIQ -> local[1][0], sockIQ);
						syncEntityField(entityIQ -> local[2][0], sockIQ);
					}
				}
				if(entityIQ -> type == PLAYER) {
					player_t* playerIQ = &entityIQ -> unique.Player;
					if(playerIQ -> currentUsage == NO_USAGE) {
						playerIQ -> useTimer = 0;
					}else{
						playerIQ -> useTimer += qFactor;
						int32_t lclX = 1;
						int32_t lclY = 1;
						double posX = entityIQ -> posX + playerIQ -> useRelX;
						double posY = entityIQ -> posY + playerIQ -> useRelY;
						if(posX < 0) {
							posX += 128;
							lclX = 0;
						}else if(posX >= 128) {
							posX -= 128;
							lclX = 2;
						}
						if(posY < 0) {
							posY += 128;
							lclY = 0;
						}else if(posY >= 128) {
							posY -= 128;
							lclY = 2;
						}
						artifactSettings* artifactIQ = &artifact[playerIQ -> role][playerIQ -> artifact];
						if(playerIQ -> currentUsage == PRIMARY_USAGE && artifactIQ -> primary != NULL) {
							(*artifactIQ -> primary)(entityIQ -> local[lclX][lclY], posX, posY, entityIQ, playerIQ, (void*)artifactIQ, &artifactIQ -> primarySettings);
						}else if(playerIQ -> currentUsage == SECONDARY_USAGE && artifactIQ -> secondary != NULL) {
							(*artifactIQ -> secondary)(entityIQ -> local[lclX][lclY], posX, posY, entityIQ, playerIQ, (void*)artifactIQ, &artifactIQ -> secondarySettings);
						}else playerIQ -> useTimer = 0;
					}
					if(playerIQ -> roleTimer > 0.0) playerIQ -> roleTimer -= qFactor;
				}
				if(entityType[entityIQ -> type].action != NULL) (*entityType[entityIQ -> type].action)((void*)entityIQ);
				if(entityIQ -> healthTimer < 60.0) {
					entityIQ -> healthTimer += qFactor;
				}
			}
		}
	}
	pthread_join(gate_id, NULL);
	pthread_join(console_id, NULL);
}
