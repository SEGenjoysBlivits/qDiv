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
	int32_t SPAWN_RATE;
	int32_t WORLD_SEED;
	bool UNLOCK_ALL;
} settings;

int32_t qTickTime;
int32_t* pTickTime = &qTickTime;
bool TickQuery = false;
bool* pTickQuery = &TickQuery;
entity_t* adminEntity = NULL;
uint16_t adminBlock = 0;
int32_t adminLayer = 0;
uint16_t* adminBlockPT = &adminBlock;
int32_t* adminLayerPT = &adminLayer;

field_t* field;
client_t* client;

time_t serverTime;

// Synchronizer
void syncEntityRemoval(int32_t slot) {
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].type == PLAYER && entityInRange(entity + slot, entity + entitySL)) send_encrypted(0x03, (void*)&slot, sizeof(int32_t), entity[entitySL].unique.Player.client);
	}
}

// Synchronizer
void syncEntity(entity_t* entityIQ) {
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].active == 1 && entity[entitySL].type == PLAYER && entityInRange(entityIQ, entity + entitySL)) send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), entity[entitySL].unique.Player.client);
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

void saveField(field_t* fieldIQ) {
	int8_t fileName[64];
	sprintf(fileName, "world/fld.%dz.%dx.%dy.dat", fieldIQ -> zone, fieldIQ -> fldX, fieldIQ -> fldY);
	FILE* fileData = fopen(fileName, "wb");
	fwrite(fieldIQ, 1, sizeof(field_t), fileData);
	fclose(fileData);
}

void saveCriteria(entity_t* entityIQ) {
	int8_t filePath[34];
	int8_t* bufferIQ = NULL;
	size_t bufferSZ = 0;
	sprintf(filePath, "criterion/CR.%s.qsm", entityIQ -> name);
	int32_t criterionSL = 0;
	uint32_t* criterionIQ = entityIQ -> unique.Player.criterion;
	while(criterionSL < MAX_CRITERION) {
		if(criterionIQ[criterionSL] != 0) {
			bufferSZ = QSMdecode(QSM_UNSIGNED_INT, (void*)(criterionIQ + criterionSL), (size_t)log10(*(criterionIQ + criterionSL)) + 1, &bufferIQ, bufferSZ, criterionTemplate[criterionSL].desc, strlen(criterionTemplate[criterionSL].desc));
		}
		criterionSL++;
	}
	QSMwrite(filePath, &bufferIQ, bufferSZ);
}

void saveEntity(entity_t* entityIQ) {
	FILE* entityFile;
	int8_t hex[QDIV_B16 + 1];
	int8_t fileName[QDIV_B16 + 11];
	uuidToHex(entityIQ -> uuid, hex);
	sprintf(fileName, "entity/%s.dat", hex);
	entityFile = fopen(fileName, "wb");
	if(entityFile != NULL) {
		fwrite(entityIQ, 1, sizeof(entity_t), entityFile);
		fclose(entityFile);
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
				if(fieldIQ -> edit) {
					saveField(fieldIQ);
				}
				fieldIQ -> active = 0;
			}
			break;
		}
	}
}

#define QDIV_DELETE_ENTITY(slot) {\
	entity[slot].active = 0;\
	entityTable[slot] = false;\
}

void removePlayer(int32_t slot) {
	QDIV_DELETE_ENTITY(slot);
	unloadField(entity[slot].zone, entity[slot].fldX, entity[slot].fldY);
	unloadField(entity[slot].zone, entity[slot].fldX+1, entity[slot].fldY);
	unloadField(entity[slot].zone, entity[slot].fldX-1, entity[slot].fldY);
	unloadField(entity[slot].zone, entity[slot].fldX, entity[slot].fldY+1);
	unloadField(entity[slot].zone, entity[slot].fldX+1, entity[slot].fldY+1);
	unloadField(entity[slot].zone, entity[slot].fldX-1, entity[slot].fldY+1);
	unloadField(entity[slot].zone, entity[slot].fldX, entity[slot].fldY-1);
	unloadField(entity[slot].zone, entity[slot].fldX+1, entity[slot].fldY-1);
	unloadField(entity[slot].zone, entity[slot].fldX-1, entity[slot].fldY-1);
	saveCriteria(entity + slot);
	free(entity[slot].unique.Player.criterion);
	saveEntity(entity + slot);
}

void setLocals(entity_t* entityIQ);
int32_t loadField(int32_t inZone, int32_t inFldX, int32_t inFldY);
int32_t spawnEntity(int8_t* name, uint8_t* uuid, int32_t inType, int32_t inZone, int32_t inFldX, int32_t inFldY, double inPosX, double inPosY);

bool damageEntity(entity_t* entityIQ, uint64_t decay) {
	if(entityIQ -> health < 0) return false;
	entityIQ -> healthTM = 0;
	entityIQ -> health--;
	if(entityIQ -> health == 0 || decay >= entityIQ -> qEnergy) {
		if(entityIQ -> type == PLAYER) {
			int32_t zonePR = entityIQ -> zone;
			int32_t fldPRX = entityIQ -> fldX;
			int32_t fldPRY = entityIQ -> fldY;
			entityIQ -> zone = OVERWORLD;
			entityIQ -> fldX = 0;
			entityIQ -> fldY = 0;
			entityIQ -> posX = 64;
			entityIQ -> posY = 64;
			entityIQ -> motX = 0;
			entityIQ -> motY = 0;
			entityIQ -> health = 5;
			entityIQ -> qEnergy = 1;
			entityIQ -> healthTM = 60.0;
			saveEntity(entityIQ);
			unloadField(zonePR, fldPRX, fldPRY);
			unloadField(zonePR, fldPRX+1, fldPRY);
			unloadField(zonePR, fldPRX-1, fldPRY);
			unloadField(zonePR, fldPRX, fldPRY+1);
			unloadField(zonePR, fldPRX+1, fldPRY+1);
			unloadField(zonePR, fldPRX-1, fldPRY+1);
			unloadField(zonePR, fldPRX, fldPRY-1);
			unloadField(zonePR, fldPRX+1, fldPRY-1);
			unloadField(zonePR, fldPRX-1, fldPRY-1);
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
			client_t* clientIQ = entityIQ -> unique.Player.client;
			send_encrypted(0x0C, (void*)entityIQ, sizeof(entity_t), clientIQ);
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
		}else{
			QDIV_DELETE_ENTITY(entityIQ -> slot);
		}
		syncEntityRemoval(entityIQ -> slot);
		return true;
	}else{
		entityIQ -> qEnergy -= decay;
		return false;
	}
}

bool isEntityPresent(entity_t* entityIQ, int32_t fldX, int32_t fldY, double posX, double posY) {
	double boxIQ = entityType[entityIQ -> type].hitBox;
	return fabs(entityIQ -> posX - posX + (double)((entityIQ -> fldX - fldX) * 128)) < boxIQ && fabs(entityIQ -> posY - posY + (double)((entityIQ -> fldY - fldY) * 128)) < boxIQ;
}

