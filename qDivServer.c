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
#include<errno.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<pthread.h>
#include<time.h>
#include "qDivBridge.h"

int32_t qShutdown = 0;
int32_t* pShutdown = &qShutdown;

struct {
	int32_t PORT;
	int32_t MAX_PLAYERS;
	int32_t MAX_FIELDS;
	int32_t PLAYER_SPEED;
	int32_t MAX_ENTITIES;
	int32_t WORLD_SEED;
} settings;

int32_t qTickTime;
int32_t* pTickTime = &qTickTime;
bool TickQuery = false;
bool* pTickQuery = &TickQuery;

field_t* field;
block_ust* uniqueBlock = NULL;

client_t* client;

// Synchronizer
void syncEntityRemoval(int32_t slot) {
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].type == PLAYER && entityInRange(entity + slot, entity + entitySL)) send_encrypted(0x03, (void*)&slot, sizeof(int32_t), entity[entitySL].unique.Player.client);
	}
}

// Synchronizer
void syncEntity(entity_t* entityIQ) {
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].type == PLAYER && entityInRange(entityIQ, entity + entitySL)) send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), entity[entitySL].unique.Player.client);
	}
}

// Synchronizer
void syncEntityField(field_t* fieldIQ, client_t* clientIQ) {
	entity_t* entityIQ;
	FOR_EVERY_ENTITY {
		if(entityTable[entitySL]) {
			entityIQ = entity + entitySL;
			if(entityIQ -> zone == fieldIQ -> zone && entityIQ -> fldX == fieldIQ -> fldX && entityIQ -> fldY == fieldIQ -> fldY) {
				send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), clientIQ);
			}
		}
	}
}

// Synchronizer
void syncBlock(uint16_t blockIQ, field_t* fieldIQ, int32_t posX, int32_t posY, int32_t layer) {
	block_l locationIQ = {blockIQ, fieldIQ -> zone, fieldIQ -> fldX, fieldIQ -> fldY, posX, posY, layer};
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entity[entitySL].type == PLAYER && (entity[entitySL].local[0][0] == fieldIQ || entity[entitySL].local[0][1] == fieldIQ || entity[entitySL].local[0][2] == fieldIQ || entity[entitySL].local[1][0] == fieldIQ || entity[entitySL].local[1][1] == fieldIQ || entity[entitySL].local[1][2] == fieldIQ || entity[entitySL].local[2][0] == fieldIQ || entity[entitySL].local[2][1] == fieldIQ || entity[entitySL].local[2][2] == fieldIQ)) {
			send_encrypted(0x07, (void*)&locationIQ, sizeof(block_l), entity[entitySL].unique.Player.client);
		}
	}
}

// Synchronizer
void syncTime() {
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].type == PLAYER) send_encrypted(0x0B, (void*)&currentHour, sizeof(int8_t), entity[entitySL].unique.Player.client);
	}
}

void unloadField(int32_t inZone, int32_t inFldX, int32_t inFldY) {
	for(int32_t fieldSL = 0; fieldSL < settings.MAX_FIELDS; fieldSL++) {
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
				/*block_ust* uniqueTMP;
				block_ust* uniqueIQ = fieldIQ -> uniqueBlock;
				while(uniqueIQ != NULL) {
					uniqueTMP = uniqueIQ;
					uniqueIQ = uniqueIQ -> next;
					free(uniqueTMP);
				}
				fieldIQ -> uniqueBlock = NULL;*/
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
		/*int8_t fileName[32];
		sprintf(fileName, "criterion/CR.%s.dat", entity[slot].name);
		FILE* criterionFile = fopen(fileName, "wb");
		if(criterionFile != NULL) {
			fwrite(entity[slot].unique.Player.criterion, sizeof(uint32_t), MAX_CRITERION, criterionFile);
			fclose(criterionFile);
		}
		free(entity[slot].unique.Player.criterion);*/
		int8_t filePath[32];
		int8_t* bufferIQ = NULL;
		size_t bufferSZ = 0;
		sprintf(filePath, "criterion/CR.%s.qsm", entity[slot].name);
		int32_t criterionSL = 0;
		uint32_t* criterionIQ = entity[slot].unique.Player.criterion;
		while(criterionSL < MAX_CRITERION) {
			if(criterionIQ[criterionSL] != 0) {
				bufferSZ = QSMdecode(QSM_UNSIGNED_INT, (void*)(criterionIQ + criterionSL), (size_t)log10(*(criterionIQ + criterionSL)) + 1, &bufferIQ, bufferSZ, criterionTemplate[criterionSL].desc, strlen(criterionTemplate[criterionSL].desc));
			}
			criterionSL++;
		}
		QSMwrite(filePath, &bufferIQ, bufferSZ);
		free(criterionIQ);
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
	entityIQ -> healthTM = 0;
	entityIQ -> health--;
	if(entityIQ -> health == 0 || decay >= entityIQ -> qEnergy) {
		if(entityIQ -> type == PLAYER) {
			entityIQ -> health = 5;
			entityIQ -> qEnergy = 1;
			entityIQ -> healthTM = 60.0;
			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), entityIQ -> unique.Player.client);
		}else{
			removeEntity(entityIQ -> slot);
			syncEntityRemoval(entityIQ -> slot);
		}
		return true;
	}else{
		if(entityIQ -> type == PLAYER) {
			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), entityIQ -> unique.Player.client);
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

