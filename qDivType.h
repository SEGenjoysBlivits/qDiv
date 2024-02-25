#include<stdint.h>
#include<stdbool.h>
#include<stddef.h>
#define QDIV_ARTIFACT_CRITERIA 4
#define QDIV_PACKET_SIZE 160

typedef uint16_t qint_t;

typedef const struct translate_s {
	char* value;
	size_t size;
} translate_t;

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
	uint8_t payload[1024];
} encryptable_seg;

typedef struct {
	size_t length;
	int32_t elements[];
} collection_st;

typedef union {
	uint64_t successionTM;
} block_ust;

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
	float movement_speed;
	float mining_factor;
} role_st;

typedef struct astar_node_s {
	struct astar_node_s* parent;
	int32_t state;
	int32_t Gcost;
	int32_t Hcost;
	int32_t Fcost;
	int32_t posX;
	int32_t posY;
} astar_node_t;

typedef struct {
	int32_t fldX;
	int32_t fldY;
	int32_t posX;
	int32_t posY;
	int32_t dirX;
	int32_t dirY;
} astar_step_t;

typedef struct {
	astar_step_t* step;
	int32_t current;
} astar_path_t;

typedef struct {
	void* captain;
} vehicle_t;

typedef struct {
	void* target;
	float searchTM;
	astar_path_t path;
} ncHostile_t;

typedef struct {
	qint_t role;
	qint_t artifact;
	qint_t currentUsage;
	float roleTM;
	float useTM;
	float useRelX;
	float useRelY;
	float spawnTM;
	float portalTM;
	uint64_t qEnergyMax;
	
	// End of TRUNCATED_ENTITY
	bool inMenu;
	uint32_t* criterion;
	struct client_s* client;
} player_t;

typedef struct {
	uint8_t friendID[16];
	float searchTM;
} friend_t;

typedef struct {
	uint64_t decay;
	int32_t pierce;
	struct entity_s* anchor;
} projectile_t;

typedef struct {
	int32_t entitySL;
	float remainTM;
	qint_t subType;
} effect_t;

typedef struct entity_s {
	int8_t name[16];
	uint8_t uuid[16];
	int32_t slot;
	bool active;
	qint_t zone;
	int32_t fldX;
	int32_t fldY;
	float posX;
	float posY;
	float motX;
	float motY;
	int32_t health;
	float healthTM;
	float despawnTM;
	uint64_t qEnergy;
	qint_t subType;
	union {
		player_t Player;
		ncHostile_t ncHostile;
		friend_t Friend;
		projectile_t Projectile;
	} sub;
	field_t* local[3][3];
	float effectTM[QDIV_MAX_EFFECT];
} entity_t;

typedef void(*entity_fp)(entity_t*);
typedef void(*effect_fp)(entity_t*, float);
#define def_EffectAction(name) void name(entity_t* entityIQ, float remainTM)

typedef struct {
	int8_t name[32];
	uint32_t texture;
	float potency;
	bool visible;
	effect_fp action;
	entity_fp init;
} effect_st;

typedef struct {
	int32_t maxHealth;
	uint64_t qEnergy;
	uint64_t decay;
	bool persistent;
	float hitBox;
	bool noClip;
	bool proj;
	float speed;
	float despawnTM;
	int32_t priority;
	uint32_t texture[8];
	int32_t kill_criterion;
	entity_fp action;
	entity_fp collision;
} entity_st;

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

typedef void(*blockAction)(field_t*, int32_t, int32_t, int32_t, entity_t*, void*);
#define def_BlockAction(name) void name(field_t* fieldIQ, int32_t posX, int32_t posY, int32_t layer, entity_t* entityIQ, void* blockVD)

typedef struct {
	int32_t range;
	float red;
	float green;
	float blue;
} light_ctx;

typedef struct {
	float friction;
	bool transparent;
	float texX;
	float texY;
	float sizeX;
	float sizeY;
	qint_t direction;
	const uint8_t* mine_sound;
	size_t mine_soundSZ;
	light_ctx light;
	uint64_t qEnergy;
	collection_st* placeable;
	blockAction usage;
	blockAction stepOn;
	int32_t mine_criterion;
	qint_t subType;
	union {
		uint16_t litSelf;
		uint16_t decomposite;
		portal_st portal;
		lootbox_st lootbox;
		timeCheck_st timeCheck;
		uint64_t decay;
	} sub;
} block_st;

typedef struct {
	qint_t represent;
	int32_t layer;
} placeBlock_st;

typedef struct {
	uint64_t decay;
	int32_t range;
} sliceEntity_st;

typedef struct {
	qint_t represent;
	uint64_t decay;
	int32_t pierce;
} shootProj_st;

typedef void(*artifactAction)(field_t*, float, float, entity_t*, player_t*, void*);
#define def_ArtifactAction(name) void name(field_t* fieldIQ, float posX, float posY, entity_t* entityIQ, player_t* playerIQ, void* usageVD)

typedef struct {
	int32_t Template;
	uint32_t value;
} criterion_t;

typedef struct {
	uint64_t cost;
	float useTM;
	bool cancelable;
	float range;
	artifactAction action;
	union {
		placeBlock_st place;
		sliceEntity_st slice;
		uint16_t lootBox;
		shootProj_st shoot;
	} sub;
} usage_st;

typedef struct {
	int8_t name[32];
	int8_t desc[256];
	bool crossCriterial;
	uint32_t texture;
	usage_st primary;
	usage_st secondary;
	uint64_t qEnergy;
	criterion_t criterion[QDIV_ARTIFACT_CRITERIA];
} artifact_st;

typedef struct {
	qint_t block;
	int32_t zone;
	int32_t fldX;
	int32_t fldY;
	int32_t posX;
	int32_t posY;
	int32_t layer;
} block_l;

typedef struct {
	int32_t usage;
	float useRelX;
	float useRelY;
} usage_t;
