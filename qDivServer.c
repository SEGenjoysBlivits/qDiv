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
#else
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<signal.h>
#endif
#include "qDivLib.h"
#include "include/open-simplex-noise.h"

typedef struct {
	int represent;
	int layer;
} PlaceBlock;
typedef struct {
	int decay;
	bool miner;
} SliceEntity;

typedef union {
	PlaceBlock place;
	SliceEntity slice;
} uniqueActionSettings;

typedef void(*artifactAction)(fieldData*, double, double, entityData*, playerData*, void*, uniqueActionSettings*);

typedef struct {
	artifactAction primary;
	artifactAction secondary;
	unsigned long long primaryCost;
	unsigned long long secondaryCost;
	double primaryUseTime;
	double secondaryUseTime;
	uniqueActionSettings primarySettings;
	uniqueActionSettings secondarySettings;
	unsigned long long qEnergy;
	struct {
		int Template;
		unsigned int value;
	} criterion[QDIV_ARTIFACT_CRITERIA];
} artifactSettings;

int qShutdown = 0;
int* pShutdown = &qShutdown;

int qTickTime;
int* pTickTime = &qTickTime;
bool TickQuery = false;
bool* pTickQuery = &TickQuery;

fieldData field[QDIV_MAX_FIELDS];
struct osn_context* simplexContext;
struct {
	struct osn_context* large_feature;
	double LARGE_FEATURE_FACTOR;
	struct osn_context* patching;
	double PATCHING_FACTOR;
	struct osn_context* crunch;
	double CRUNCH_FACTOR;
} mags;
artifactSettings artifact[6][4000];

unsigned int criterion[QDIV_MAX_PLAYERS][MAX_CRITERION];

// Packet
void makeEntityRemovalPacket(int slot) {
	SendPacket[0] = 0x03;
	memcpy(SendPacket+1, &slot, sizeof(int));
}

// Packet
void makeEntityUpdatePacket(entityData* entityIQ) {
	SendPacket[0] = 0x04;
	memcpy(SendPacket+1, entityIQ, sizeof(entityData));
}

// Packet
void makeCriterionPacket(unsigned int templateIQ, unsigned int* criterion) {
	SendPacket[0] = 0x09;
	memcpy(SendPacket+1, &templateIQ, sizeof(int));
	memcpy(SendPacket+5, criterion + templateIQ, sizeof(int));
}

// Packet
void makeLightTaskPacket() {
	SendPacket[0] = 0x0B;
	SendPacket[1] = (char)currentHour;
}

// Synchronizer
void syncEntityRemoval(int slot) {
	makeEntityRemovalPacket(slot);
	for(int entitySL = 0; entitySL < QDIV_MAX_PLAYERS; entitySL++) {
		if(entityInRange(entity + slot, entity + entitySL)) send(entity[entitySL].unique.Player.socket, SendPacket, QDIV_PACKET_SIZE, 0);
	}
}

// Synchronizer
void syncEntity(entityData* entityIQ) {
	makeEntityUpdatePacket(entityIQ);
	for(int entitySL = 0; entitySL < QDIV_MAX_PLAYERS; entitySL++) {
		if(entityInRange(entityIQ, entity + entitySL)) send(entity[entitySL].unique.Player.socket, SendPacket, QDIV_PACKET_SIZE, 0);
	}
}

// Synchronizer
void syncEntityField(fieldData* fieldIQ, int sockIQ) {
	entityData entityIQ;
	FOR_EVERY_ENTITY {
		if(entitySlot[entitySL] == 1) {
			entityIQ = entity[entitySL];
			if(entityIQ.zone == fieldIQ -> zone && entityIQ.fldX == fieldIQ -> fldX && entityIQ.fldY == fieldIQ -> fldY) {
				makeEntityUpdatePacket(&entityIQ);
				send(sockIQ, SendPacket, QDIV_PACKET_SIZE, 0);
			}
		}
	}
}

// Synchronizer
void syncBlock(int blockIQ, fieldData* fieldIQ, int posX, int posY, int layer) {
	makeBlockPacket(blockIQ, fieldIQ -> fldX, fieldIQ -> fldY, posX, posY, layer);
	for(int entitySL = 0; entitySL < QDIV_MAX_PLAYERS; entitySL++) {
		if(entity[entitySL].local[0][0] == fieldIQ || entity[entitySL].local[0][1] == fieldIQ || entity[entitySL].local[0][2] == fieldIQ || entity[entitySL].local[1][0] == fieldIQ || entity[entitySL].local[1][1] == fieldIQ || entity[entitySL].local[1][2] == fieldIQ || entity[entitySL].local[2][0] == fieldIQ || entity[entitySL].local[2][1] == fieldIQ || entity[entitySL].local[2][2] == fieldIQ) send(entity[entitySL].unique.Player.socket, SendPacket, QDIV_PACKET_SIZE, 0);
	}
}