bool isEntityPresentInRange(entity_t* entityIQ, int32_t fldX, int32_t fldY, double centerX, double centerY, double range) {
	double boxIQ = entityType[entityIQ -> type].hitBox;
	double posRelX = (double)((entityIQ -> fldX - fldX) * 128);
	double posRelY = (double)((entityIQ -> fldY - fldY) * 128);
	return entityIQ -> posX + boxIQ + posRelX > centerX - range && entityIQ -> posX - boxIQ + posRelX < centerX + range && entityIQ -> posY + boxIQ + posRelY > centerY - range && entityIQ -> posY - boxIQ + posRelY < centerY + range;
}

void setBlock(uint16_t blockIQ, field_t* fieldIQ, int32_t posX, int32_t posY, int32_t layer) {
	fieldIQ -> edit = true;
	fieldIQ -> block[posX][posY][layer] = blockIQ;
	memset(&fieldIQ -> unique[posX][posY][layer], 0x00, sizeof(block_ust));
	switch(block[layer][blockIQ].type) {
		case TIME_CHECK:
			fieldIQ -> unique[posX][posY][layer].successionTM = (uint64_t)serverTime + block[layer][blockIQ].unique.timeCheck.duration;
			break;
	}
	syncBlock(blockIQ, fieldIQ, posX, posY, layer);
}

// Block Usage
def_BlockAction(igniteLamp) {
	if(qEnergyConsume(1, (entity_t*)entityVD)) {
		setBlock(((block_st*)blockVD) -> unique.litSelf, fieldIQ, posX, posY, layer);
	}
}

// Block Usage
def_BlockAction(lavaDamage) {
	entity_t* entityIQ = (entity_t*)entityVD;
	if(entityIQ -> healthTM > 1.0) damageEntity(entityIQ, 10);
}

// Artifact Action
def_ArtifactAction(placeBlock) {
	artifact_st* artifactIQ = (artifact_st*)artifactVD;
	placeBlock_st* settingsIQ = (placeBlock_st*)settingsVD;
	if(playerIQ -> useTM >= artifactIQ -> primary.useTM) {
		playerIQ -> useTM = 0;
		int32_t blockX = (int32_t)posX;
		int32_t blockY = (int32_t)posY;
		int32_t layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		collection_st* collectionIQ = block[settingsIQ -> layer][settingsIQ -> represent].placeable;
		if(layerSL < settingsIQ -> layer && entityIQ -> qEnergy -1 >= 1 && (settingsIQ -> layer == 0 || collectionIQ == NULL || inCollection((int32_t)fieldIQ -> block[blockX][blockY][0], collectionIQ))) {
			entityIQ -> qEnergy -= 1;
			setBlock(settingsIQ -> represent, fieldIQ, blockX, blockY, settingsIQ -> layer);
			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), playerIQ -> client);
		}
	}
}

// Artifact Action
def_ArtifactAction(mineBlock) {
	if(playerIQ -> useTM >= ((artifact_st*)artifactVD) -> primary.useTM) {
		playerIQ -> useTM = 0;
		int32_t blockX = (int32_t)posX;
		int32_t blockY = (int32_t)posY;
		int32_t layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		if(layerSL != -1) {
			block_st* blockIQ = &block[layerSL][fieldIQ -> block[blockX][blockY][layerSL]];
			if(!inCollection(blockIQ -> type, &unbreakableBlock)) {
				entityIQ -> qEnergy += qEnergyRelevance(entityIQ -> qEnergy, blockIQ -> qEnergy);
				if(entityIQ -> qEnergy > playerIQ -> qEnergyMax) playerIQ -> qEnergyMax = entityIQ -> qEnergy;
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
}

// Artifact Action
def_ArtifactAction(sliceEntity) {
	artifact_st* artifactIQ = (artifact_st*)artifactVD;
	sliceEntity_st* settingsIQ = (sliceEntity_st*)settingsVD;
	if(playerIQ -> useTM == 0) entityIQ -> qEnergy -= artifactIQ -> primary.cost;
	for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
		if(entityTable[entitySL] && entity[entitySL].active == 1 && entityInRange(entity + entitySL, entityIQ) && entity[entitySL].healthTM > 1.0 && entityType[entity[entitySL].type].maxHealth != 0 && entity + entitySL != entityIQ) {
			int32_t pivotSL = 1;
			float scale;
			while(pivotSL <= settingsIQ -> range) {
				scale = pivotSL * QDIV_HITBOX_UNIT;
				//printf("%f %f\n", entityIQ -> posX + (scale * (playerIQ -> useRelX / fabs(playerIQ -> useRelX)) * cos(playerIQ -> useTM * 6.28 / artifactIQ -> primary.useTM)), entityIQ -> posY + (scale * sin(playerIQ -> useTM * 6.28 / artifactIQ -> primary.useTM)));
				if(isEntityPresent(entity + entitySL, entityIQ -> fldX, entityIQ -> fldY, entityIQ -> posX + (scale * (playerIQ -> useRelX / fabs(playerIQ -> useRelX)) * cos(playerIQ -> useTM * 6.28 / artifactIQ -> primary.useTM)), entityIQ -> posY + (scale * sin(playerIQ -> useTM * 6.28 / artifactIQ -> primary.useTM)))) {
					int32_t criterionTP = entityType[entity[entitySL].type].kill_criterion;
					if(damageEntity(entity + entitySL, settingsIQ -> decay) && criterionTP != NO_CRITERION) {
						criterion_t criterionIQ = {criterionTP, ++playerIQ -> criterion[criterionTP]};
						send_encrypted(0x09, (void*)&criterionIQ, sizeof(criterion_t), playerIQ -> client);
					}
					break;
				}
				pivotSL++;
			}
		}
	}
	if(playerIQ -> useTM >= artifactIQ -> primary.useTM) playerIQ -> useTM = 0;
}

// Artifact Action
def_ArtifactAction(fertilizeBlock) {
	if(playerIQ -> useTM >= ((artifact_st*)artifactVD) -> secondary.useTM) {
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

// Artifact Action
def_ArtifactAction(scoopWater) {
	if(playerIQ -> useTM >= ((artifact_st*)artifactVD) -> primary.useTM) {
		playerIQ -> useTM = 0;
		int32_t blockX = (int32_t)posX;
		int32_t blockY = (int32_t)posY;
		if(fieldIQ -> block[blockX][blockY][0] == WATER) {
			entityIQ -> qEnergy += qEnergyRelevance(entityIQ -> qEnergy, 20);
			setBlock(NO_WALL, fieldIQ, blockX, blockY, 0);
			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), playerIQ -> client);
		}
	}
}

// Artifact Action
def_ArtifactAction(buildAdmin) {
	if(entityIQ == adminEntity && playerIQ -> useTM >= 0.1) {
		playerIQ -> useTM = 0;
		int32_t blockX = (int32_t)posX;
		int32_t blockY = (int32_t)posY;
		setBlock(*adminBlockPT, fieldIQ, blockX, blockY, *adminLayerPT);
		send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), playerIQ -> client);
	}
}

