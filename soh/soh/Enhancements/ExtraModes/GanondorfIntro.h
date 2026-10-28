#pragma once

// SoH: "Play as Ganondorf", the Gerudo intro: a cutscene of its own for files that start in the Gerudo Fortress.
//
// The director (GanondorfIntro.cpp) runs it shot by shot through real scenes: it moves the camera, places Link (who
// Ganondorf follows), spawns the Gerudo, horses and props, and shows the dialogue. Ganondorf's own code
// (ovl_Boss_Ganon) reads gGdfIntroCs every frame for what the director wants from him: an arm pose, where he looks,
// a glow in his hand, a horse to ride, and the props it draws (the map table, the letter, the vision).
// Include after z64.h.

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GDF_CSPOSE_NONE,   // his own animation
    GDF_CSPOSE_WINDOW, // right hand behind his back, left on the windowsill
    GDF_CSPOSE_TABLE,  // right hand down on the map table
    GDF_CSPOSE_RAISE,  // right arm raised high (open hand or fist: openHand)
    GDF_CSPOSE_LETTER, // both hands before his chest, holding the letter
    GDF_CSPOSE_CROSSED, // arms crossed
    GDF_CSPOSE_HAND     // right hand held up before him, palm to his face
} GdfCsPose;

typedef struct {
    u8 active;      // the intro is running
    u8 pose;        // GdfCsPose
    u8 openHand;    // right hand open
    u8 laugh;       // set to make him laugh (cleared when he starts)
    u8 hideAll;     // neither Ganondorf nor Link is drawn (shots without him)
    u8 showLink;    // Link is drawn instead of Ganondorf (the vision of the boy, the sleeping boy)
    f32 headYaw;    // his head turned this many degrees to his left (negative: right)
    f32 headPitch;  // and down this many degrees
    f32 handGlow;   // a glow in his right hand (the light ball's scale)
    struct Actor* horse; // the black horse he's riding (EnHorseGanon), or NULL

    // Props, drawn with him (positions in the world)
    u8 voidAlpha;       // the black void around the camera (the vision)
    f32 goldLight;      // the golden light in the void (scale, 0 off)
    Vec3f goldPos;
    f32 triforceScale;  // the three triangles turning in the dark (0 off)
    Vec3f triforcePos;
    s16 triforceYaw;
    u8 triforceAlpha;
    u8 tableOn;         // the war table with the map of Hyrule
    Vec3f tablePos;     // the middle of its top
    s16 tableYaw;
    u8 stoneOn;         // the black stone on the map
    f32 stoneU;         // where on the map (texels of the 216x128 map)
    f32 stoneV;
    f32 stoneDrop;      // how far above the map (it's set down)
    u8 letterOn;        // the king's letter, in his hands
    u8 sealBroken;
    u8 ocarinaOn;       // the ocarina glowing blue in the boy's hands
    Vec3f ocarinaPos;
    Vec3f cameraEye;    // where the camera is (the void is drawn around it)
} GdfIntroCs;

extern GdfIntroCs gGdfIntroCs;

// The map table's size (world units) and height
#define GDF_TABLE_W 150.0f
#define GDF_TABLE_D 89.0f
#define GDF_TABLE_H 62.0f

// Is the intro running
s32 GanondorfIntro_IsActive(void);

// Start it (a new Gerudo file, or the menu's replay button)
void GanondorfIntro_Start(s32 fromNewFile);

#ifdef __cplusplus
}
#endif