void syncTime() {
	makeLightTaskPacket();
	for(int entitySL = 0; entitySL < QDIV_MAX_PLAYERS; entitySL++) {
		if(entitySlot[entitySL] == 1 && entity[entitySL].type == PLAYER) send(entity[entitySL].unique.Player.socket, SendPacket, QDIV_PACKET_SIZE, 0);
	}
}

void unloadField(int inZone, int inFldX, int inFldY) {
	for(int fieldSL = 0; fieldSL < QDIV_MAX_FIELDS; fieldSL++) {
		fieldData fieldIQ = field[fieldSL];
		if(fieldIQ.zone == inZone && fieldIQ.fldX == inFldX && fieldIQ.fldY == inFldY) {
			int entitySL = 0;
			entityData entityIQ = entity[entitySL];
			while((entityIQ.type != PLAYER || entityIQ.active != 1 || entityIQ.zone != fieldIQ.zone || entityIQ.fldX > fieldIQ.fldX+1 || entityIQ.fldX < fieldIQ.fldX-1 || entityIQ.fldY > fieldIQ.fldY+1 || entityIQ.fldY < fieldIQ.fldY-1) && entitySL < QDIV_MAX_PLAYERS) {
				entitySL++;
				entityIQ = entity[entitySL];
			}
			if(entitySL == QDIV_MAX_PLAYERS) {
				field[fieldSL].active = 0;
				if(field[fieldSL].edit == 1) {
					char fileName[64];
					sprintf(fileName, "world/fld.%dz.%dx.%dy.dat", inZone, inFldX, inFldY);
					FILE* fileData = fopen(fileName, "wb");
					fwrite(field[fieldSL].block, 1, sizeof(field[fieldSL].block), fileData);
					fclose(fileData);
				}
			}
			break;
		}
	}
}

void removeEntity(int slot) {
	entity[slot].active = 0;
	entitySlot[slot] = 0;
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
		if(slot >= QDIV_MAX_PLAYERS) nonsense(262);
	}
}

void damageEntity(entityData* entityIQ, unsigned long long decay) {
	entityIQ -> healthTimer = 0;
	entityIQ -> health--;
	if(entityIQ -> health == 0 || decay >= entityIQ -> qEnergy) {
		if(entityIQ -> type == PLAYER) {
			entityIQ -> health = 5;
			entityIQ -> qEnergy = 1;
		}else{
			removeEntity(entityIQ -> slot);
			syncEntityRemoval(entityIQ -> slot);
		}
	}else{
		entityIQ -> qEnergy -= decay;
	}
	if(entityIQ -> type == PLAYER) {
		makeEntityUpdatePacket(entityIQ);
		send(entityIQ -> unique.Player.socket, SendPacket, QDIV_PACKET_SIZE, 0);
	}
}

bool isArtifactUnlocked(int roleSL, int artifactSL, entityData* entityIQ) {
	if(entityIQ -> qEnergy < artifact[roleSL][artifactSL].qEnergy) return false;
	for(int criterionSL = 0; criterionSL < QDIV_ARTIFACT_CRITERIA; criterionSL++) {
		int TemplateSL = artifact[roleSL][artifactSL].criterion[criterionSL].Template;
		if(TemplateSL != NO_CRITERION && artifact[roleSL][artifactSL].criterion[criterionSL].value > entityIQ -> unique.Player.criterion[TemplateSL]) return false;
	}
	return true;
}

bool isEntityPresent(entityData* entityIQ, int fldX, int fldY, double posX, double posY) {
	double boxIQ = entityType[entityIQ -> type].hitBox;
	return abs(entityIQ -> posX - posX + (double)((entityIQ -> fldX - fldX) * 128)) < boxIQ && abs(entityIQ -> posY - posY + (double)((entityIQ -> fldY - fldY) * 128)) < boxIQ;
}

bool isEntityPresentInRange(entityData* entityIQ, int fldX, int fldY, double centerX, double centerY, double range) {
	double boxIQ = entityType[entityIQ -> type].hitBox;
	double posRelX = (double)((entityIQ -> fldX - fldX) * 128);
	double posRelY = (double)((entityIQ -> fldY - fldY) * 128);
	return entityIQ -> posX + boxIQ + posRelX > centerX - range && entityIQ -> posX - boxIQ + posRelX < centerX + range && entityIQ -> posY + boxIQ + posRelY > centerY - range && entityIQ -> posY - boxIQ + posRelY < centerY + range;
}