void setBlock(uint16_t blockIQ, field_t* fieldIQ, int32_t posX, int32_t posY, int32_t layer) {
	uint16_t blockPR = fieldIQ -> block[posX][posY][layer];
	fieldIQ -> edit = 1;
	fieldIQ -> block[posX][posY][layer] = blockIQ;
	block_l locationAS = {blockPR, fieldIQ -> zone, fieldIQ -> fldX, fieldIQ -> fldY, posX, posY, layer};
	if(block[layer][blockPR].type > NO_TYPE_L) {
		block_ust* uniqueIQ;
		block_ust* uniqueTMP = uniqueBlock;
		while(uniqueTMP != NULL && uniqueTMP -> next != NULL) {
			uniqueIQ = uniqueTMP;
			uniqueTMP = uniqueTMP -> next;
			if(memcmp(&uniqueTMP -> location, &locationAS, sizeof(block_l)) == 0) {
				uniqueIQ -> next = uniqueTMP -> next;
				free(uniqueTMP);
				uniqueTMP = uniqueIQ -> next;
			}
		}
	}
	if(block[layer][blockIQ].type > NO_TYPE_L) {
		block_ust** uniqueIQ = &uniqueBlock;
		while(*uniqueIQ != NULL) uniqueIQ = &(*uniqueIQ) -> next;
		*uniqueIQ = malloc(sizeof(block_ust));
		memset(*uniqueIQ, 0x00, sizeof(block_ust));
		(*uniqueIQ) -> next = NULL;
		block_l* locationIQ = &(*uniqueIQ) -> location;
		locationIQ -> block = blockIQ;
		locationIQ -> zone = fieldIQ -> zone;
		locationIQ -> fldX = fieldIQ -> fldX;
		locationIQ -> fldY = fieldIQ -> fldY;
		locationIQ -> posX = posX;
		locationIQ -> posY = posY;
		locationIQ -> layer = layer;
	}
	syncBlock(blockIQ, fieldIQ, posX, posY, layer);
}

// Artifact Action
def_ArtifactAction(placeBlock) {
	if(playerIQ -> useTM >= 0.1 && (playerIQ -> useRelX > 1.5 || playerIQ -> useRelX < -1.5 || playerIQ -> useRelY > 1.5 || playerIQ -> useRelY < -1.5)) {
		artifact_st* artifactIQ = (artifact_st*)artifactVD;
		playerIQ -> useTM = 0;
		int32_t blockX = (int)posX;
		int32_t blockY = (int)posY;
		int32_t layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		if(layerSL < settings -> place.layer && entityIQ -> qEnergy -1 >= 1) {
			entityIQ -> qEnergy -= 1;
			setBlock(settings -> place.represent, fieldIQ, blockX, blockY, settings -> place.layer);
			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), playerIQ -> client);
		}
	}
}

// Artifact Action
def_ArtifactAction(mineBlock) {
	if(playerIQ -> useTM >= ((artifact_st*)artifactVD) -> primaryUseTime) {
		playerIQ -> useTM = 0;
		int32_t blockX = (int32_t)posX;
		int32_t blockY = (int32_t)posY;
		int32_t layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		if(layerSL != -1) {
			block_st* blockIQ = &block[layerSL][fieldIQ -> block[blockX][blockY][layerSL]];
			entityIQ -> qEnergy += qEnergyRelevance(entityIQ -> qEnergy, blockIQ);
			int32_t criterionTP = blockIQ -> mine_criterion;
			if(criterionTP != NO_CRITERION) {
				criterion_t criterionIQ = {criterionTP, ++playerIQ -> criterion[criterionTP]};
				send_encrypted(0x09, (void*)&criterionIQ, sizeof(criterion_t), playerIQ -> client);
			}
			setBlock(NO_WALL * (blockIQ -> type != DECOMPOSING) + blockIQ -> unique.decomposite * (blockIQ -> type == DECOMPOSING), fieldIQ, blockX, blockY, layerSL);
			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), playerIQ -> client);
		}
	}
}