// Artifact Action
def_ArtifactAction(openLootBox) {
	artifact_st* artifactIQ = (artifact_st*)artifactVD;
	if(playerIQ -> role == EXPLORER && playerIQ -> useTM >= artifactIQ -> primary.useTM && fieldIQ -> block[(int32_t)posX][(int32_t)posY][1] == artifactIQ -> primary.unique.lootBox) {
		block_st* blockIQ = &block[1][artifactIQ -> primary.unique.lootBox];
		playerIQ -> useTM = 0;
		for(int32_t lootSL = 0; lootSL < blockIQ -> unique.lootbox.lootAmount; lootSL++) {
			uint32_t value;
			QDIV_RANDOM(&value, sizeof(uint32_t));
			int32_t templateIQ = blockIQ -> unique.lootbox.lootTable -> elements[value % blockIQ -> unique.lootbox.lootTable -> length];
			criterion_t criterionIQ = {templateIQ, ++playerIQ -> criterion[templateIQ]};
			send_encrypted(0x09, (void*)&criterionIQ, sizeof(criterion_t), playerIQ -> client);
		}
		setBlock(NO_WALL, fieldIQ, posX, posY, 1);
	}
}

// Artifact Action
def_ArtifactAction(shootProjectile) {
	artifact_st* artifactIQ = (artifact_st*)artifactVD;
	if(playerIQ -> useTM >= artifactIQ -> primary.useTM && qEnergyConsume(artifactIQ -> primary.cost, entityIQ)) {
		playerIQ -> useTM = 0;
		double hypothenuse = sqrt(pow(playerIQ -> useRelX, 2) + pow(playerIQ -> useRelY, 2));
		double unitX = playerIQ -> useRelX / hypothenuse;
		double unitY = playerIQ -> useRelY / hypothenuse;
		int32_t entitySL = spawnEntity("Entity", uuidNull, KOBATINE_BLAST, entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY, entityIQ -> posX + (2 * unitX), entityIQ -> posY + (2 * unitY));
		if(entitySL != -1) {
			entity_t* entityIR = entity + entitySL;
			entityIR -> motX = unitX * entityType[entityIR -> type].speed;
			entityIR -> motY = unitY * entityType[entityIR -> type].speed;
			syncEntity(entityIR);
		}
	}
}

// Artifact Action
def_ArtifactAction(vivoSpell) {
	artifact_st* artifactIQ = (artifact_st*)artifactVD;
	if(playerIQ -> useTM >= artifactIQ -> primary.useTM && qEnergyConsume(artifactIQ -> primary.cost, entityIQ)) {
		playerIQ -> useTM = 0;
		int32_t blockX = (int32_t)posX;
		int32_t blockY = (int32_t)posY;
		int32_t layerSL = getOccupiedLayer(fieldIQ -> block[blockX][blockY]);
		uint16_t grassIQ = fieldIQ -> block[blockX][blockY][0];
		if(layerSL == 0 && block[0][grassIQ].type == FERTILE && qEnergyConsume(artifactIQ -> primary.cost, entityIQ)) {
			int32_t value = rand();
			value = value % 1000;
			uint16_t blockIQ;
			switch(grassIQ) {
				case SHALLAND_FLATGRASS:
					if(value > 750) {
						blockIQ = SHALLAND_BUSH;
					}else{
						blockIQ = SHALLAND_GRASS;
					}
					break;
				case ARIDIS_FLATGRASS:
					if(value > 500) {
						blockIQ = ARIDIS_BUSH;
					}else{
						blockIQ = ARIDIS_GRASS;
					}
					break;
				case REDWOOD_FLATGRASS:
					if(value > 500) {
						blockIQ = REDWOOD_TREE;
					}else{
						blockIQ = REDWOOD_WEEDS;
					}
					break;
				case TROPICAL_FLATGRASS:
					if(value > 375) {
						blockIQ = COFFEE_BUSH;
					}else{
						blockIQ = FERN;
					}
					break;
			}
			setBlock(blockIQ, fieldIQ, blockX, blockY, 1);
			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), playerIQ -> client);
		}
	}
}