// Artifact Action
void placeBlock(fieldData* fieldIQ, double posX, double posY, entityData* entityIQ, playerData* playerIQ, void* artifactVD, uniqueActionSettings* settings) {
	if(playerIQ -> useTimer >= 0.1) {
		artifactSettings* artifactIQ = (artifactSettings*)artifactVD;
		playerIQ -> useTimer = 0;
		int blockX = (int)posX;
		int blockY = (int)posY;
		int layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		if(layerSL < settings -> place.layer && entityIQ -> qEnergy -1 >= 1) {
			entityIQ -> qEnergy -= 1;
			fieldIQ -> block[blockX][blockY][settings -> place.layer] = settings -> place.represent;
			fieldIQ -> edit = 1;
			syncBlock(settings -> place.represent, fieldIQ, blockX, blockY, settings -> place.layer);
			makeEntityUpdatePacket(entityIQ);
			send(playerIQ -> socket, SendPacket, QDIV_PACKET_SIZE, 0);
		}
	}
}

// Artifact Action
void mineBlock(fieldData* fieldIQ, double posX, double posY, entityData* entityIQ, playerData* playerIQ, void* artifactVD, uniqueActionSettings* settings) {
	if(playerIQ -> useTimer >= ((artifactSettings*)artifactVD) -> primaryUseTime) {
		playerIQ -> useTimer = 0;
		int blockX = (int)posX;
		int blockY = (int)posY;
		int layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		if(layerSL != -1) {
			entityIQ -> qEnergy += qEnergyRelevance(entityIQ -> qEnergy, &block[layerSL][fieldIQ -> block[blockX][blockY][layerSL]]);
			fieldIQ -> block[blockX][blockY][layerSL] = NO_WALL;
			fieldIQ -> edit = 1;
			syncBlock(NO_WALL, fieldIQ, blockX, blockY, layerSL);
			makeEntityUpdatePacket(entityIQ);
			send(playerIQ -> socket, SendPacket, QDIV_PACKET_SIZE, 0);
		}
	}
}

// Artifact Action
void oldSliceEntity(fieldData* fieldIQ, double posX, double posY, entityData* entityIQ, playerData* playerIQ, void* artifactVD, uniqueActionSettings* settings) {
	artifactSettings* artifactIQ = (artifactSettings*)artifactVD;
	if(playerIQ -> useTimer >= artifactIQ -> primaryUseTime) {
		playerIQ -> useTimer = 0;
		FOR_EVERY_ENTITY {
			if(entitySlot[entitySL] == 1 &&
			entityInRange(entity + entitySL, entityIQ) && entityType[entity[entitySL].type].maxHealth != 0 &&
			isEntityPresentInRange(entity + entitySL, entityIQ -> fldX, entityIQ -> fldY, entityIQ -> posX, entityIQ -> posY, 1.0) && entity + entitySL != entityIQ) {
				damageEntity(entity + entitySL, settings -> slice.decay);
			}
		}
	}
}

void sliceEntity(fieldData* fieldIQ, double posX, double posY, entityData* entityIQ, playerData* playerIQ, void* artifactVD, uniqueActionSettings* settings) {
	artifactSettings* artifactIQ = (artifactSettings*)artifactVD;
	if(playerIQ -> useTimer >= artifactIQ -> primaryUseTime) {
		playerIQ -> useTimer = 0;
		FOR_EVERY_ENTITY {
			if(entitySlot[entitySL] == 1 && entityInRange(entity + entitySL, entityIQ) && entity[entitySL].healthTimer > 1.0 && entityType[entity[entitySL].type].maxHealth != 0 && entity + entitySL != entityIQ) {
				double scale = QDIV_HITBOX_UNIT;
				while(scale <= 1.2) {
					if(isEntityPresent(entity + entitySL, entityIQ -> fldX, entityIQ -> fldY, entityIQ -> posX + cos(playerIQ -> useTimer * 6.28 / artifactIQ -> primaryUseTime) * scale, entityIQ -> posY + sin(playerIQ -> useTimer * 6.28 / artifactIQ -> primaryUseTime) * scale)) {
						damageEntity(entity + entitySL, settings -> slice.decay);
						break;
					}
					scale += QDIV_HITBOX_UNIT;
				}
			}
		}
	}
}

