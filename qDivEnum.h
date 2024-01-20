enum connectionEnum {
	OFFLINE_NET,
	WAITING_NET,
	CONNECTED_NET
};

enum zoneEnum {
	OVERWORLD,
	CAVERN
};

enum biomeEnum {
	OCEAN,
	SHALLAND,
	BORDER_SHALLAND,
	ARIDIS,
	DESERT,
	REDWOOD_FOREST,
	MAX_BIOME
};

enum blockTypeEnum {
	NO_TYPE_ST,
	FERTILE,
	DECOMPOSING,
	ZONE_PORTAL,
	
	NO_TYPE_L,
	CROP
};

enum floorEnum {
	NO_FLOOR,
	SHALLAND_FLOOR,
	SHALLAND_FLATGRASS,
	WATER,
	SAND,
	ARIDIS_FLATGRASS,
	ARIDIS_FLOOR,
	REDWOOD_FLATGRASS,
	REDWOOD_FLOOR,
	SOIL
};

enum wallEnum {
	NO_WALL,
	SHALLAND_WALL,
	LAMP,
	SHALLAND_GRASS,
	SHALLAND_BUSH,
	ARIDIS_GRASS,
	ARIDIS_BUSH,
	ARIDIS_WALL,
	AGAVE,
	REDWOOD_WEEDS,
	REDWOOD_TREE,
	REDWOOD_LOG,
	REDWOOD_WALL,
	IMMINENT_POTATO,
	POTATO,
	OVERWORLD_PORTAL,
	CAVERN_PORTAL
};

enum entityTypeEnum {
	NULL_ENTITY,
	PLAYER,
	SHALLAND_SNAIL,
	MAX_ENTITY_TYPE
};

enum directionEnum {
	NORTH,
	EAST,
	SOUTH,
	WEST,
	NORTH_STOP,
	EAST_STOP,
	SOUTH_STOP,
	WEST_STOP
};

enum criterionEnum {
	NO_CRITERION = -1,
	MINE_SHALLAND_BUSH,
	MINE_ARIDIS_BUSH,
	MINE_REDWOOD_TREE,
	FERTILIZE_LAND,
	MINE_SHALLAND_GRASS,
	MAX_CRITERION
};

enum warriorEnum {
	OLD_SLICER,
	MAX_WARRIOR_ARTIFACT
};
enum explorerEnum {
	MAX_EXPLORER_ARTIFACT
};
enum builderEnum {
	OLD_SWINGER,
	SHALLAND_FLOOR_ARTIFACT,
	SHALLAND_WALL_ARTIFACT,
	LAMP_ARTIFACT,
	ARIDIS_FLOOR_ARTIFACT,
	ARIDIS_WALL_ARTIFACT,
	REDWOOD_FLOOR_ARTIFACT,
	REDWOOD_WALL_ARTIFACT,
	MAX_BUILDER_ARTIFACT
};
enum gardenerEnum {
	SIMPLE_HOE,
	POTATO_ARTIFACT,
	MAX_GARDENER_ARTIFACT
};
enum engineerEnum {
	MAX_ENGINEER_ARTIFACT
};
enum wizardEnum {
	MAX_WIZARD_ARTIFACT
};

enum roleEnum {
	WARRIOR,
	EXPLORER,
	BUILDER,
	GARDENER,
	ENGINEER,
	WIZARD
};

enum usageEnum {
	NO_USAGE,
	PRIMARY_USAGE,
	SECONDARY_USAGE
};