#include "qDivEnum.h"
#include "qDivType.h"
#include<math.h>
#include<string.h>
#include "include/open-simplex-noise.h"

int32_t currentSeed;

// GLOBAL
struct osn_context* OSN_cavernJoint;

// OVERWORLD
struct osn_context* OSN_oceanBiome;
struct osn_context* OSN_warmBiome;
struct osn_context* OSN_alternativeBiome;
struct osn_context* OSN_humidBiome;
struct osn_context* OSN_largeRiver;
struct osn_context* OSN_smallRiver;
struct osn_context* OSN_patching;
struct osn_context* OSN_crunch;

// CAVERN
struct osn_context* OSN_caveA;
struct osn_context* OSN_caveB;
struct osn_context* OSN_caveC;
struct osn_context* OSN_caveD;
struct osn_context* OSN_caveE;
struct osn_context* OSN_caveF;
struct osn_context* OSN_caveG;
struct osn_context* OSN_caveH;
struct osn_context* OSN_bronze;
struct osn_context* OSN_aluminium;
struct osn_context* OSN_iron;

void initGenerator(int32_t seed) {
	currentSeed = seed;
	srand(seed);
	open_simplex_noise(rand(), &OSN_cavernJoint);
	open_simplex_noise(rand(), &OSN_oceanBiome);
	open_simplex_noise(rand(), &OSN_warmBiome);
	open_simplex_noise(rand(), &OSN_alternativeBiome);
	open_simplex_noise(rand(), &OSN_humidBiome);
	open_simplex_noise(rand(), &OSN_largeRiver);
	open_simplex_noise(rand(), &OSN_smallRiver);
	open_simplex_noise(rand(), &OSN_patching);
	open_simplex_noise(rand(), &OSN_crunch);
	open_simplex_noise(rand(), &OSN_caveA);
	open_simplex_noise(rand(), &OSN_caveB);
	open_simplex_noise(rand(), &OSN_caveC);
	open_simplex_noise(rand(), &OSN_caveD);
	open_simplex_noise(rand(), &OSN_caveE);
	open_simplex_noise(rand(), &OSN_caveF);
	open_simplex_noise(rand(), &OSN_caveG);
	open_simplex_noise(rand(), &OSN_caveH);
	open_simplex_noise(rand(), &OSN_bronze);
	open_simplex_noise(rand(), &OSN_aluminium);
	open_simplex_noise(rand(), &OSN_iron);
}

#define QDIV_NOISE(ctx, factor) open_simplex_noise2(ctx, (double)(fieldIQ -> fldX * 128 + posX + currentSeed) * factor, (double)(fieldIQ -> fldY * 128 + posY - currentSeed) * factor)