bool isEntityObstructed(entityData* entityIQ, int direction) {
	entitySettings typeIQ = entityType[entityIQ -> type];
	if(!typeIQ.noClip && direction < NORTH_STOP) {
		double boxIQ = typeIQ.hitBox;
		double minX = 0;
		double minY = 0;
		double maxX = entityIQ -> posX + boxIQ * 0.5;
		double maxY = entityIQ -> posY + boxIQ * 0.5;
		int lclX = 1;
		int lclY = 1;
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

double getMinSpeed(entityData* entityIQ, int direction) {
	return 0;
}

double getMaxSpeed(entityData* entityIQ, int direction) {
	return entityType[entityIQ -> type].speed;
}

bool changeDirection(entityData* entityIQ, int direction) {
	entitySettings typeIQ = entityType[entityIQ -> type];
	double* inMotX = &entityIQ -> motX;
	double* inMotY = &entityIQ -> motY;
	entityData entityCK = *entityIQ;
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
	int rng = rand();
	entityData* entityIQ = (entityData*)entityVD;
	if(rng % 1600 < 8) {
		changeDirection(entityIQ, rng % 8);
		syncEntity(entityIQ);
	}
}

int roleEX;
artifactSettings* artifactEX;

void makeArtifact4(int artifactSL, artifactAction inPrimary, unsigned long long inPrimaryCost, double inPrimaryTime, artifactAction inSecondary, unsigned long long inSecondaryCost, double inSecondaryTime, unsigned long long inEnergy, int Template1, unsigned int value1, int Template2, unsigned int value2, int Template3, unsigned int value3, int Template4, unsigned int value4) {
	artifactEX = &artifact[roleEX][artifactSL];
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

void makeArtifact3(int artifactSL, artifactAction inPrimary, unsigned long long inPrimaryCost, double inPrimaryTime, artifactAction inSecondary, unsigned long long inSecondaryCost, double inSecondaryTime, unsigned long long inEnergy, int Template1, unsigned int value1, int Template2, unsigned int value2, int Template3, unsigned int value3) {
	makeArtifact4(artifactSL, inPrimary, inPrimaryCost, inPrimaryTime, inSecondary, inSecondaryCost, inSecondaryTime, inEnergy, Template1, value1, Template2, value2, Template3, value3, NO_CRITERION, 0);
}

void makeArtifact2(int artifactSL, artifactAction inPrimary, unsigned long long inPrimaryCost, double inPrimaryTime, artifactAction inSecondary, unsigned long long inSecondaryCost, double inSecondaryTime, unsigned long long inEnergy, int Template1, unsigned int value1, int Template2, unsigned int value2) {
	makeArtifact4(artifactSL, inPrimary, inPrimaryCost, inPrimaryTime, inSecondary, inSecondaryCost, inSecondaryTime, inEnergy, Template1, value1, Template2, value2, NO_CRITERION, 0, NO_CRITERION, 0);
}

void makeArtifact1(int artifactSL, artifactAction inPrimary, unsigned long long inPrimaryCost, double inPrimaryTime, artifactAction inSecondary, unsigned long long inSecondaryCost, double inSecondaryTime, unsigned long long inEnergy, int Template1, unsigned int value1) {
	makeArtifact4(artifactSL, inPrimary, inPrimaryCost, inPrimaryTime, inSecondary, inSecondaryCost, inSecondaryTime, inEnergy, Template1, value1, NO_CRITERION, 0, NO_CRITERION, 0, NO_CRITERION, 0);
}

void makeArtifacts() {
	roleEX = WARRIOR;
	makeArtifact1(OLD_SLICER, &sliceEntity, 0, 0.5, NULL, 0, 0, 0, NO_CRITERION, 0);
	artifactEX -> primarySettings.slice.decay = 5;
	roleEX = EXPLORER;
	roleEX = BUILDER;
	makeArtifact1(OLD_SWINGER, &mineBlock, 0, 1, NULL, 0, 0, 0, NO_CRITERION, 0);
	makeArtifact1(BLOCK, &placeBlock, 0, 0.1, NULL, 0, 0, 5, NO_CRITERION, 0);
	artifactEX -> primarySettings.place.represent = FILLED;
	artifactEX -> primarySettings.place.layer = 1;
	makeArtifact1(PLASTIC_ARTIFACT, &placeBlock, 0, 0.1, NULL, 0, 0, 15, NO_CRITERION, 0);
	artifactEX -> primarySettings.place.represent = PLASTIC;
	artifactEX -> primarySettings.place.layer = 0;
	makeArtifact1(LAMP_ARTIFACT, &placeBlock, 0, 0.1, NULL, 0, 0, 80, NO_CRITERION, 0);
	artifactEX -> primarySettings.place.represent = LAMP;
	artifactEX -> primarySettings.place.layer = 1;
	roleEX = GARDENER;
	roleEX = ENGINEER;
	roleEX = WIZARD;
}

void makeEntityTypes() {
	entityType[PLAYER] = makeEntityType(5, true, 2, false, 32.0, 1, NULL);
	entityType[SHALLAND_SNAIL] = makeEntityType(5, true, 4, false, 0.25, 1, &basicSnail);
}

void sliceAndSend(fieldData* fieldIQ, int sockIQ) {
	SendPacket[0] = 0x05;
	fieldSlice sliceIQ;
	sliceIQ.fldX = fieldIQ -> fldX;
	sliceIQ.fldY = fieldIQ -> fldY;
	for(int posX = 0; posX < 128; posX++) {
		sliceIQ.posX = posX;
		memcpy(&sliceIQ.block, &fieldIQ -> block[posX], 256*sizeof(int));
		memcpy(&SendPacket[1], &sliceIQ, sizeof(fieldSlice));
		send(sockIQ, SendPacket, QDIV_PACKET_SIZE, 0);
	}
	sliceIQ.posX = 128;
	memcpy(&SendPacket[1], &sliceIQ, sizeof(fieldSlice));
	send(sockIQ, SendPacket, QDIV_PACKET_SIZE, 0);
}

int getFieldSlot(int inZone, int inFldX, int inFldY) {
	int fieldSL = 0;
	fieldData fieldIQ;
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

void setLocals(entityData* entityIQ) {
	int lclX = 0;
	int lclY = 0;
	while(lclY < 3) {
		entityIQ -> local[lclX][lclY] = &field[getFieldSlot(entityIQ -> zone, entityIQ -> fldX + lclX -1, entityIQ -> fldY + lclY -1)];
		lclX++;
		if(lclX == 3) {
			lclX = 0;
			lclY++;
		}
	}
}

int loadField(int inZone, int inFldX, int inFldY) {
	int fieldSL = getFieldSlot(inZone, inFldX, inFldY);
	if(fieldSL == -1 || fieldSL >= QDIV_MAX_FIELDS) {
		fieldData fieldIQ;
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
					char fileName[64];
					sprintf(fileName, "world/fld.%dz.%dx.%dy.dat", inZone, inFldX, inFldY);
					FILE* fileData = fopen(fileName, "rb");
					if(fileData == NULL) {
						memset(fieldIQ.block, 0x00, sizeof(fieldIQ.block));
						int posX = 0;
						int posY = 0;
						double large_feature, patching, crunch;
						while(posY < 128) {
							large_feature = open_simplex_noise2(mags.large_feature, (double)(inFldX * 128 + posX) * mags.LARGE_FEATURE_FACTOR, (double)(inFldY * 128 + posY) * mags.LARGE_FEATURE_FACTOR);
							patching = open_simplex_noise2(mags.patching, (double)(inFldX * 128 + posX) * mags.PATCHING_FACTOR, (double)(inFldY * 128 + posY) * mags.PATCHING_FACTOR);
							crunch = open_simplex_noise2(mags.crunch, (double)(inFldX * 128 + posX) * mags.CRUNCH_FACTOR, (double)(inFldY * 128 + posY) * mags.CRUNCH_FACTOR);
							if(large_feature > 0) {
								fieldIQ.block[posX][posY][0] = SHALLAND_FLATGRASS;
								if(patching > 0.25) {
									if(crunch > 0.25) {
										if(crunch > 0.75) {
											fieldIQ.block[posX][posY][1] = SHALLAND_BUSH;
										}else{
											fieldIQ.block[posX][posY][1] = SHALLAND_GRASS;
										}
									}
								}
							}else{
								fieldIQ.block[posX][posY][0] = WATER;
							}
							posX++;
							if(posX == 128) {
								posX = 0;
								posY++;
							}
						}
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
	}
	return fieldSL;
}

int spawnEntity(int inType, int inZone, int inFldX, int inFldY, double inPosX, double inPosY, int socketIfAny) {
	entityData entityIQ;
	entityIQ.type = inType;
	entityIQ.active = 1;
	entityIQ.zone = inZone;
	entityIQ.fldX = inFldX;
	entityIQ.fldY = inFldY;
	entityIQ.posX = inPosX;
	entityIQ.posY = inPosY;
	entityIQ.motX = 0;
	entityIQ.motY = 0;
	entityIQ.health = entityType[inType].maxHealth;
	entityIQ.qEnergy = 500;
	strcpy(entityIQ.name, "Esample");
	int entitySL;
	for(entitySL = 0; entitySL < 10000; entitySL++) {
		if(entitySlot[entitySL] == 0) {
			entity[entitySL] = entityIQ;
			entity[entitySL].slot = entitySL;
			switch(inType) {
				case PLAYER:
					playerData* playerIQ = &entityIQ.unique.Player;
					playerIQ -> socket = socketIfAny;
					playerIQ -> role = BUILDER;
					playerIQ -> roleTimer = 0;
					playerIQ -> artifact = OLD_SWINGER;
					playerIQ -> criterion = criterion[entitySL];
					playerIQ -> useTimer = 0;
					sliceAndSend(&field[loadField(inZone, inFldX, inFldY)], socketIfAny);
					sliceAndSend(&field[loadField(inZone, inFldX+1, inFldY)], socketIfAny);
					sliceAndSend(&field[loadField(inZone, inFldX-1, inFldY)], socketIfAny);
					usleep(1000);
					sliceAndSend(&field[loadField(inZone, inFldX, inFldY+1)], socketIfAny);
					sliceAndSend(&field[loadField(inZone, inFldX+1, inFldY+1)], socketIfAny);
					sliceAndSend(&field[loadField(inZone, inFldX-1, inFldY+1)], socketIfAny);
					usleep(1000);
					sliceAndSend(&field[loadField(inZone, inFldX, inFldY-1)], socketIfAny);
					sliceAndSend(&field[loadField(inZone, inFldX+1, inFldY-1)], socketIfAny);
					sliceAndSend(&field[loadField(inZone, inFldX-1, inFldY-1)], socketIfAny);
					makeLightTaskPacket();
					send(socketIfAny, SendPacket, QDIV_PACKET_SIZE, 0);
					break;
			}
			setLocals(&entityIQ);
			entity[entitySL] = entityIQ;
			entity[entitySL].slot = entitySL;
			switch(inType) {
				case PLAYER:
					usleep(1000);
					syncEntityField(entity[entitySL].local[0][0], socketIfAny);
					syncEntityField(entity[entitySL].local[1][0], socketIfAny);
					syncEntityField(entity[entitySL].local[2][0], socketIfAny);
					usleep(1000);
					syncEntityField(entity[entitySL].local[0][1], socketIfAny);
					syncEntityField(entity[entitySL].local[1][1], socketIfAny);
					syncEntityField(entity[entitySL].local[2][1], socketIfAny);
					usleep(1000);
					syncEntityField(entity[entitySL].local[0][2], socketIfAny);
					syncEntityField(entity[entitySL].local[1][2], socketIfAny);
					syncEntityField(entity[entitySL].local[2][2], socketIfAny);
					usleep(1000);
					break;
			}
			entitySlot[entitySL] = 1;
			break;
		}
	}
	return entitySL;
}

// Thread
void* thread_gate() {
	#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 0), &wsa);
	#else
	signal(SIGPIPE, SIG_IGN);
	#endif
    struct sockaddr_in6 address;
    int addrlen = sizeof(address);
    int socketOption = 1;
    int sockSF = socket(AF_INET6, SOCK_STREAM, 0);
    int sockIQ, sockMX, sockSL, sockAM, sockCK;
    fd_set sockRD;
    struct timeval qTimeOut;
    setsockopt(sockSF, SOL_SOCKET, SO_REUSEADDR, &socketOption, sizeof(socketOption));
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
            sockIQ = entity[sockSL].unique.Player.socket;
            FD_SET(sockIQ, &sockRD);
            if(sockIQ > sockMX) sockMX = sockIQ;
        }
        qTimeOut.tv_sec = 1;
        qTimeOut.tv_usec = 0;
        if(select(sockMX+1, &sockRD, NULL, NULL, &qTimeOut) > 0) {
        	for(sockSL = 0; sockSL < QDIV_MAX_PLAYERS; sockSL++) {
        		entityData* entityIQ;
            	if(FD_ISSET(sockSF, &sockRD) && entity[sockSL].type == PLAYER && entity[sockSL].unique.Player.socket == 0) {
            	    int entitySL = spawnEntity(PLAYER, 0, 0, 0, 0, 0, accept(sockSF, (struct sockaddr*)&address, (socklen_t*)&addrlen));
            	    entityIQ = &entity[entitySL];
            	    makeEntityUpdatePacket(entityIQ);
            	    SendPacket[0] = 0x06;
					send(entityIQ -> unique.Player.socket, SendPacket, QDIV_PACKET_SIZE, 0);
					makeLightTaskPacket();
					send(entityIQ -> unique.Player.socket, SendPacket, QDIV_PACKET_SIZE, 0);
					puts("[Gate] Player connected");
            	    break;
            	}
        	}
        	for(sockSL = 0; sockSL < QDIV_MAX_PLAYERS; sockSL++) {
            	if(entity[sockSL].unique.Player.socket > 0 && FD_ISSET(entity[sockSL].unique.Player.socket, &sockRD)) {
            	    if(read(entity[sockSL].unique.Player.socket, ReceivePacket, QDIV_PACKET_SIZE) == 0) {
            	        close(entity[sockSL].unique.Player.socket);
            	        entity[sockSL].unique.Player.socket = 0;
            	        removeEntity(sockSL);
            	        syncEntityRemoval(sockSL);
            	        puts("[Gate] Player Disconnected");
            	    }else{
            	        entityData* entityIQ = entity + sockSL;
            	        playerData* playerIQ = &entityIQ -> unique.Player;
            	        switch(ReceivePacket[0]) {
            	        	case 0x02:
            	        		int direction;
            	        		memcpy(&direction, ReceivePacket+1, sizeof(int));
            	        		if(changeDirection(entityIQ, direction)) syncEntity(entityIQ);
            	        		break;
            	        	case 0x08:
            	        		int roleIQ, artifactIQ;
            	        		memcpy(&roleIQ, ReceivePacket+1, sizeof(int));
            	        		memcpy(&artifactIQ, ReceivePacket+5, sizeof(int));
            	        		if(isArtifactUnlocked(roleIQ, artifactIQ, entityIQ)) {
            	        			playerIQ -> role = roleIQ;
            	        			playerIQ -> artifact = artifactIQ;
            	        			makeEntityUpdatePacket(entityIQ);
            	        			send(playerIQ -> socket, SendPacket, QDIV_PACKET_SIZE, 0);
            	        		}
            	        	case 0x0A:
            	        		usageData usageIQ;
            	        		memcpy(&usageIQ, ReceivePacket+1, sizeof(usageData));
        	        			playerIQ -> currentUsage = usageIQ.usage;
	        	        		playerIQ -> useRelX = usageIQ.useRelX;
	        	        		playerIQ -> useRelY = usageIQ.useRelY;
	        	        		syncEntity(entityIQ);
		        	        	break;
            	        }
            	    }
            	}
            }
        }
    }
    for(sockSL = 0; sockSL < QDIV_MAX_PLAYERS; sockSL++) {
        if(entity[sockSL].type == PLAYER && entity[sockSL].unique.Player.socket > 0) {
        	SendPacket[0] = 0x01;
			send(entity[sockSL].unique.Player.socket, SendPacket, QDIV_PACKET_SIZE, 0);
            close(entity[sockSL].unique.Player.socket);
            entity[sockSL].unique.Player.socket = 0;
            removeEntity(sockSL);
            puts("[Gate] Player Disconnected");
        }
    }
}

// Thread
void* thread_console() {
	char prompt[64];
	while(1) {
		scanf("%64s", prompt);
		if(strcmp(prompt, "shutdown") == 0) {
			*pShutdown = 1;
			break;
		}
		if(strcmp(prompt, "report") == 0) {
			int repClk;
			printf("Tick Time: %d µs / 20000 µs\n\n", *pTickTime);
			puts("–– Entity Table ––");
			for(repClk = 0; repClk < 400; repClk++) {
				printf("%d ", entitySlot[repClk]);
				if((repClk+1) % 20 == 0) {
					printf("\n");
				}
			}
			printf("\n");
			puts("–– Client Table ––");
			for(repClk = 0; repClk < 64; repClk++) {
				printf("%d ", entity[repClk].type == PLAYER ? entity[repClk].unique.Player.socket : 0);
				if((repClk+1) % 8 == 0) {
					printf("\n");
				}
			}
			puts("–– Field Table ––");
			int fieldTable[9][9] = {0};
			for(repClk = 0; repClk < QDIV_MAX_FIELDS; repClk++) {
				if(field[repClk].active && field[repClk].fldX > -5 && field[repClk].fldX < 5 && field[repClk].fldY > -5 && field[repClk].fldY < 5) {
					fieldTable[field[repClk].fldX+4][field[repClk].fldY+4] = 1;
				}
			}
			int hlpClk;
			for(hlpClk = 9; hlpClk >= 0; hlpClk--) {
				for(repClk = 0; repClk < 9; repClk++) {
					printf("%d ", fieldTable[repClk][hlpClk]);
				}
				printf("\n");
			}
			printf("X: %f, Y: %f, FX: %d, FY: %d\n", entity[0].posX, entity[0].posY, entity[0].fldX, entity[0].fldY);
			memset(prompt, '\0', 32 * sizeof(char));
		}
		if(strcmp(prompt, "query") == 0) {
			if(*pTickQuery) {
				puts("[Query] Disabled");
				*pTickQuery = false;
			}else{
				puts("[Query] Enabled");
				*pTickQuery = true;
			}
			memset(prompt, '\0', 32 * sizeof(char));
		}
		if(strcmp(prompt, "snail") == 0) {
			spawnEntity(SHALLAND_SNAIL, entity[0].zone, entity[0].fldX, entity[0].fldY, entity[0].posX, entity[0].posY, 0);
		}
	}
}

int main() {
	printf("\n–––– qDivServer-%d.%c ––––\n\n", QDIV_VERSION, QDIV_BRANCH);
	memset(entity, 0x00, sizeof(entity));
	srand(time(NULL));
	libMain();
	makeArtifacts();
	makeEntityTypes();
	puts("[Out] Starting Threads");
	pthread_t console_id, gate_id;
	pthread_create(&gate_id, NULL, thread_gate, NULL);
	pthread_create(&console_id, NULL, thread_console, NULL);
	puts("[Out] Initializing OpenSimplex");
	open_simplex_noise(43017, &mags.large_feature);
	mags.LARGE_FEATURE_FACTOR = 0.05;
	open_simplex_noise(51129, &mags.patching);
	mags.PATCHING_FACTOR = 0.1;
	open_simplex_noise(86283, &mags.crunch);
	mags.CRUNCH_FACTOR = 1;
	puts("[Out] Setting up Tick Stamper");
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
		if(TickQuery) printf("[Out] Tick: %d\n", qTickStart.tv_usec);
		for(int entitySL = 0; entitySL < 10000; entitySL++) {
			if(entitySlot[entitySL] == 1) {
				entityData* entityIQ = entity + entitySL;
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
				int sockIQ = entityIQ -> unique.Player.socket;
				if(entityIQ -> posX >= 128) {
					entityIQ -> posX = -128 + entityIQ -> posX;
					entityIQ -> fldX++;
					syncEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						unloadField(entityIQ -> zone, entityIQ -> fldX-2, entityIQ -> fldY);
						unloadField(entityIQ -> zone, entityIQ -> fldX-2, entityIQ -> fldY+1);
						unloadField(entityIQ -> zone, entityIQ -> fldX-2, entityIQ -> fldY-1);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY)], sockIQ);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY+1)], sockIQ);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY-1)], sockIQ);
						makeLightTaskPacket();
						send(sockIQ, SendPacket, QDIV_PACKET_SIZE, 0);
					}
					setLocals(entityIQ);
					if(entityIQ -> type == PLAYER) {
						syncEntityField(entityIQ -> local[2][0], sockIQ);
						syncEntityField(entityIQ -> local[2][1], sockIQ);
						syncEntityField(entityIQ -> local[2][2], sockIQ);
					}
				}
				if(entityIQ -> posX < 0) {
					entityIQ -> posX = 128 + entityIQ -> posX;
					entityIQ -> fldX--;
					syncEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						unloadField(entityIQ -> zone, entityIQ -> fldX+2, entityIQ -> fldY);
						unloadField(entityIQ -> zone, entityIQ -> fldX+2, entityIQ -> fldY+1);
						unloadField(entityIQ -> zone, entityIQ -> fldX+2, entityIQ -> fldY-1);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY)], sockIQ);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY+1)], sockIQ);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY-1)], sockIQ);
						makeLightTaskPacket();
						send(sockIQ, SendPacket, QDIV_PACKET_SIZE, 0);
					}
					setLocals(entityIQ);
					if(entityIQ -> type == PLAYER) {
						syncEntityField(entityIQ -> local[0][0], sockIQ);
						syncEntityField(entityIQ -> local[0][1], sockIQ);
						syncEntityField(entityIQ -> local[0][2], sockIQ);
					}
				}
				if(entityIQ -> posY >= 128) {
					entityIQ -> posY = -128 + entityIQ -> posY;
					entityIQ -> fldY++;
					syncEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						unloadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY-2);
						unloadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY-2);
						unloadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY-2);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY+1)], sockIQ);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY+1)], sockIQ);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY+1)], sockIQ);
						makeLightTaskPacket();
						send(sockIQ, SendPacket, QDIV_PACKET_SIZE, 0);
					}
					setLocals(entityIQ);
					if(entityIQ -> type == PLAYER) {
						syncEntityField(entityIQ -> local[0][2], sockIQ);
						syncEntityField(entityIQ -> local[1][2], sockIQ);
						syncEntityField(entityIQ -> local[2][2], sockIQ);
					}
				}
				if(entityIQ -> posY < 0) {
					entityIQ -> posY = 128 + entityIQ -> posY;
					entityIQ -> fldY--;
					syncEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						unloadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY+2);
						unloadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY+2);
						unloadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY+2);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY-1)], sockIQ);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY-1)], sockIQ);
						sliceAndSend(&field[loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY-1)], sockIQ);
						makeLightTaskPacket();
						send(sockIQ, SendPacket, QDIV_PACKET_SIZE, 0);
					}
					setLocals(entityIQ);
					if(entityIQ -> type == PLAYER) {
						syncEntityField(entityIQ -> local[0][0], sockIQ);
						syncEntityField(entityIQ -> local[1][0], sockIQ);
						syncEntityField(entityIQ -> local[2][0], sockIQ);
					}
				}
				if(entityIQ -> type == PLAYER) {
					playerData* playerIQ = &entityIQ -> unique.Player;
					if(playerIQ -> currentUsage == NO_USAGE) {
						playerIQ -> useTimer = 0;
					}else{
						playerIQ -> useTimer += qFactor;
						int lclX = 1;
						int lclY = 1;
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
							lclX = 0;
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
					if(playerIQ -> roleTimer > 0) playerIQ -> roleTimer -= qFactor;
				}
				if(entityType[entityIQ -> type].action != NULL) (*entityType[entityIQ -> type].action)((void*)entityIQ);
				if(entityIQ -> healthTimer < 60.0) {
					entityIQ -> healthTimer += qFactor;
				}
				if(entitySL >= QDIV_MAX_PLAYERS && entityIQ -> type == PLAYER) nonsense(1130);
			}
		}
	}
	pthread_join(gate_id, NULL);
	pthread_join(console_id, NULL);
}