// Artifact Action
def_ArtifactAction(shearSheep) {
	artifact_st* artifactIQ = (artifact_st*)artifactVD;
	if(playerIQ -> useTM >= artifactIQ -> primary.useTM) {
		playerIQ -> useTM = 0;
		int32_t entitySL = 0;
		entity_t* entityIR;
		while(entitySL < settings.MAX_ENTITIES) {
			entityIR = entity + entitySL;
			if(entityTable[entitySL] && entityIR -> active && entityIQ -> type == SHEEP && entityInRange(entityIR, entityIQ) && isEntityPresent(entityIR, entityIQ -> fldX, entityIQ -> fldY, posX, posY)) {
				criterion_t criterionIQ = {ACQUIRE_WOOL, ++playerIQ -> criterion[ACQUIRE_WOOL]};
				send_encrypted(0x09, (void*)&criterionIQ, sizeof(criterion_t), playerIQ -> client);
				break;
			}
			entitySL++;
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
					if(entityIQ -> local[lclX][lclY] != NULL && block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].friction != 1.0) {
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
					if(entityIQ -> local[lclX][lclY] != NULL && block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].friction != 1.0) {
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
					if(entityIQ -> local[lclX][lclY] != NULL && block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].friction != 1.0) {
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
					if(entityIQ -> local[lclX][lclY] != NULL && block[1][entityIQ -> local[lclX][lclY] -> block[(int)minX][(int)minY][1]].friction != 1.0) {
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
	return entityType[entityIQ -> type].speed * block[0][entityIQ -> local[1][1] -> block[(int32_t)entityIQ -> posX][(int32_t)entityIQ -> posY][0]].friction;
}

bool changeDirection(entity_t* entityIQ, int32_t direction) {
	double speedIQ = entityType[entityIQ -> type].speed;
	double* inMotX = &entityIQ -> motX;
	double* inMotY = &entityIQ -> motY;
	entity_t entityCK = *entityIQ;
	switch(direction) {
		case NORTH:
			entityCK.posY += speedIQ * 0.02;
			break;
		case EAST:
			entityCK.posX += speedIQ * 0.02;
			break;
		case SOUTH:
			entityCK.posY -= speedIQ * 0.02;
			break;
		case WEST:
			entityCK.posX -= speedIQ * 0.02;
			break;
	}
	if(isEntityObstructed(&entityCK, direction)) return false;
	switch(direction) {
		case NORTH:
			*inMotY = speedIQ;
			break;
		case EAST:
			*inMotX = speedIQ;
			break;
		case SOUTH:
			*inMotY = -speedIQ;
			break;
		case WEST:
			*inMotX = -speedIQ;
			break;
		case NORTH_STOP:
			if(*inMotY > 0) *inMotY = 0;
			break;
		case EAST_STOP:
			if(*inMotX > 0) *inMotX = 0;
			break;
		case SOUTH_STOP:
			if(*inMotY < 0) *inMotY = 0;
			break;
		case WEST_STOP:
			if(*inMotX < 0) *inMotX = 0;
			break;
	}
	return true;
}

/*
typedef struct {
	bool traversable;
	int32_t Fcost;
	int32_t Gcost;
	int32_t Hcost;
} astar_node_t;

astar_node_t* astar_findPath(entity_t* entityIQ, int32_t endX, int32_t endY) {
	entity_st* typeIQ = entityType + entityIQ -> type;
	size_t sideSZ = ((size_t)24 * 2) + 1;
	astar_node_t* gridIQ = malloc(sideSZ * sideSZ);
	memset(gridIQ, 0x00, sideSZ * sideSZ);
	int32_t posX = 0;
	int32_t posY = 0;
	int32_t lclX, lclY, relX, relY;
	field_t* fieldIQ;
	while(posY < sideSZ) {
		fieldIQ = entityIQ -> local[(posX + 128) / 128][(posY + 128) / 128];
		if(fieldIQ == NULL) {
			gridIQ[posX][posY].traversable = false;
		}else{
			relX = posX - (int32_t)entityIQ -> posX;
			relY = posY - (int32_t)entityIQ -> posY;
		}
		posX++;
		if(posX == sideSZ) {
			posX = 0;
			posY++;
		}
	}
}*/

int32_t selectEntityType(field_t* fieldIQ, double posX, double posY) {
	//return NULL_ENTITY;
	int32_t blockX = (int32_t)posX;
	int32_t blockY = (int32_t)posY;
	uint16_t floorIQ = fieldIQ -> block[blockX][blockY][0];
	uint16_t wallIQ = fieldIQ -> block[blockX][blockY][1];
	uint16_t biomeIQ = fieldIQ -> biome[blockX][blockY];
	uint32_t randomIQ;
	QDIV_RANDOM(&randomIQ, sizeof(uint32_t));
	randomIQ = randomIQ % 1000;
	switch(biomeIQ) {
		case SHALLAND:
			if(floorIQ == SHALLAND_FLATGRASS && wallIQ == NO_WALL) {
				if(randomIQ > 900) {
					return SHALLAND_SNAIL;
				}
			}
			break;
		case REDWOOD_FOREST:
			if(floorIQ == REDWOOD_FLATGRASS && (currentHour < 6 || currentHour >= 18)) {
				if(randomIQ > 950) {
					return WISP;
				}
			}
			break;
		case SEDIMENTARY_CAVES:
			if(floorIQ == LIMESTONE_FLOOR && wallIQ == NO_WALL) {
				if(randomIQ > 900) {
					return CALCIUM_CRAWLER;
				}
			}
			break;
		case KOBATINE_HILLS:
			if(floorIQ == KOBATINE_FLOOR && wallIQ == NO_WALL) {
				if(randomIQ > 900) {
					return KOBATINE_SNAIL;
				}
				return KOBATINE_BUG;
			}
			break;
		case BLUEBERRY_SHALLAND:
			if(floorIQ == SHALLAND_FLATGRASS && wallIQ == NO_WALL) {
				if(randomIQ > 950) {
					return SHALLAND_SNAIL;
				}else if(randomIQ > 900) {
					return WILD_SHEEP;
				}
			}
			break;
	}
	return NULL_ENTITY;
}

// Entity Action
void playerAction(void* entityVD) {
	entity_t* entityIQ = (entity_t*)entityVD;
	player_t* playerIQ = &entityIQ -> unique.Player;
	client_t* clientIQ = playerIQ -> client;
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
		if(playerIQ -> currentUsage == BLOCK_USAGE) {
			playerIQ -> currentUsage = NO_USAGE;
			playerIQ -> useTM = 0;
			int32_t blockX = (int32_t)posX;
			int32_t blockY = (int32_t)posY;
			int32_t layer = getOccupiedLayer(entityIQ -> local[lclX][lclY] -> block[blockX][blockY]);
			if(layer != -1) {
				block_st* blockIQ = &block[layer][entityIQ -> local[lclX][lclY] -> block[blockX][blockY][layer]];
				if(blockIQ -> usage != NULL) {
					(*blockIQ -> usage)(entityIQ -> local[lclX][lclY], blockX, blockY, layer, (void*)entityIQ, (void*)blockIQ);
				}
			}
		}else{
			artifact_st* artifactIQ = &artifact[playerIQ -> role][playerIQ -> artifact];
			if(playerIQ -> currentUsage == PRIMARY_USAGE && artifactIQ -> primary.action != NULL) {
				(*artifactIQ -> primary.action)(entityIQ -> local[lclX][lclY], posX, posY, entityIQ, playerIQ, (void*)artifactIQ, (void*)&artifactIQ -> primary.unique);
			}else if(playerIQ -> currentUsage == SECONDARY_USAGE && artifactIQ -> secondary.action != NULL) {
				(*artifactIQ -> secondary.action)(entityIQ -> local[lclX][lclY], posX, posY, entityIQ, playerIQ, (void*)artifactIQ, (void*)&artifactIQ -> secondary.unique);
			}else playerIQ -> useTM = 0;
		}
	}
	playerIQ -> spawnTM += qFactor;
	if(playerIQ -> spawnTM > (1.0 / (double)settings.SPAWN_RATE)) {
		playerIQ -> spawnTM = 0.0;
		uint32_t value;
		int32_t lclX, lclY, fldX, fldY;
		double posX, posY;
		QDIV_RANDOM(&value, sizeof(uint32_t));
		switch((value % 512) / 128) {
			case NORTH:
				posX = (double)((int32_t)(value % 128)) - 64;
				posY = 64;
				break;
			case EAST:
				posX = 64;
				posY = (double)((int32_t)(value % 128)) - 64;
				break;
			case SOUTH:
				posX = (double)((int32_t)(value % 128)) - 64;
				posY = -64;
				break;
			case WEST:
				posX = -64;
				posY = (double)((int32_t)(value % 128)) - 64;
				break;
		}
		posX += entityIQ -> posX;
		posY += entityIQ -> posY;
		lclX = 1;
		lclY = 1;
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
		fldX = entityIQ -> fldX + lclX - 1;
		fldY = entityIQ -> fldY + lclY - 1;
		int32_t typeSL;
		int32_t entitySL = 0;
		entity_t* entityIR;
		while(entitySL < settings.MAX_ENTITIES) {
			entityIR = entity + entitySL;
			if(entityIR -> type == PLAYER && entityIR -> zone == entityIQ -> zone &&
			entityIR -> fldX < fldX + 2 && entityIR -> fldX > fldX - 2 &&
			entityIR -> fldY < fldY + 2 && entityIR -> fldY > fldY - 2 &&
			abs((int32_t)((posX + 128 * (lclX - 1)) - entityIR -> posX)) < 63 &&
			abs((int32_t)((posY + 128 * (lclY - 1)) - entityIR -> posY)) < 63) goto playerIsPresent;
			entitySL++;
		}
		typeSL = selectEntityType(entityIQ -> local[lclX][lclY], posX, posY);
		if(typeSL != NULL_ENTITY) {
			entitySL = spawnEntity("Entity", uuidNull, typeSL, entityIQ -> zone, entityIQ -> fldX + lclX - 1, entityIQ -> fldY + lclY - 1, posX, posY);
			if(entitySL != -1) syncEntity(entity + entitySL);
		}
		playerIsPresent:
	}
	if(playerIQ -> roleTM > 0.0) playerIQ -> roleTM -= qFactor;
	block_st* wallIQ = &block[1][entityIQ -> local[1][1] -> block[(int32_t)entityIQ -> posX][(int32_t)entityIQ -> posY][1]];
	if(wallIQ -> type == ZONE_PORTAL) {
		playerIQ -> portalTM += qFactor;
		if(playerIQ -> portalTM > 10.0) {
			playerIQ -> portalTM = 0;
			int32_t zonePR = entityIQ -> zone;
			entityIQ -> zone = wallIQ -> unique.portal.destination;
			unloadField(zonePR, entityIQ -> fldX-1, entityIQ -> fldY);
			unloadField(zonePR, entityIQ -> fldX-1, entityIQ -> fldY+1);
			unloadField(zonePR, entityIQ -> fldX-1, entityIQ -> fldY-1);
			unloadField(zonePR, entityIQ -> fldX, entityIQ -> fldY);
			unloadField(zonePR, entityIQ -> fldX, entityIQ -> fldY+1);
			unloadField(zonePR, entityIQ -> fldX, entityIQ -> fldY-1);
			unloadField(zonePR, entityIQ -> fldX+1, entityIQ -> fldY);
			unloadField(zonePR, entityIQ -> fldX+1, entityIQ -> fldY+1);
			unloadField(zonePR, entityIQ -> fldX+1, entityIQ -> fldY-1);
			loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY);
			loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY+1);
			loadField(entityIQ -> zone, entityIQ -> fldX-1, entityIQ -> fldY-1);
			loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY);
			loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY+1);
			loadField(entityIQ -> zone, entityIQ -> fldX, entityIQ -> fldY-1);
			loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY);
			loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY+1);
			loadField(entityIQ -> zone, entityIQ -> fldX+1, entityIQ -> fldY-1);
			setLocals(entityIQ);
			syncEntity(entityIQ);
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
		}
	}else{
		playerIQ -> portalTM = 0;
	}
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