// Artifact Action
def_ArtifactAction(sliceEntity) {
	artifact_st* artifactIQ = (artifact_st*)artifactVD;
	if(playerIQ -> useTM == 0) entityIQ -> qEnergy -= artifactIQ -> primaryCost;
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entityInRange(entity + entitySL, entityIQ) && entity[entitySL].healthTM > 1.0 && entityType[entity[entitySL].type].maxHealth != 0 && entity + entitySL != entityIQ) {
			double scale = QDIV_HITBOX_UNIT;
			while(scale <= 0.4) {
				if(isEntityPresent(entity + entitySL, entityIQ -> fldX, entityIQ -> fldY, (entityIQ -> posX + cos(playerIQ -> useTM * 6.28 / artifactIQ -> primaryUseTime) * scale) * (playerIQ -> useRelX / abs(playerIQ -> useRelX)), entityIQ -> posY + sin(playerIQ -> useTM * 6.28 / artifactIQ -> primaryUseTime) * scale)) {
					int32_t criterionTP = entityType[entity[entitySL].type].kill_criterion;
					if(damageEntity(entity + entitySL, settings -> slice.decay) && criterionTP != NO_CRITERION) {
						criterion_t criterionIQ = {criterionTP, ++playerIQ -> criterion[criterionTP]};
						send_encrypted(0x09, (void*)&criterionIQ, sizeof(criterion_t), playerIQ -> client);
					}
					break;
				}
				scale += QDIV_HITBOX_UNIT;
			}
		}
	}
	if(playerIQ -> useTM >= artifactIQ -> primaryUseTime) playerIQ -> useTM = 0;
}