void useGenerator(field_t* fieldIQ) {
	memset(fieldIQ -> biome, 0x00, sizeof(fieldIQ -> biome));
	memset(fieldIQ -> block, 0x00, sizeof(fieldIQ -> block));
	int32_t posX = 0;
	int32_t posY = 0;
	int32_t dist;
	uint16_t biomeIQ, floorIQ, wallIQ;
	while(posY < 128) {
		double cavernJoint = QDIV_NOISE(OSN_cavernJoint, 1.0);
		double oceanBiome = (((QDIV_NOISE(OSN_oceanBiome, 0.0005) + QDIV_NOISE(OSN_oceanBiome, 0.001)) * 0.875) + (QDIV_NOISE(OSN_oceanBiome, 0.003125) + QDIV_NOISE(OSN_oceanBiome, 0.00625) + QDIV_NOISE(OSN_oceanBiome, 0.0125) + QDIV_NOISE(OSN_oceanBiome, 0.025)) * 0.125) * 0.5;
		double warmBiome = QDIV_NOISE(OSN_warmBiome, 0.0006);
		double alternativeBiome = QDIV_NOISE(OSN_alternativeBiome, 0.0013);
		double humidBiome = QDIV_NOISE(OSN_humidBiome, 0.0020);
		double patching = QDIV_NOISE(OSN_patching, 0.1);
		double crunch = QDIV_NOISE(OSN_crunch, 1.0);
		switch(fieldIQ -> zone) {
			case OVERWORLD:
				double largeRiver = QDIV_NOISE(OSN_largeRiver, 0.010);
				double smallRiver = QDIV_NOISE(OSN_smallRiver, 0.022);
				floorIQ = WATER;
				wallIQ = NO_WALL;
				if(oceanBiome < -0.1) {
					if(humidBiome < -0.4) {
						if(warmBiome > -0.3 && warmBiome < 0.3) {
							biomeIQ = DESERT;
							if(largeRiver < 0.9) {
								floorIQ = SAND;
								if((patching > 0.5 && crunch > 0.5) || (largeRiver > 0.8 && crunch > 0.75)) wallIQ = AGAVE;
							}
						}else if((warmBiome < -0.5 || warmBiome > 0.5) && oceanBiome < -0.18) {
							biomeIQ = ARIDIS;
							if(largeRiver < -0.08 || largeRiver > 0.08) {
								floorIQ = ARIDIS_FLATGRASS;
								if(crunch > 0.5) {
									wallIQ = ARIDIS_BUSH;
								}else if(crunch > 0.25) {
									wallIQ = ARIDIS_GRASS;
								}
							}
						}else goto notDry;
					}else{
						notDry:
						if((warmBiome < -0.4 || warmBiome > 0.4) && (oceanBiome > -0.18 || (alternativeBiome > 0.8 && oceanBiome > -0.8))) {
							biomeIQ = REDWOOD_FOREST;
							floorIQ = REDWOOD_FLATGRASS;
							if(cavernJoint > 0.864) {
								wallIQ = CAVERN_PORTAL;
							}else if(patching > -0.25) {
								if(crunch > 0.8) {
									wallIQ = REDWOOD_LOG;
								}else if(crunch > 0.5 && oceanBiome < -0.11) {
									wallIQ = REDWOOD_TREE;
								}else if(crunch > 0.25) {
									wallIQ = REDWOOD_WEEDS;
								}
							}else{
								if(crunch > 0.25) {
									wallIQ = REDWOOD_WEEDS;
								}
							}
						}else{
							if((humidBiome < -0.39 && ((warmBiome > -0.31 && warmBiome < 0.31) || ((warmBiome < -0.51 || warmBiome > 0.51) && oceanBiome < -0.17))) // Dry Border
							|| ((warmBiome < -0.39 || warmBiome > 0.39) && (oceanBiome > -0.19 || (alternativeBiome > 0.79 && oceanBiome > -0.81)))) { // Redwood Border
								biomeIQ = BORDER_SHALLAND;
							}else{
								biomeIQ = SHALLAND;
							}
							if(biomeIQ == BORDER_SHALLAND || smallRiver < -0.08 || smallRiver > 0.08) {
								floorIQ = SHALLAND_FLATGRASS;
								if(patching > 0.25) {
									if(crunch > 0.75) {
										wallIQ = SHALLAND_BUSH;
									}else if(crunch > 0.25) {
										wallIQ = SHALLAND_GRASS;
									}
								}
							}
						}
					}
				}else{
					biomeIQ = OCEAN;
				}
				break;
			case CAVERN:
				double cave = (QDIV_NOISE(OSN_caveA, 0.0005) + QDIV_NOISE(OSN_caveB, 0.001) + QDIV_NOISE(OSN_caveC, 0.005) + QDIV_NOISE(OSN_caveD, 0.01) + QDIV_NOISE(OSN_caveE, 0.015) + QDIV_NOISE(OSN_caveF, 0.04)) * 0.166;
				double bronze = QDIV_NOISE(OSN_bronze, 0.05);
				double aluminium = QDIV_NOISE(OSN_aluminium, 0.025);
				double iron = QDIV_NOISE(OSN_iron, 0.05);
				biomeIQ = SEDIMENTARY_CAVES;
				floorIQ = LIMESTONE_FLOOR;
				if(cave > 0.025 || cave < -0.025) {
					wallIQ = LIMESTONE_WALL;
				}else if(patching > 0.25 && crunch > 0.5) {
					wallIQ = STALAGMITE;
				}else{
					wallIQ = NO_WALL;
				}
				if(oceanBiome < -0.1 && humidBiome >= -0.4 && (warmBiome < -0.4 || warmBiome > 0.4) && (oceanBiome > -0.18 || (alternativeBiome > 0.8 && oceanBiome > -0.8)) && cavernJoint > 0.864) {
					wallIQ = OVERWORLD_PORTAL;
				}else if(aluminium > 0.6 && crunch > 0.0) {
					wallIQ = ALUMINIUM_CLUMP;
				}else if(bronze > 0.65 && crunch > 0.25) {
					wallIQ = BRONZE_CLUMP;
				}else if(iron > 0.7 && crunch > 0.25) {
					wallIQ = IRON_CLUMP;
				}
				break;
		}
		fieldIQ -> biome[posX][posY] = biomeIQ;
		fieldIQ -> block[posX][posY][0] = floorIQ;
		fieldIQ -> block[posX][posY][1] = wallIQ;
		posX++;
		if(posX == 128) {
			posX = 0;
			posY++;
		}
	}
}