// Entity Action
void noClipHostileAction(void* entityVD) {
	int32_t rng = rand();
	entity_t* entityIQ = (entity_t*)entityVD;
	ncHostile_t* hostileIQ = &entityIQ -> unique.ncHostile;
	hostileIQ -> searchTM += qFactor;
	if(hostileIQ -> target == NULL) {
		if(rng % 1600 < 8) {
			changeDirection(entityIQ, rng % 8);
			syncEntity(entityIQ);
		}
		if(hostileIQ -> searchTM > 1.0) {
			int32_t entitySL = 0;
			entity_t* entityIR;
			while(entitySL < settings.MAX_ENTITIES) {
				entityIR = entity + entitySL;
				if(entityTable[entitySL] && entityIR -> type == PLAYER && entityInRange(entityIR, entityIQ) &&
				fabs(entityIR -> posX - entityIQ -> posX) + (double)(abs(entityIR -> fldX - entityIQ -> fldX) * 128) < 24.0 &&
				fabs(entityIR -> posY - entityIQ -> posY) + (double)(abs(entityIR -> fldY - entityIQ -> fldY) * 128) < 24.0) {
					hostileIQ -> target = (void*)entityIR;
					break;
				}
				entitySL++;
			}
			hostileIQ -> searchTM = 0;
		}
	}else{
		entity_t* targetIQ = (entity_t*)hostileIQ -> target;
		double relX = targetIQ -> posX - entityIQ -> posX + (double)(targetIQ -> fldX - entityIQ -> fldX) * 128;
		double relY = targetIQ -> posY - entityIQ -> posY + (double)(targetIQ -> fldY - entityIQ -> fldY) * 128;
		double boxIQ = entityType[entityIQ -> type].hitBox;
		if(rng % 400 < 8) {
			if(relX > boxIQ) {
				changeDirection(entityIQ, EAST);
			}else if(relX < -boxIQ) {
				changeDirection(entityIQ, WEST);
			}else{
				changeDirection(entityIQ, EAST_STOP);
				changeDirection(entityIQ, WEST_STOP);
			}
			if(relY > boxIQ) {
				changeDirection(entityIQ, NORTH);
			}else if(relY < -boxIQ) {
				changeDirection(entityIQ, SOUTH);
			}else{
				changeDirection(entityIQ, NORTH_STOP);
				changeDirection(entityIQ, SOUTH_STOP);
			}
			syncEntity(entityIQ);
		}
		if(targetIQ -> healthTM > 1.0 && entityInRange(targetIQ, entityIQ) &&
		fabs(targetIQ -> posX - entityIQ -> posX) + (double)(abs(targetIQ -> fldX - entityIQ -> fldX) * 128) < boxIQ &&
		fabs(targetIQ -> posY - entityIQ -> posY) + (double)(abs(targetIQ -> fldY - entityIQ -> fldY) * 128) < boxIQ
		) {
			damageEntity(targetIQ, 10);
			syncEntity(targetIQ);
		}
		if(!(entityTable[targetIQ -> slot] && targetIQ -> active && targetIQ -> type == PLAYER && entityInRange(targetIQ, entityIQ) &&
			fabs(targetIQ -> posX - entityIQ -> posX) + (double)(abs(targetIQ -> fldX - entityIQ -> fldX) * 128) < 24.0 &&
			fabs(targetIQ -> posY - entityIQ -> posY) + (double)(abs(targetIQ -> fldY - entityIQ -> fldY) * 128) < 24.0)) hostileIQ -> target = NULL;
		hostileIQ -> searchTM = 0;
	}
}

// Entity Action
void friendAction(void* entityVD) {
	int32_t rng = rand();
	entity_t* entityIQ = (entity_t*)entityVD;
	friend_t* friendIQ = &entityIQ -> unique.Friend;
	friendIQ -> searchTM += qFactor;
	if(friendIQ -> searchTM > 1.0) {
		friendIQ -> searchTM = 0.0;
		if(memcmp(friendIQ -> friendID, uuidNull, QDIV_B256) == 0) {
			int32_t entitySL = 0;
			entity_t* entityIR;
			while(entitySL < settings.MAX_ENTITIES) {
				entityIR = entity + entitySL;
				if(entityTable[entitySL] && entityIR -> type == PLAYER && entityIR -> unique.Player.role == GARDENER && entityInRange(entityIR, entityIQ) &&
				fabs(entityIR -> posX - entityIQ -> posX) + (double)(abs(entityIR -> fldX - entityIQ -> fldX) * 128) < 8.0 &&
				fabs(entityIR -> posY - entityIQ -> posY) + (double)(abs(entityIR -> fldY - entityIQ -> fldY) * 128) < 8.0) {
					memcpy(friendIQ -> friendID, entityIR -> uuid, QDIV_B256);
					entityIQ -> type = SHEEP;
					QDIV_RANDOM(entityIQ -> uuid, QDIV_B256);
					break;
				}
				entitySL++;
			}
		}else{
			int32_t entitySL = 0;
			entity_t* entityIR;
			while(entitySL < settings.MAX_ENTITIES) {
				entityIR = entity + entitySL;
				if(entityTable[entitySL] && memcmp(friendIQ -> friendID, entityIR -> uuid, QDIV_B256) == 0) {
					if(entityInRange(entityIR, entityIQ) &&
					(fabs(entityIR -> posX - entityIQ -> posX) + (double)(abs(entityIR -> fldX - entityIQ -> fldX) * 128) > 16.0 ||
					fabs(entityIR -> posY - entityIQ -> posY) + (double)(abs(entityIR -> fldY - entityIQ -> fldY) * 128) > 16.0)) {
						double relX = entityIR -> posX - entityIQ -> posX + (double)(entityIR -> fldX - entityIQ -> fldX) * 128;
						double relY = entityIR -> posY - entityIQ -> posY + (double)(entityIR -> fldY - entityIQ -> fldY) * 128;
						if(relX > 16.0) {
							changeDirection(entityIQ, EAST);
						}else if(relX < -16.0) {
							changeDirection(entityIQ, WEST);
						}
						if(relY > 16.0) {
							changeDirection(entityIQ, NORTH);
						}else if(relY < -16.0) {
							changeDirection(entityIQ, SOUTH);
						}
						syncEntity(entityIQ);
						goto sheepNoAmbient;
					}
					break;
				}
				entitySL++;
			}
		}
	}
	if(rng % 400 < 8) {
		changeDirection(entityIQ, rng % 8);
		syncEntity(entityIQ);
	}
	sheepNoAmbient:
}