// Artifact Action
def_ArtifactAction(fertilizeBlock) {
	if(playerIQ -> useTM >= ((artifact_st*)artifactVD) -> secondaryUseTime) {
		playerIQ -> useTM = 0;
		int32_t blockX = (int32_t)posX;
		int32_t blockY = (int32_t)posY;
		int32_t layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		if(layerSL == 0 && block[0][fieldIQ -> block[blockX][blockY][0]].type == FERTILE) {
			criterion_t criterionIQ = {FERTILIZE_LAND, ++playerIQ -> criterion[FERTILIZE_LAND]};
			send_encrypted(0x09, (void*)&criterionIQ, sizeof(criterion_t), playerIQ -> client);
			setBlock(SOIL, fieldIQ, blockX, blockY, layerSL);
		}
	}
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
					if(entityIQ -> local[lclX][lclY] == NULL || block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].friction != 1.0) {
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
					if(entityIQ -> local[lclX][lclY] == NULL || block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].friction != 1.0) {
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
					if(entityIQ -> local[lclX][lclY] == NULL || block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].friction != 1.0) {
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
					if(entityIQ -> local[lclX][lclY] == NULL || block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].friction != 1.0) {
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
	return entityType[entityIQ -> type].speed * block[0][entityIQ -> local[1][1] -> block[0][(int32_t)entityIQ -> posX][(int32_t)entityIQ -> posY]].friction;
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

int32_t selectEntityType(field_t* fieldIQ, double posX, double posY) {
	printf("%d %d\n", (int32_t)posX, (int32_t)posY);
	uint16_t biomeIQ = fieldIQ -> biome[(int32_t)posX][(int32_t)posY];
	switch(biomeIQ) {
		case SHALLAND:
			return SHALLAND_SNAIL;
	}
	return NULL_ENTITY;
}

// Entity Action
void playerAction(void* entityVD) {
	entity_t* entityIQ = (entity_t*)entityVD;
	player_t* playerIQ = &entityIQ -> unique.Player;
	if(playerIQ -> currentUsage == NO_USAGE) {
		playerIQ -> useTM = 0;
	}else{
		playerIQ -> useTM += qFactor;
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
		artifact_st* artifactIQ = &artifact[playerIQ -> role][playerIQ -> artifact];
		if(playerIQ -> currentUsage == PRIMARY_USAGE && artifactIQ -> primary != NULL) {
			(*artifactIQ -> primary)(entityIQ -> local[lclX][lclY], posX, posY, entityIQ, playerIQ, (void*)artifactIQ, &artifactIQ -> primarySettings);
		}else if(playerIQ -> currentUsage == SECONDARY_USAGE && artifactIQ -> secondary != NULL) {
			(*artifactIQ -> secondary)(entityIQ -> local[lclX][lclY], posX, posY, entityIQ, playerIQ, (void*)artifactIQ, &artifactIQ -> secondarySettings);
		}else playerIQ -> useTM = 0;
	}
	playerIQ -> spawnTM += qFactor;
	if(playerIQ -> spawnTM > 5.0) {
		playerIQ -> spawnTM = 0.0;
		uint32_t value;
		int32_t lclX, lclY;
		double posX, posY;
		QDIV_RANDOM(&value, sizeof(uint32_t));
		switch((value % 768) / 192) {
			case NORTH:
				posX = (double)(((int32_t)value) % 192) - 96;
				posY = 96;
				break;
			case EAST:
				posX = 96;
				posY = (double)(((int32_t)value) % 192) - 96;
				break;
			case SOUTH:
				posX = (double)(((int32_t)value) % 192) - 96;
				posY = -96;
				break;
			case WEST:
				posX = -96;
				posY = (double)(((int32_t)value) % 192) - 96;
				break;
		}
		posX += entityIQ -> posX;
		posY += entityIQ -> posY;
		derelativize(&lclX, &lclY, &posX, &posY);
		int32_t entitySL;
		if(entityIQ -> local[lclX][lclY] == NULL) {
			entitySL = NULL_ENTITY;
		}else{
			entitySL = selectEntityType(entityIQ -> local[lclX][lclY], posX, posY);
		}
		if(entitySL != NULL_ENTITY) syncEntity(entity + spawnEntity("Entity", uuidNull, entitySL, entityIQ -> zone, entityIQ -> fldX + lclX - 1, entityIQ -> fldY + lclY - 1, posX, posY)); 
	}
	if(playerIQ -> roleTM > 0.0) playerIQ -> roleTM -= qFactor;
}

// Entity Action
void snailAction(void* entityVD) {
	int32_t rng = rand();
	entity_t* entityIQ = (entity_t*)entityVD;
	if(rng % 1600 < 8) {
		changeDirection(entityIQ, rng % 8);
		syncEntity(entityIQ);
	}
}

void makeEntityTypes() {
	entityType[NULL_ENTITY] = makeEntityType(0, false, 0, false, 0.0, 1, NULL, NO_CRITERION);
	entityType[PLAYER] = makeEntityType(5, true, 2, false, 1.5 * ((double)settings.PLAYER_SPEED), 1, &playerAction, NO_CRITERION);
	entityType[SHALLAND_SNAIL] = makeEntityType(5, false, 4, false, 0.25, 1, &snailAction, NO_CRITERION);
}

int32_t getFieldSlot(int32_t inZone, int32_t inFldX, int32_t inFldY) {
	int32_t fieldSL = 0;
	field_t fieldIQ;
	do{
		fieldIQ = field[fieldSL];
		fieldSL++;
	}while((fieldIQ.zone != inZone || fieldIQ.fldX != inFldX || fieldIQ.fldY != inFldY) && fieldSL < settings.MAX_FIELDS);
	if(fieldSL-- == settings.MAX_FIELDS) {
		return -1;
	}else if(fieldIQ.active == 1) {
		return fieldSL;
	}else{
		return fieldSL + settings.MAX_FIELDS;
	}
}

void setLocals(entity_t* entityIQ) {
	int32_t lclX = 0;
	int32_t lclY = 0;
	int32_t slotIQ;
	while(lclY < 3) {
		slotIQ = getFieldSlot(entityIQ -> zone, entityIQ -> fldX + lclX -1, entityIQ -> fldY + lclY -1);
		entityIQ -> local[lclX][lclY] = slotIQ == -1 || slotIQ > settings.MAX_FIELDS ? NULL : field + slotIQ;
		lclX++;
		if(lclX == 3) {
			lclX = 0;
			lclY++;
		}
	}
}

int32_t loadField(int32_t inZone, int32_t inFldX, int32_t inFldY) {
	int32_t fieldSL = getFieldSlot(inZone, inFldX, inFldY);
	if(fieldSL == -1 || fieldSL >= settings.MAX_FIELDS) {
		field_t fieldIQ;
		fieldIQ.active = 1;
		fieldIQ.edit = 0;
		fieldIQ.zone = inZone;
		fieldIQ.fldX = inFldX;
		fieldIQ.fldY = inFldY;
		if(fieldSL >= settings.MAX_FIELDS && inZone != 0) {
			fieldSL -= settings.MAX_FIELDS;
			field[fieldSL].active = 1;
		}else{
			for(fieldSL = 0; fieldSL < settings.MAX_FIELDS; fieldSL++) {
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
					block_ust** uniqueIQ = &uniqueBlock;
					while(*uniqueIQ != NULL) {
						block_l* locationIQ = &(*uniqueIQ) -> location;
						if(locationIQ -> zone == inZone && locationIQ -> fldX == inZone && locationIQ -> fldY == inZone) goto FieldUpToDate;
						uniqueIQ = &(*uniqueIQ) -> next;
					}
					int32_t posX = 0;
					int32_t posY = 0;
					int32_t layerIQ = 0;
					while(layerIQ < 2) {
						if(block[layerIQ][fieldIQ.block[posX][posY][layerIQ]].type > NO_TYPE_L) {
							*uniqueIQ = malloc(sizeof(block_ust));
							memset(*uniqueIQ, 0x00, sizeof(block_ust));
							(*uniqueIQ) -> next = NULL;
							block_l* locationIQ = &(*uniqueIQ) -> location;
							locationIQ -> block = fieldIQ.block[posX][posY][layerIQ];
							locationIQ -> zone = fieldIQ.zone;
							locationIQ -> fldX = fieldIQ.fldX;
							locationIQ -> fldY = fieldIQ.fldY;
							locationIQ -> posX = posX;
							locationIQ -> posY = posY;
							locationIQ -> layer = layerIQ;
							uniqueIQ = &(*uniqueIQ) -> next;
						}
						posX++;
						if(posX == 128) {
							posX = 0;
							posY++;
							if(posY == 128) {
								posY = 0;
								layerIQ++;
							}
						}
					}
					FieldUpToDate:
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
			entityIQ.healthTM = 60.0;
			entityIQ.qEnergy = 40;
			entityIQ.type = inType;
			switch(inType) {
				case PLAYER:
					player_t* playerIQ = &entityIQ.unique.Player;
					playerIQ -> role = BUILDER;
					playerIQ -> roleTM = 0;
					playerIQ -> artifact = OLD_SWINGER;
					playerIQ -> useTM = 0;
					break;
			}
			oldEntity:
			if(entityIQ.type == PLAYER) {
				loadField(entityIQ.zone, entityIQ.fldX, entityIQ.fldY);
				loadField(entityIQ.zone, entityIQ.fldX+1, entityIQ.fldY);
				loadField(entityIQ.zone, entityIQ.fldX-1, entityIQ.fldY);
				loadField(entityIQ.zone, entityIQ.fldX, entityIQ.fldY+1);
				loadField(entityIQ.zone, entityIQ.fldX+1, entityIQ.fldY+1);
				loadField(entityIQ.zone, entityIQ.fldX-1, entityIQ.fldY+1);
				loadField(entityIQ.zone, entityIQ.fldX, entityIQ.fldY-1);
				loadField(entityIQ.zone, entityIQ.fldX+1, entityIQ.fldY-1);
				loadField(entityIQ.zone, entityIQ.fldX-1, entityIQ.fldY-1);
			}
			setLocals(&entityIQ);
			entityIQ.slot = entitySL;
			entityIQ.active = 1;
			entity[entitySL] = entityIQ;
			entityTable[entitySL] = true;
			return entitySL;
		}
	}
	return -1;
}

// Thread
void* thread_gate() {
	#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	#else
	signal(SIGPIPE, SIG_IGN);
	#endif
    struct sockaddr_in6 addrSF;
    struct timeval qTimeOut = {0, 0};
    fd_set sockRD;
    sockSF = malloc(sizeof(int32_t));
    *sockSF = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    int32_t clientSL;
    int32_t sockTRUE = 1;
    int32_t sockFALSE = 0;
    #ifdef _WIN32
    setsockopt(*sockSF, SOL_SOCKET, SO_REUSEADDR, (const char*)&sockTRUE, sizeof(sockTRUE));
    setsockopt(*sockSF, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&sockFALSE, sizeof(sockFALSE));
    #else
    setsockopt(*sockSF, SOL_SOCKET, SO_REUSEADDR, &sockTRUE, sizeof(sockTRUE));
    setsockopt(*sockSF, IPPROTO_IPV6, IPV6_V6ONLY, &sockFALSE, sizeof(sockFALSE));
    setsockopt(*sockSF, IPPROTO_IPV6, IPV6_RECVPKTINFO, &sockTRUE, sizeof(sockTRUE));
    #endif
    memset(&addrSF, 0x00, sizeof(addrSF));
    addrSF.sin6_family = AF_INET6;
    addrSF.sin6_addr = in6addr_any;
    addrSF.sin6_port = htons(QDIV_PORT);
    bind(*sockSF, (struct sockaddr*)&addrSF, sizeof(addrSF));
    encryptable_t packetIQ;
    struct sockaddr_storage addrIQ;
    socklen_t addrSZ;
    client_t* clientIQ;
    while(*pShutdown != 1) {
    	FD_ZERO(&sockRD);
		FD_SET(*sockSF, &sockRD);
		qTimeOut.tv_sec = 1;
		if(select((*sockSF) + 1, &sockRD, NULL, NULL, &qTimeOut) > 0) {
			addrSZ = sizeof(struct sockaddr_in6);
			int32_t result = recvfrom(*sockSF, (char*)&packetIQ, sizeof(encryptable_t), 0, (struct sockaddr*)&addrIQ, &addrSZ);
			int32_t clientSL = 0;
			int32_t emptySL = -1;
			while(clientSL < settings.MAX_PLAYERS) {
				if(client[clientSL].connected) {
					if(memcmp(&addrIQ, &client[clientSL].addr, sizeMin(addrSZ, client[clientSL].addrSZ)) == 0) {
						clientIQ = client + clientSL;
				        entity_t* entityIQ = clientIQ -> entity;
				        player_t* playerIQ = &entityIQ -> unique.Player;
				        AES_ctx_set_iv(&clientIQ -> AES_CTX, packetIQ.Iv);
				        AES_CBC_decrypt_buffer(&clientIQ -> AES_CTX, packetIQ.payload, QDIV_PACKET_SIZE);
				        if(entityIQ == NULL) {
				        	if(memcmp(packetIQ.payload, clientIQ -> challenge, QDIV_PACKET_SIZE) != 0) {
				        		clientIQ -> connected = false;
				        		printf("> %s rejected: Name not authentic\n", clientIQ -> name);
								break;
				        	}
							int32_t entitySL = spawnEntity(clientIQ -> name, clientIQ -> uuid, PLAYER, 0, 0, 0, 0, 0);
							entity_t* entityIQ = entity + entitySL;
							clientIQ -> entity = entityIQ;
							player_t* playerIQ = &entityIQ -> unique.Player;
							playerIQ -> client = clientIQ;
							playerIQ -> criterion = (uint32_t*)malloc(MAX_CRITERION * sizeof(uint32_t));
							memset(playerIQ -> criterion, 0x00, MAX_CRITERION * sizeof(uint32_t));
							int8_t fileName[35];
							sprintf(fileName, "criterion/CR.%s.qsm", clientIQ -> name);
							int8_t* buffer = NULL;
							size_t bufferSZ = QSMread(fileName, &buffer);
							if(bufferSZ != 0) {
								int32_t resultIQ;
								int32_t criterionSL = 0;
								uint32_t* criterionIQ = playerIQ -> criterion;
								size_t entryPT = 0;
								while(criterionSL < MAX_CRITERION) {
									QSMencode(&resultIQ, (void*)(criterionIQ + criterionSL), buffer, bufferSZ, &entryPT, criterionTemplate[criterionSL].desc, strlen(criterionTemplate[criterionSL].desc));
									criterionSL++;
								}
								free(buffer);
							}
							send_encrypted(0x06, (void*)entityIQ, sizeof(entity_t), clientIQ);
							send_encrypted(0x0B, (void*)&currentHour, sizeof(uint8_t), clientIQ);
							syncEntityField(entityIQ -> local[0][0], clientIQ);
							syncEntityField(entityIQ -> local[1][0], clientIQ);
							syncEntityField(entityIQ -> local[2][0], clientIQ);
							syncEntityField(entityIQ -> local[0][1], clientIQ);
							syncEntityField(entityIQ -> local[1][1], clientIQ);
							syncEntityField(entityIQ -> local[2][1], clientIQ);
							syncEntityField(entityIQ -> local[0][2], clientIQ);
							syncEntityField(entityIQ -> local[1][2], clientIQ);
							syncEntityField(entityIQ -> local[2][2], clientIQ);
							printf("> %s connected\n", clientIQ -> name);
						}else{
					        switch(packetIQ.payload[0]) {
					        	case 0x01:
					        		removeEntity(clientIQ -> entity -> slot);
									syncEntityRemoval(clientIQ -> entity -> slot);
					        		clientIQ -> entity = NULL;
									clientIQ -> connected = false;
									printf("> %s disconnected: Remote\n", clientIQ -> name);
					        	case 0x02:
					        		int32_t direction;
					        		memcpy(&direction, packetIQ.payload + 1, sizeof(int));
					        		if(changeDirection(entityIQ, direction)) syncEntity(entityIQ);
					        		break;
					        	case 0x05:
					        		encryptable_seg encSegIQ;
					        		segment_t segmentIQ;
					        		memcpy(&segmentIQ, packetIQ.payload + 1, sizeof(segment_t));
					        		QDIV_RANDOM(encSegIQ.Iv, AES_BLOCKLEN);
					        		memcpy(encSegIQ.payload, entityIQ -> local[segmentIQ.lclX][segmentIQ.lclY] -> block[segmentIQ.posX], 32768);
					        		AES_ctx_set_iv(&clientIQ -> AES_CTX, encSegIQ.Iv);
									AES_CBC_encrypt_buffer(&clientIQ -> AES_CTX, encSegIQ.payload, 32768);
									struct sockaddr_storage addrIQ = clientIQ -> addr;
									sendto(*sockSF, (char*)&encSegIQ, sizeof(encryptable_seg), 0, (struct sockaddr*)&addrIQ, clientIQ -> addrSZ);
									break;
					        	case 0x08:
					        		int32_t roleIQ, artifactIQ;
					        		memcpy(&roleIQ, packetIQ.payload + 1, sizeof(int32_t));
					        		memcpy(&artifactIQ, packetIQ.payload + 1 + sizeof(int32_t), sizeof(int32_t));
					        		if(isArtifactUnlocked(roleIQ, artifactIQ, entityIQ)) {
					        			if(playerIQ -> role != roleIQ) {
					        				playerIQ -> roleTM = 3600.0;
					        				playerIQ -> role = roleIQ;
					        			}
					        			playerIQ -> artifact = artifactIQ;
					        			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), clientIQ);
					        		}
					        		break;
					        	case 0x0A:
					        		usage_t usageIQ;
					        		memcpy(&usageIQ, packetIQ.payload + 1, sizeof(usage_t));
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
					    break;
					}
				}else if(emptySL == -1) emptySL = clientSL;
				clientSL++;
			}
			if(clientSL == settings.MAX_PLAYERS && emptySL != -1) {
				clientIQ = &client[emptySL];
				clientIQ -> connected = true;
				clientIQ -> entity = NULL;
				clientIQ -> addr = addrIQ;
				clientIQ -> addrSZ = addrSZ;
				uint8_t AES_IV[16];
				memcpy(clientIQ -> name, packetIQ.Iv, 16);
				int8_t fileName[37];
				sprintf(fileName, "player/identity/%s.qid", clientIQ -> name);
				fileName[36] = 0x00;
				FILE* identityFile = fopen(fileName, "rb");
				if(identityFile == NULL) {
					clientIQ -> connected = false;
					printf("> %s rejected: Not whitelisted\n", clientIQ -> name);
					goto reiterate;
				}
				fread(clientIQ -> uuid, 1, 16, identityFile);
				fclose(identityFile);
				AES_init_ctx(&clientIQ -> AES_CTX, clientIQ -> uuid);
				QDIV_RANDOM(clientIQ -> challenge, QDIV_PACKET_SIZE);
				QDIV_RANDOM(packetIQ.Iv, AES_BLOCKLEN);
				memcpy(packetIQ.payload, clientIQ -> challenge, QDIV_PACKET_SIZE);
				struct sockaddr_storage addrIQ = clientIQ -> addr;
				sendto(*sockSF, (char*)&packetIQ, sizeof(encryptable_t), 0, (struct sockaddr*)&addrIQ, clientIQ -> addrSZ);
			}
			reiterate:
		}
    }
    for(clientSL = 0; clientSL < settings.MAX_PLAYERS; clientSL++) {
        if(client[clientSL].connected) {
            send_encrypted(0x01, NULL, 0, client + clientSL);
            removeEntity(client[clientSL].entity -> slot);
            printf("> %s disconnected: Self\n", client[clientSL].name);
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
			for(repClk = 0; repClk < settings.MAX_PLAYERS; repClk++) {
				printf("%d ", client[repClk].connected);
				if((repClk+1) % 8 == 0) {
					printf("\n");
				}
			}
			printf("\n");
			puts("–– Field Table ––");
			int32_t fieldTable[9][9] = {0};
			for(repClk = 0; repClk < settings.MAX_FIELDS; repClk++) {
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
	settings.PORT = 38652;
	settings.MAX_PLAYERS = 64;
	settings.PLAYER_SPEED = 1;
	settings.WORLD_SEED = 34658323;
	settings.MAX_ENTITIES = 10000;
	int8_t* bufferIQ;
	int32_t resultIQ;
	size_t bufferSZ = QSMread("settings.qsm", &bufferIQ);
	if(bufferIQ != NULL) {
		QSMencode(&resultIQ, (void*)&settings.PORT, bufferIQ, bufferSZ, NULL, "Port", 4);
		if(resultIQ != QSM_SIGNED_INT || settings.PORT > 65535) goto abort;
		QSMencode(&resultIQ, (void*)&settings.MAX_PLAYERS, bufferIQ, bufferSZ, NULL, "MaxPlayers", 10);
		if(resultIQ != QSM_SIGNED_INT) goto abort;
		QSMencode(&resultIQ, (void*)&settings.PLAYER_SPEED, bufferIQ, bufferSZ, NULL, "PlayerSpeed", 11);
		if(resultIQ != QSM_SIGNED_INT) goto abort;
		QSMencode(&resultIQ, (void*)&settings.MAX_ENTITIES, bufferIQ, bufferSZ, NULL, "MaxEntities", 11);
		if(resultIQ != QSM_SIGNED_INT) goto abort;
		QSMencode(&resultIQ, (void*)&settings.WORLD_SEED, bufferIQ, bufferSZ, NULL, "WorldSeed", 9);
		if(resultIQ != QSM_SIGNED_INT) goto abort;
	}
	settings.MAX_FIELDS = settings.MAX_PLAYERS * 9;
	memset(entity, 0x00, sizeof(entity));
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) entity[entitySL].type = -1;
	client = malloc(sizeof(client_t) * settings.MAX_PLAYERS);
	field = malloc(sizeof(field_t) * settings.MAX_FIELDS);
	memset(client, 0x00, sizeof(client_t) * settings.MAX_PLAYERS);
	memset(field, 0x00, sizeof(field_t) * settings.MAX_FIELDS);
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
	initGenerator(settings.WORLD_SEED);
	srand(time(NULL));
	puts("> Setting up Tick Stamper");
	struct timeval qTickStart;
	struct timeval qTickEnd;
	double blockUpdateTM = 0;
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
				client_t* clientIQ;
				if(entityIQ -> type == PLAYER) clientIQ = entityIQ -> unique.Player.client;
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
						syncEntityField(entityIQ -> local[2][0], clientIQ);
						syncEntityField(entityIQ -> local[2][1], clientIQ);
						syncEntityField(entityIQ -> local[2][2], clientIQ);
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
						syncEntityField(entityIQ -> local[0][0], clientIQ);
						syncEntityField(entityIQ -> local[0][1], clientIQ);
						syncEntityField(entityIQ -> local[0][2], clientIQ);
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
						syncEntityField(entityIQ -> local[0][2], clientIQ);
						syncEntityField(entityIQ -> local[1][2], clientIQ);
						syncEntityField(entityIQ -> local[2][2], clientIQ);
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
						syncEntityField(entityIQ -> local[0][0], clientIQ);
						syncEntityField(entityIQ -> local[1][0], clientIQ);
						syncEntityField(entityIQ -> local[2][0], clientIQ);
					}
				}
				if(entityType[entityIQ -> type].action != NULL) (*entityType[entityIQ -> type].action)((void*)entityIQ);
				if(entityIQ -> healthTM < 60.0) {
					entityIQ -> healthTM += qFactor;
				}else if(entityIQ -> health < entityType[entityIQ -> type].maxHealth) {
					entityIQ -> healthTM = 0.0;
					entityIQ -> health++;
					syncEntity(entityIQ);
				}
			}
		}
		blockUpdateTM += qFactor;
		if(blockUpdateTM > 4.0) {
			blockUpdateTM = 0.0;
			block_ust* uniqueIQ = uniqueBlock;
			block_l* locationIQ;
			while(uniqueIQ != NULL) {
				locationIQ = &uniqueIQ -> location;
				switch(block[locationIQ -> layer][locationIQ -> block].type) {
					case CROP:
						uniqueIQ -> data.timer++;
						crop_st* cropIQ = &block[locationIQ -> layer][locationIQ -> block].unique.crop;
						if(uniqueIQ -> data.timer == cropIQ -> growthDuration) {
							locationIQ -> block = cropIQ -> successor;
							int32_t fieldSL = getFieldSlot(locationIQ -> zone, locationIQ -> fldX, locationIQ -> fldY);
							if(fieldSL > -1 && fieldSL < settings.MAX_FIELDS) {
								/*field[fieldSL].block[locationIQ -> layer][locationIQ -> posX][locationIQ -> posY] = locationIQ -> block;
								syncBlock(locationIQ -> block, field + fieldSL, locationIQ -> posX, locationIQ -> posY, locationIQ -> layer);*/
								setBlock(locationIQ -> block, field + fieldSL, locationIQ -> posX, locationIQ -> posY, locationIQ -> layer);
							}
						}
				}
				uniqueIQ = uniqueIQ -> next;
			}
		}
	}
	pthread_join(gate_id, NULL);
	pthread_join(console_id, NULL);
	free(client);
	free(field);
	block_ust* uniqueTMP;
	block_ust* uniqueIQ = uniqueBlock;
	while(uniqueIQ != NULL) {
		uniqueTMP = uniqueIQ;
		uniqueIQ = uniqueIQ -> next;
		free(uniqueTMP);
	}
	abort:
	free(bufferIQ);
}
