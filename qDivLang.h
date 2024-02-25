#include<string.h>

#define QDIV_TEXT(tag, value) static const struct translate_s tag = {value, strlen(value)}

#define QDIV_LANG "en_MX"

// Menu
QDIV_TEXT(T_PLAY, "Play");
QDIV_TEXT(T_SETTINGS, "Settings");
QDIV_TEXT(T_ABOUT_QDIV, "About qDiv");
QDIV_TEXT(T_VSYNC_ON, "Vsync On");
QDIV_TEXT(T_VSYNC_OFF, "Vsync Off");
QDIV_TEXT(T_CURSOR_SYNCED, "Cursor: Synced");
QDIV_TEXT(T_CURSOR_INSTANT, "Cursor: Instant");
QDIV_TEXT(T_DERIVE, "ID: Derive");
QDIV_TEXT(T_REUSE, "ID: Reuse");
QDIV_TEXT(T_HIDDEN_DEBUG, "Hidden Debug");
QDIV_TEXT(T_VERBOSE_DEBUG, "Verbose Debug");
QDIV_TEXT(T_CONNECT, "Connecting to specified Server");
QDIV_TEXT(T_PAGE, "Page %d/%d");

// Roles
QDIV_TEXT(T_WARRIOR, "Warrior");
QDIV_TEXT(T_EXPLORER, "Explorer");
QDIV_TEXT(T_BUILDER, "Builder");
QDIV_TEXT(T_GARDENER, "Farmer");
QDIV_TEXT(T_ENGINEER, "Engineer");
QDIV_TEXT(T_WIZARD, "Wizard");

// Artifacts
QDIV_TEXT(T_WALL_DESC, "Placeable Wall");
QDIV_TEXT(T_FLOOR_DESC, "Placeable Floor");
QDIV_TEXT(T_CROP_DESC, "Placeable Crop.\nTakes %d minutes to grow.");
QDIV_TEXT(T_LAMP_DESC, "Placeable Lamp.\nNeeds to be relit every %d minutes.");
QDIV_TEXT(T_TOOL_DESC, "Mines a block every %d seconds.");