// Entity Action
void wispAction(void* entityVD) {
	entity_t* entityIQ = (entity_t*)entityVD;
	double boxIQ = entityType[entityIQ -> type].hitBox;
	int32_t entitySL = 0;
	entity_t* entityIR;
	while(entitySL < settings.MAX_ENTITIES) {
		entityIR = entity + entitySL;
		if(entityTable[entitySL] && entityIR -> type == PLAYER && entityInRange(entityIR, entityIQ) &&
		(fabs(entityIR -> posX - entityIQ -> posX) + (double)(abs(entityIR -> fldX - entityIQ -> fldX) * 128) < boxIQ &&
		fabs(entityIR -> posY - entityIQ -> posY) + (double)(abs(entityIR -> fldY - entityIQ -> fldY) * 128) < boxIQ)) {
			criterion_t criterionIQ = {ENCOUNTER_WISP, ++entityIR -> unique.Player.criterion[ENCOUNTER_WISP]};
			send_encrypted(0x09, (void*)&criterionIQ, sizeof(criterion_t), entityIR -> unique.Player.client);
			QDIV_DELETE_ENTITY(entityIQ -> slot);
			syncEntityRemoval(entityIQ -> slot);
			break;
		}
		entitySL++;
	}
}

// Entity Action
void blastAction(void* entityVD) {
	entity_t* entityIQ = (entity_t*)entityVD;
	double boxIQ = entityType[entityIQ -> type].hitBox;
	int32_t entitySL = 0;
	entity_t* entityIR;
	while(entitySL < settings.MAX_ENTITIES) {
		entityIR = entity + entitySL;
		if(entityTable[entitySL] && entityType[entityIR -> type].maxHealth > 0 && entityInRange(entityIR, entityIQ) &&
		isEntityPresentInRange(entityIR, entityIQ -> fldX, entityIQ -> fldY, entityIQ -> posX, entityIQ -> posY, boxIQ)) {
			damageEntity(entityIR, 4);
			QDIV_DELETE_ENTITY(entityIQ -> slot);
			syncEntityRemoval(entityIQ -> slot);
			goto blastCollision;
		}
		entitySL++;
	}
	entityIQ -> despawnTM += qFactor;
	if(entityIQ -> despawnTM > entityType[entityIQ -> type].despawnTM || entityIQ -> motX == 0.0 || entityIQ -> motY == 0.0) {
		QDIV_DELETE_ENTITY(entityIQ -> slot);
		syncEntityRemoval(entityIQ -> slot);
	}
	blastCollision:
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
						fread(&fieldIQ, 1, sizeof(field_t), fileData);
						fclose(fileData);
					}
					fieldIQ.active = 1;
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
				entityIQ -> despawnTM = 0;
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
			entityIQ.qEnergy = 1;
			entityIQ.type = inType;
			switch(inType) {
				case PLAYER:
					player_t* playerIQ = &entityIQ.unique.Player;
					playerIQ -> role = BUILDER;
					playerIQ -> roleTM = 0;
					playerIQ -> artifact = OLD_SWINGER;
					playerIQ -> useTM = 0;
					playerIQ -> portalTM = 0;
					playerIQ -> inMenu = false;
					playerIQ -> qEnergyMax = 1;
					break;
				case KOBATINE_BUG:
					ncHostile_t* hostileIQ = &entityIQ.unique.ncHostile;
					hostileIQ -> target = NULL;
					hostileIQ -> searchTM = 0;
					hostileIQ -> attackTM = 0;
					break;
				case SHEEP:
				case WILD_SHEEP:
					friend_t* friendIQ = &entityIQ.unique.Friend;
					memset(friendIQ -> friendID, 0x00, QDIV_B256);
					friendIQ -> searchTM = 0;
					
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
			entityIQ.despawnTM = 0;
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
							int32_t entitySL = spawnEntity(clientIQ -> name, clientIQ -> uuid, PLAYER, OVERWORLD, 0, 0, 64, 64);
							if(entitySL == -1) {
								clientIQ -> connected = false;
				        		printf("> %s rejected: Entity Table filled\n", clientIQ -> name);
								break;
							}
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
							if(!isArtifactUnlocked(playerIQ -> role, playerIQ -> artifact, playerIQ)) playerIQ -> artifact = 0;
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
							if(strcmp("admin", clientIQ -> name) == 0) {
								adminEntity = entityIQ;
							}
						}else{
					        switch(packetIQ.payload[0]) {
					        	case 0x01:
					        		int32_t entitySL = clientIQ -> entity -> slot;
					        		removePlayer(entitySL);
									syncEntityRemoval(entitySL);
					        		clientIQ -> entity = NULL;
									clientIQ -> connected = false;
									printf("> %s disconnected: Remote\n", clientIQ -> name);
									if(strcmp("admin", clientIQ -> name) == 0) {
										adminEntity = NULL;
									}
					        	case 0x02:
					        		playerIQ -> inMenu = false;
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
					        		struct AES_ctx AES_IQ = clientIQ -> AES_CTX; 
					        		AES_ctx_set_iv(&AES_IQ, encSegIQ.Iv);
									AES_CBC_encrypt_buffer(&AES_IQ, encSegIQ.payload, 32768);
									struct sockaddr_storage addrIQ = clientIQ -> addr;
									sendto(*sockSF, (char*)&encSegIQ, sizeof(encryptable_seg), 0, (struct sockaddr*)&addrIQ, clientIQ -> addrSZ);
									break;
					        	case 0x08:
					        		if(playerIQ -> inMenu) {
							    		int32_t roleIQ, artifactSL;
							    		memcpy(&roleIQ, packetIQ.payload + 1, sizeof(int32_t));
							    		memcpy(&artifactSL, packetIQ.payload + 1 + sizeof(int32_t), sizeof(int32_t));
							    		if(isArtifactUnlocked(roleIQ, artifactSL, playerIQ) || settings.UNLOCK_ALL) {
							    			if(playerIQ -> role != roleIQ) {
							    				playerIQ -> roleTM = 3600.0;
							    				playerIQ -> role = roleIQ;
							    			}
							    			playerIQ -> artifact = artifactSL;
							    			send_encrypted(0x04, (void*)entityIQ, sizeof(entity_t), clientIQ);
							    		}
							    	}
					        		break;
					        	case 0x0A:
					        		playerIQ -> inMenu = false;
						    		usage_t usageIQ;
						    		memcpy(&usageIQ, packetIQ.payload + 1, sizeof(usage_t));
						    		artifact_st* artifactIQ = &artifact[playerIQ -> role][playerIQ -> artifact];
						    		if(usageIQ.usage == BLOCK_USAGE ||
						    		(usageIQ.usage == PRIMARY_USAGE && fabs(usageIQ.useRelX) < artifactIQ -> primary.range && fabs(usageIQ.useRelY) < artifactIQ -> primary.range) ||
						    		(usageIQ.usage == SECONDARY_USAGE && fabs(usageIQ.useRelX) < artifactIQ -> secondary.range && fabs(usageIQ.useRelY) < artifactIQ -> secondary.range)) {
										playerIQ -> currentUsage = usageIQ.usage;
										playerIQ -> useRelX = usageIQ.useRelX;
										playerIQ -> useRelY = usageIQ.useRelY;
									}else{
										playerIQ -> currentUsage = NO_USAGE;
									}
									syncEntity(entityIQ);
							    	break;
							    case 0x0E:
							    	entityIQ -> motX = 0;
							    	entityIQ -> motY = 0;
							    	playerIQ -> currentUsage = NO_USAGE;
							    	playerIQ -> inMenu = true;
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
            removePlayer(client[clientSL].entity -> slot);
            printf("> %s disconnected: Self\n", client[clientSL].name);
        }
    }
    QDIV_CLOSE(*sockSF);
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
				if(field[repClk].active && field[repClk].zone == OVERWORLD && field[repClk].fldX > -5 && field[repClk].fldX < 5 && field[repClk].fldY > -5 && field[repClk].fldY < 5) {
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
			if(adminEntity != NULL) printf("X: %f, Y: %f, FX: %d, FY: %d\n", adminEntity -> posX, adminEntity -> posY, adminEntity -> fldX, adminEntity -> fldY);
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
		
		if(adminEntity != NULL) {
			if(strcmp(prompt, "roach") == 0) {
				spawnEntity("Entity", uuidNull, CALCIUM_CRAWLER, adminEntity -> zone, adminEntity -> fldX, adminEntity -> fldY, adminEntity -> posX, adminEntity -> posY);
			}
			if(strcmp(prompt, "erase") == 0) {
				memset(adminEntity -> local[1][1] -> block, 0x00, sizeof(adminEntity -> local[1][1] -> block));
			}
			if(strcmp(prompt, "changeBlock") == 0) {
				printf("[Enter requested Block]: ");
				scanf("%5u", adminBlockPT);
				printf("[Enter requested Layer]: ");
				scanf("%1d", adminLayerPT);
			}
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
	settings.SPAWN_RATE = 1.0;
	settings.UNLOCK_ALL = false;
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
		QSMencode(&resultIQ, (void*)&settings.SPAWN_RATE, bufferIQ, bufferSZ, NULL, "SpawnRate", 9);
		if(resultIQ != QSM_SIGNED_INT) goto abort;
		QSMencode(&resultIQ, (void*)&settings.WORLD_SEED, bufferIQ, bufferSZ, NULL, "WorldSeed", 9);
		if(resultIQ != QSM_SIGNED_INT) goto abort;
		QSMencode(&resultIQ, (void*)&settings.UNLOCK_ALL, bufferIQ, bufferSZ, NULL, "UnlockAll", 9);
		if(resultIQ == QSM_NOVALUE) settings.UNLOCK_ALL = true;
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
	entityType[PLAYER].speed = 2.0 * settings.PLAYER_SPEED;
	puts("> Starting Threads");
	pthread_t console_id, gate_id;
	pthread_create(&gate_id, NULL, thread_gate, NULL);
	pthread_create(&console_id, NULL, thread_console, NULL);
	puts("> Initializing OpenSimplex");
	initGenerator(settings.WORLD_SEED);
	srand(time(NULL));
	puts("> Spawning Entities");
	uint8_t persistentFL[QDIV_MAX_ENTITIES][QDIV_B256];
	FILE* fileIQ = fopen("entity/persistent.dat", "rb");
	if(fileIQ != NULL) {
		fread(persistentFL, 1, sizeof(persistentFL), fileIQ);
		fclose(fileIQ);
		int32_t uuidSL = 0;
		while(uuidSL < QDIV_MAX_ENTITIES) {
			if(memcmp(persistentFL[uuidSL], uuidNull, 16) != 0) {
				int32_t entitySL = spawnEntity("Entity", persistentFL[uuidSL], NULL_ENTITY, 0, 0, 0, 0, 0);
				if(entitySL != -1) entity[entitySL].active = 0;
			}
			uuidSL++;
		}
	}
	puts("> Setting up Tick Stamper");
	struct timeval qTickStart;
	struct timeval qTickEnd;
	double blockUpdateTM = 0;
	double backupTM = 0;
	while(qShutdown != 1) {
		gettimeofday(&qTickEnd, NULL);
		if(qTickStart.tv_usec > qTickEnd.tv_usec) qTickEnd.tv_usec += 1000000;
		qTickTime = qTickEnd.tv_usec - qTickStart.tv_usec;
		qFactor = qTickTime < 20000 ? 0.02 : (double)qTickTime * 0.000001;
		serverTime = time(NULL);
		if(localtime(&serverTime) -> tm_hour != currentHour) currentHour = localtime(&serverTime) -> tm_hour;
		if(qTickTime < 20000) usleep(20000 - qTickTime);
		gettimeofday(&qTickStart, NULL);
		if(TickQuery) printf("> Tick: %d\n", qTickStart.tv_usec);
		for(int32_t entitySL = 0; entitySL < QDIV_MAX_ENTITIES; entitySL++) {
			if(entityTable[entitySL]) {
				entity_t* entityIQ = entity + entitySL;
				if(entityIQ -> active) {
					client_t* clientIQ;
					if(entityIQ -> type == PLAYER) clientIQ = entityIQ -> unique.Player.client;
					bool motUpdate = false;
					
					// Move on X axis
					entityIQ -> posX += entityIQ -> motX * qFactor * (1 + QDIV_SUBDIAG * (entityIQ -> motY != 0));
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
						if(entityIQ -> local[1][1] == NULL) {
							entityIQ -> active = 0;
							continue;
						}
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
						if(entityIQ -> local[1][1] == NULL) {
							entityIQ -> active = 0;
							continue;
						}
						syncEntity(entityIQ);
						if(entityIQ -> type == PLAYER) {
							syncEntityField(entityIQ -> local[0][0], clientIQ);
							syncEntityField(entityIQ -> local[0][1], clientIQ);
							syncEntityField(entityIQ -> local[0][2], clientIQ);
						}
					}
					if(entityIQ -> motX > 0 && isEntityObstructed(entityIQ, EAST)) {
						while(isEntityObstructed(entityIQ, EAST)) {
							entityIQ -> posX -= 0.01;
						}
						entityIQ -> motX = 0;
						motUpdate = true;
					}
					if(entityIQ -> motX < 0 && isEntityObstructed(entityIQ, WEST)) {
						while(isEntityObstructed(entityIQ, WEST)) {
							entityIQ -> posX += 0.01;
						}
						entityIQ -> motX = 0;
						motUpdate = true;
					}
					
					// Move on Y axis
					entityIQ -> posY += entityIQ -> motY * qFactor * (1 + QDIV_SUBDIAG * (entityIQ -> motX != 0));
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
						if(entityIQ -> local[1][1] == NULL) {
							entityIQ -> active = 0;
							continue;
						}
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
						if(entityIQ -> local[1][1] == NULL) {
							entityIQ -> active = 0;
							continue;
						}
						syncEntity(entityIQ);
						if(entityIQ -> type == PLAYER) {
							syncEntityField(entityIQ -> local[0][0], clientIQ);
							syncEntityField(entityIQ -> local[1][0], clientIQ);
							syncEntityField(entityIQ -> local[2][0], clientIQ);
						}
					}
					if(entityIQ -> motY > 0 && isEntityObstructed(entityIQ, NORTH)) {
						while(isEntityObstructed(entityIQ, NORTH)) {
							entityIQ -> posY -= 0.01;
						}
						entityIQ -> motY = 0;
						motUpdate = true;
					}
					if(entityIQ -> motY < 0 && isEntityObstructed(entityIQ, SOUTH)) {
						while(isEntityObstructed(entityIQ, SOUTH)) {
							entityIQ -> posY += 0.01;
						}
						entityIQ -> motY = 0;
						motUpdate = true;
					}
					if(motUpdate) {
						syncEntity(entityIQ);
						motUpdate = false;
					}
					
					// Execute Block Stepon Functions
					if(entityIQ -> local[1][1] != NULL) {
						int32_t blockX = (int32_t)entityIQ -> posX;
						int32_t blockY = (int32_t)entityIQ -> posY;
						block_st* wallSO = &block[1][entityIQ -> local[1][1] -> block[blockX][blockY][1]];
						block_st* floorSO = &block[0][entityIQ -> local[1][1] -> block[blockX][blockY][0]];
						if(wallSO -> stepOn != NULL) {
							(*wallSO -> stepOn)(entityIQ -> local[1][1], blockX, blockY, 1, (void*)entityIQ, (void*)wallSO);
						}else if(floorSO -> stepOn != NULL) {
							(*floorSO -> stepOn)(entityIQ -> local[1][1], blockX, blockY, 0, (void*)entityIQ, (void*)floorSO);
						}
					}
					
					// Execute Entity Actions
					if(entityType[entityIQ -> type].action != NULL) (*entityType[entityIQ -> type].action)((void*)entityIQ);
					
					// Regenerate Health
					if(entityIQ -> healthTM < 60.0) {
						entityIQ -> healthTM += qFactor;
					}else if(entityIQ -> health < entityType[entityIQ -> type].maxHealth) {
						entityIQ -> healthTM = 0.0;
						entityIQ -> health++;
						syncEntity(entityIQ);
					}
				}else if(!entityType[entityIQ -> type].persistent) {
					// Attempt Despawn
					entityIQ -> despawnTM += qFactor;
					if(entityIQ -> despawnTM > entityType[entityIQ -> type].despawnTM) {
						QDIV_DELETE_ENTITY(entityIQ -> slot);
					}
				}
			}
			nextEntity:
		}
		blockUpdateTM += qFactor;
		if(blockUpdateTM > 4.0) {
			blockUpdateTM = 0.0;
			int32_t fieldSL = 0;
			field_t* fieldIQ;
			while(fieldSL < settings.MAX_FIELDS) {
				fieldIQ = field + fieldSL;
				if(fieldIQ -> active == 1) {
					int32_t posX = 0;
					int32_t posY = 0;
					int32_t layer = 0;
					block_st* blockIQ;
					while(layer < 2) {
						blockIQ = &block[layer][fieldIQ -> block[posX][posY][layer]];
						switch(blockIQ -> type) {
							case TIME_CHECK:
								if(fieldIQ -> unique[posX][posY][layer].successionTM < serverTime) {
									setBlock(blockIQ -> unique.timeCheck.successor, fieldIQ, posX, posY, layer);
								}
						}
						posX++;
						if(posX == 128) {
							posX = 0;
							posY++;
							if(posY == 128) {
								posY = 0;
								layer++;
							}
						}
					}
				}
				fieldSL++;
			}
		}
		backupTM += qFactor;
		if(backupTM > 300.0) {
			backupTM = 0.0;
			int32_t backupSL = 0;
			int32_t fieldCTR = 0;
			while(backupSL < settings.MAX_FIELDS) {
				if(field[backupSL].active && field[backupSL].edit) {
					saveField(field + backupSL);
					fieldCTR++;
				}
				backupSL++;
			}
			backupSL = 0;
			memset(persistentFL, 0x00, sizeof(persistentFL));
			int32_t entityCTR = 0;
			entity_t* entityIQ;
			while(backupSL < settings.MAX_ENTITIES) {
				entityIQ = entity + backupSL;
				if(entityTable[backupSL] && entityType[entityIQ -> type].persistent) {
					saveEntity(entityIQ);
					if(entityIQ -> type == PLAYER) {
						saveCriteria(entityIQ);
					}else{
						memcpy(persistentFL[backupSL], entity[backupSL].uuid, QDIV_B256);
					}
					entityCTR++;
				}
				backupSL++;
			}
			fileIQ = fopen("entity/persistent.dat", "wb");
			fwrite(persistentFL, 1, sizeof(persistentFL), fileIQ);
			fclose(fileIQ);
			if(fieldCTR > 0 || entityCTR > 0) printf("> Autosaved: %d Fields, %d Entities\n", fieldCTR, entityCTR);
		}
	}
	puts("> Stopping Threads");
	pthread_join(gate_id, NULL);
	pthread_join(console_id, NULL);
	puts("> Saving Entities");
	memset(persistentFL, 0x00, sizeof(persistentFL));
	int32_t entitySL = 0;
	while(entitySL < QDIV_MAX_ENTITIES) {
		if(entityTable[entitySL] && entityType[entity[entitySL].type].persistent) {
			memcpy(persistentFL[entitySL], entity[entitySL].uuid, QDIV_B256);
			saveEntity(entity + entitySL);
		}
		entitySL++;
	}
	fileIQ = fopen("entity/persistent.dat", "wb");
	fwrite(persistentFL, 1, sizeof(persistentFL), fileIQ);
	fclose(fileIQ);
	puts("> Freeing up Memory");
	free(client);
	free(field);
	abort:
	free(bufferIQ);
}
