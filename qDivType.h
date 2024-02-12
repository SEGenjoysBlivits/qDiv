#include<stdint.h>
#include<stddef.h>
#include<stdbool.h>
#define QDIV_ARTIFACT_CRITERIA 4
#define QDIV_PACKET_SIZE 512

typedef int32_t int24_t;

typedef struct {
	int32_t iteration;
	int32_t frequency;
	int32_t stability;
	int32_t occasion;
} qrng_t;

typedef struct {
	uint8_t Iv[16];
	uint8_t payload[QDIV_PACKET_SIZE];
} encryptable_t;

typedef struct {
	uint8_t Iv[16];
	uint8_t payload[32768];
} encryptable_seg;

typedef struct {
	size_t length;
	int32_t elements[];
} collection_st;

typedef union {
	uint64_t successionTM;
} block_ust;

typedef uint16_t block_gt[128][128][2];

typedef struct {
	int32_t slot;
	int32_t active;
	int32_t zone;
	int32_t fldX;
	int32_t fldY;
	bool edit;
	uint16_t biome[128][128];
	uint16_t block[128][128][2];
	block_ust unique[128][128][2];
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
	int32_t destination;
	int8_t hoverText[32];
} portal_st;

typedef struct {
	int32_t key;
	int8_t hoverText[32];
	collection_st* lootTable;
	int32_t lootAmount;
} lootbox_st;

typedef struct {
	uint64_t duration;
	uint16_t successor;
} timeCheck_st;

typedef void(*blockAction)(field_t*, int32_t, int32_t, int32_t, void*, void*);
#define def_BlockAction(name) void name(field_t* fieldIQ, int32_t posX, int32_t posY, int32_t layer, void* entityVD, void* blockVD)

typedef struct {
	int32_t range;
	float red;
	float green;
	float blue;
} light_ctx;

typedef struct {
	double friction;
	bool transparent;
	float texX;
	float texY;
	float sizeX;
	float sizeY;
	const uint8_t* mine_sound;
	size_t mine_soundSZ;
	light_ctx light;
	uint64_t qEnergy;
	collection_st* placeable;
	int32_t type;
	union {
		uint16_t litSelf;
		uint16_t decomposite;
		portal_st portal;
		lootbox_st lootbox;
		timeCheck_st timeCheck;
	} unique;
	blockAction usage;
	blockAction stepOn;
	int32_t mine_criterion;
} block_st;

typedef struct {
	float red;
	float green;
	float blue;
	float alpha;
} color_t;

typedef struct {
	int8_t desc[32];
	color_t textColor;
} criterion_st;

typedef struct {
	int8_t name[16];
	color_t textColor;
	int32_t maxArtifact;
	double movement_speed;
	double mining_factor;
} role_st;

typedef void(*entityAction)(void*);

typedef struct {
	int32_t maxHealth;
	bool persistent;
	double hitBox;
	bool noClip;
	double speed;
	double despawnTM;
	uint64_t qEnergy;
	entityAction action;
	int32_t priority;
	int32_t kill_criterion;
	uint32_t texture[8];
} entity_st;

typedef struct {
	void* captain;
} vehicle_t;

typedef struct {
	void* target;
	double searchTM;
	double attackTM;
} ncHostile_t;

typedef struct {
	struct client_s* client;
	int32_t role;
	double roleTM;
	int32_t artifact;
	int32_t currentUsage;
	double useTM;
	double useRelX;
	double useRelY;
	uint32_t* criterion;
	double spawnTM;
	double portalTM;
	bool inMenu;
	uint64_t qEnergyMax;
	void* vehicle;
} player_t;

typedef struct {
	uint8_t friendID[16];
	double searchTM;
} friend_t;

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
	double healthTM;
	double despawnTM;
	float mood;
	uint64_t qEnergy;
	union {
		player_t Player;
		ncHostile_t ncHostile;
		friend_t Friend;
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

typedef void(*artifactAction)(field_t*, double, double, entity_t*, player_t*, void*, void*);
#define def_ArtifactAction(name) void name(field_t* fieldIQ, double posX, double posY, entity_t* entityIQ, player_t* playerIQ, void* artifactVD, void* settingsVD)

typedef struct {
	int32_t Template;
	uint32_t value;
} criterion_t;

typedef struct {
	artifactAction action;
	uint64_t cost;
	double useTM;
	union {
		placeBlock_st place;
		sliceEntity_st slice;
		uint16_t lootBox;
	} unique;
	bool cancelable;
	double range;
} usageHalf_st;

typedef struct {
	int8_t name[32];
	int8_t desc[256];
	bool crossCriterial;
	uint32_t texture;
	usageHalf_st primary;
	usageHalf_st secondary;
	uint64_t qEnergy;
	criterion_t criterion[QDIV_ARTIFACT_CRITERIA];
} artifact_st;

typedef struct {
	uint16_t block;
	int32_t zone;
	int32_t fldX;
	int32_t fldY;
	int32_t posX;
	int32_t posY;
	int32_t layer;
} block_l;

typedef struct {
	int32_t usage;
	double useRelX;
	double useRelY;
} usage_t;
