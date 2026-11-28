// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// SoH: "Play as Ganondorf", the final battle with the roles swapped. At the top of Ganon's Tower the tower Ganondorf
// (Boss_Ganon) gives way to the Hero: Dark Link's actor (ovl_En_Torch2, a second Player that a made-up controller
// drives, so he moves with Link's own moveset) in Link's own look: green tunic, Master Sword, Mirror Shield. Dark Link's
// fencing (slashes, combos, rolls, backflips, side hops) is his, and on top of it, here:
//  - his sword arts: spin attacks (held and released) and jump attacks
//  - his bow: Fire, Ice and Light Arrows at range
//  - bombs, lit and thrown
//  - Din's Fire when Ganondorf crowds him, Nayru's Love when he's hurt (nothing gets through it while it lasts)
//  - potions (red: half his health back) and a fairy that brings him back once
//  - his hearts, and double defence (every hit on him halved)
// Beaten, he falls and stays down (the ending with the roles swapped comes next). Every number is tunable (hero.*).
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_Magic_Fire/z_magic_fire.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "GanondorfTuning.h"
#include "GanondorfHeroLink.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
extern PlayState* gPlayState;
void Player_DrawGameplay(PlayState* play, Player* this_, s32 lod, Gfx* cullDList, OverrideLimbDrawOpa overrideLimbDraw);
Actor* BossGanon_HeroIncomingBall(PlayState* play, Actor* hero, f32 range, s32* big);
void BossGanon_Player_CsStagger(s16 frames);
s32 PlayAsGanondorf_IsLevitating(void);
void PlayAsGanondorf_KnockOutOfAir(void);
void BossGanon_HeroReturnBall(PlayState* play, Actor* ball);
}

namespace {

enum HeroAct : u8 { HA_NONE, HA_BOW, HA_BOMB, HA_DIN, HA_NAYRU, HA_DRINK };

#define HERO_ARROWS 8

struct HeroState {
    u8 act = HA_NONE;
    s16 t = 0;
    s16 arrow = ARROW_LIGHT;
    s8 potions = 2;
    bool fairy = true;
    bool nayruUsed = false;
    s16 nayru = 0;     // frames of Nayru's Love left
    s16 cooldown = 90; // to the next bow or bomb
    s16 dinCd = 200;
    s16 jumpCd = 60;
    s16 spin = 0;      // frames into a held spin attack
    s16 fallenT = 0;
    u8 maxHealth = 48;
    Actor* bomb = nullptr;
    Actor* arrows[HERO_ARROWS] = {};
    s32 arrowNext = 0;
    Actor* din = nullptr;
    // Tennis
    Actor* swingBall = nullptr; // the light ball he's swinging at
    bool swingGood = false;     // and whether he hits it
    bool spinForBall = false;   // charging a spin attack for Ganondorf's Big Magic
    bool spinGood = false;
    s16 spinReflectT = 0;       // frames his released spin sends Big Magic back
    f32 bigDist = 9999.0f;      // the nearest Big Magic coming at him
    // The classic rules
    s16 stunT = 0;      // frames Ganondorf lies stunned, open to the sword
    u32 seenFrame = 0;  // the last frame he fought (he's here)
    bool engaged = false;
    s16 strafe = 0;     // his drift along the edge
    s16 closeT = 0;     // frames Ganondorf has been right on him
};
HeroState sH;

#define ANIM(name) ((LinkAnimationHeader*)(void*)(name))

bool HeroOn() {
    return CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.Enabled"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.VillainLink"), 1) &&
           CVarGetInteger(CVAR_ENHANCEMENT("PlayAsGanondorf.HeroBoss"), 1);
}

s32 Tune(const char* key, f32 def, s32 lo, s32 hi) {
    return CLAMP((s32)GdfTune(key, def), lo, hi);
}

bool Classic() {
    return Tune("hero.classic", 1.0f, 0, 1) != 0;
}

// Ganondorf aloft and not stunned: the Hero keeps to the edge, and nothing of his touches him but the rally and the
// Light Arrow
bool GanondorfAloft() {
    PlayState* play = gPlayState;
    bool aloft = PlayAsGanondorf_IsLevitating();

    if (!aloft && (play != nullptr)) {
        // (also floating down, or anywhere well off the floor)
        Player* player = GET_PLAYER(play);
        aloft = !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                (player->actor.world.pos.y - player->actor.floorHeight > GdfTune("hero.aloft_height", 45.0f));
    }
    return Classic() && aloft && (sH.stunT == 0);
}

bool ActorStillThere(PlayState* play, Actor* a, s32 cat) {
    if (a == nullptr) {
        return false;
    }
    for (Actor* it = play->actorCtx.actorLists[cat].head; it != nullptr; it = it->next) {
        if (it == a) {
            return it->update != nullptr;
        }
    }
    return false;
}

void Anim(PlayState* play, Player* hero, const char* anim, bool loop, f32 morph = -6.0f) {
    LinkAnimationHeader* a = ANIM(anim);
    LinkAnimation_Change(play, &hero->skelAnime, a, 1.0f, 0.0f, (f32)Animation_GetLastFrame(a),
                         (u8)(loop ? ANIMMODE_LOOP : ANIMMODE_ONCE), morph);
}

void Sparkles(PlayState* play, Vec3f* at, s32 count, u8 r, u8 g, u8 b) {
    static Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { 255, 255, 255, 255 };
    Color_RGBA8 env = { r, g, b, 0 };

    for (s32 i = 0; i < count; i++) {
        Vec3f pos = { at->x + Rand_CenteredFloat(50.0f), at->y + Rand_ZeroFloat(70.0f), at->z + Rand_CenteredFloat(50.0f) };
        Vec3f vel = { 0.0f, 1.0f + Rand_ZeroFloat(1.5f), 0.0f };
        EffectSsKiraKira_SpawnDispersed(play, &pos, &vel, &accel, &prim, &env, 1200, 16);
    }
}

// The way he faces: to Ganondorf
void FaceFoe(Player* hero) {
    Math_SmoothStepToS(&hero->actor.shape.rot.y, hero->actor.yawTowardsPlayer, 2, 0x1800, 0x100);
    hero->actor.world.rot.y = hero->yaw = hero->actor.shape.rot.y;
}

Vec3f AheadOf(Player* hero, f32 up, f32 ahead) {
    s16 yaw = hero->actor.shape.rot.y;
    return { hero->actor.world.pos.x + Math_SinS(yaw) * ahead, hero->actor.world.pos.y + up,
             hero->actor.world.pos.z + Math_CosS(yaw) * ahead };
}

void StartAct(PlayState* play, Player* hero, u8 act) {
    sH.act = act;
    sH.t = 0;
    sH.spin = 0;
    hero->meleeWeaponState = 0;
    hero->linearVelocity = 0.0f;
    hero->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
}

void EndAct(PlayState* play, Player* hero) {
    if (ActorStillThere(play, sH.bomb, ACTORCAT_EXPLOSIVE) && (GET_PLAYER(play)->heldActor != sH.bomb)) {
        sH.bomb->velocity.y = 2.0f; // (dropped where he stands, still lit)
    }
    sH.bomb = nullptr;
    sH.act = HA_NONE;
    hero->stateFlags3 &= ~PLAYER_STATE3_PAUSE_ACTION_FUNC;
    hero->itemAction = PLAYER_IA_SWORD_MASTER;
    hero->heldItemAction = PLAYER_IA_SWORD_MASTER;
    Player_SetModelGroup(hero, PLAYER_MODELGROUP_SWORD_AND_SHIELD);
    play->func_11D54(hero, play); // (back to standing ready)
    sH.cooldown = (s16)((f32)Tune("hero.item_wait", 100.0f, 10, 2000) + Rand_ZeroFloat(80.0f));
}

void ShootArrow(PlayState* play, Player* hero) {
    Player* player = GET_PLAYER(play);
    Vec3f from = AheadOf(hero, 52.0f, 22.0f);
    Vec3f to = player->actor.world.pos;
    f32 dx, dy, dz, dxz;
    EnArrow* arrow;

    to.y += 60.0f;
    dx = to.x - from.x;
    dy = to.y - from.y;
    dz = to.z - from.z;
    dxz = sqrtf(SQ(dx) + SQ(dz));
    player->unk_A73 = 4; // (an arrow only leaves the bow while the player has just shot one: see EnArrow_Shoot)
    arrow = (EnArrow*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ARROW, from.x, from.y, from.z,
                                  RADF_TO_BINANG(Math_FAtan2F(-dy, dxz)), RADF_TO_BINANG(Math_FAtan2F(dx, dz)), 0,
                                  sH.arrow);
    if (arrow == nullptr) {
        return;
    }
    // His: it hurts Ganondorf, not the enemies
    arrow->collider.base.atFlags = AT_ON | AT_TYPE_ENEMY;
    arrow->collider.info.toucher.damage = (u8)Tune("hero.arrow_damage", 12.0f, 0, 255);
    switch (sH.arrow) {
        case ARROW_FIRE:
            arrow->collider.info.toucher.effect = HIT_SPECIAL_EFFECT_FIRE;
            break;
        case ARROW_ICE:
            arrow->collider.info.toucher.effect =
                Tune("hero.ice_freezes", 1.0f, 0, 1) ? HIT_SPECIAL_EFFECT_ICE : HIT_SPECIAL_EFFECT_NONE;
            break;
        default:
            arrow->collider.info.toucher.effect = HIT_SPECIAL_EFFECT_KNOCKBACK;
            arrow->collider.info.toucher.damage = (u8)Tune("hero.light_arrow_damage", 16.0f, 0, 255);
            break;
    }
    sH.arrows[sH.arrowNext] = &arrow->actor;
    sH.arrowNext = (sH.arrowNext + 1) % HERO_ARROWS;
}

void RunAct(PlayState* play, Player* hero, Input* input) {
    Player* player = GET_PLAYER(play);
    s16 t = sH.t++;

    hero->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
    hero->linearVelocity = 0.0f;
    input->cur.button = input->press.button = input->rel.button = 0;
    input->cur.stick_x = input->cur.stick_y = input->rel.stick_x = input->rel.stick_y = 0;
    FaceFoe(hero);
    LinkAnimation_Update(play, &hero->skelAnime);

    switch (sH.act) {
        case HA_BOW:
            if (t == 0) {
                static const s16 kinds[] = { ARROW_FIRE, ARROW_ICE, ARROW_LIGHT };
                // (at Ganondorf in the air only the Light Arrow will do)
                sH.arrow = (Classic() && PlayAsGanondorf_IsLevitating()) ? ARROW_LIGHT
                                                                        : kinds[(s32)Rand_ZeroFloat(2.99f)];
                hero->itemAction = hero->heldItemAction = PLAYER_IA_BOW_LIGHT;
                Player_SetModelGroup(hero, PLAYER_MODELGROUP_BOW_SLINGSHOT);
                Anim(play, hero, gPlayerAnim_link_bow_bow_ready, false);
                Audio_PlayActorSound2(&hero->actor, NA_SE_IT_BOW_DRAW);
            } else if (t == 18) {
                ShootArrow(play, hero);
                Audio_PlayActorSound2(&hero->actor, NA_SE_IT_MAGIC_ARROW_SHOT);
                Anim(play, hero, gPlayerAnim_link_bow_bow_shoot_next, false, -2.0f);
            } else if (t == 32) {
                Anim(play, hero, gPlayerAnim_link_bow_bow_shoot_end, false);
            } else if (t >= 46) {
                EndAct(play, hero);
            }
            break;

        case HA_BOMB:
            if (t == 0) {
                hero->itemAction = hero->heldItemAction = PLAYER_IA_BOMB;
                Player_SetModelGroup(hero, PLAYER_MODELGROUP_EXPLOSIVES);
                Anim(play, hero, gPlayerAnim_link_normal_light_bom, false);
            } else if (t == 8) {
                Vec3f at = AheadOf(hero, 75.0f, 6.0f);
                EnBom* bomb = (EnBom*)Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOM, at.x, at.y, at.z, 0,
                                                  hero->actor.shape.rot.y, 0, BOMB_BODY);
                if (bomb != nullptr) {
                    bomb->timer = (s16)Tune("hero.bomb_fuse", 45.0f, 8, 70);
                    sH.bomb = &bomb->actor;
                }
            }
            if ((sH.bomb != nullptr) && (!ActorStillThere(play, sH.bomb, ACTORCAT_EXPLOSIVE) ||
                                         (player->heldActor == sH.bomb))) {
                sH.bomb = nullptr; // (gone, or Ganondorf took it from him)
            }
            if ((sH.bomb != nullptr) && (t < 24)) {
                // Held up over his head
                sH.bomb->world.pos = AheadOf(hero, 75.0f, 6.0f);
                sH.bomb->velocity.y = 0.0f;
                sH.bomb->speedXZ = 0.0f;
            }
            if (t == 20) {
                Anim(play, hero, gPlayerAnim_link_normal_throw_free, false);
            } else if (t == 24) {
                if (sH.bomb != nullptr) {
                    sH.bomb->world.rot.y = hero->actor.shape.rot.y;
                    sH.bomb->speedXZ = CLAMP(hero->actor.xzDistToPlayer / 20.0f, 5.0f, 16.0f);
                    sH.bomb->velocity.y = 6.0f;
                    sH.bomb = nullptr;
                }
                Audio_PlayActorSound2(&hero->actor, NA_SE_VO_LI_SWORD_N);
                Audio_PlayActorSound2(&hero->actor, NA_SE_PL_THROW);
            } else if (t >= 40) {
                EndAct(play, hero);
            }
            break;

        case HA_DIN:
            if (t == 0) {
                hero->itemAction = hero->heldItemAction = PLAYER_IA_NONE;
                Player_SetModelGroup(hero, PLAYER_MODELGROUP_DEFAULT);
                Anim(play, hero, gPlayerAnim_link_magic_honoo1, false);
                Audio_PlayActorSound2(&hero->actor, NA_SE_VO_LI_SWORD_L);
            } else if (t == 12) {
                MagicFire* fire = (MagicFire*)Actor_SpawnAsChild(&play->actorCtx, &hero->actor, play,
                                                                 ACTOR_MAGIC_FIRE, hero->actor.world.pos.x,
                                                                 hero->actor.world.pos.y, hero->actor.world.pos.z, 0,
                                                                 0, 0, 0);
                if (fire != nullptr) {
                    fire->collider.base.atFlags = AT_ON | AT_TYPE_ENEMY;
                    fire->collider.info.toucher.effect = HIT_SPECIAL_EFFECT_FIRE;
                    sH.din = &fire->actor;
                }
                Anim(play, hero, gPlayerAnim_link_magic_honoo2, true);
            } else if (t == 54) {
                Anim(play, hero, gPlayerAnim_link_magic_honoo3, false);
            } else if (t >= 72) {
                sH.dinCd = (s16)Tune("hero.din_wait", 900.0f, 30, 5000);
                EndAct(play, hero);
            }
            break;

        case HA_NAYRU:
            if (t == 0) {
                hero->itemAction = hero->heldItemAction = PLAYER_IA_NONE;
                Player_SetModelGroup(hero, PLAYER_MODELGROUP_DEFAULT);
                Anim(play, hero, gPlayerAnim_link_magic_tamashii1, false);
            } else if (t == 14) {
                sH.nayru = (s16)Tune("hero.nayru_frames", 600.0f, 20, 30000);
                sH.nayruUsed = true;
                Audio_PlayActorSound2(&hero->actor, NA_SE_PL_MAGIC_SOUL_FLASH);
                Anim(play, hero, gPlayerAnim_link_magic_tamashii2, true);
            } else if (t == 40) {
                Anim(play, hero, gPlayerAnim_link_magic_tamashii3, false);
            } else if (t >= 56) {
                EndAct(play, hero);
            }
            break;

        case HA_DRINK:
            if (t == 0) {
                hero->itemAction = hero->heldItemAction = PLAYER_IA_BOTTLE_POTION_RED;
                Player_SetModelGroup(hero, PLAYER_MODELGROUP_BOTTLE);
                Anim(play, hero, gPlayerAnim_link_bottle_drink_demo_start, false);
            } else if (t == 18) {
                Anim(play, hero, gPlayerAnim_link_bottle_drink_demo_wait, true);
                Audio_PlayActorSound2(&hero->actor, NA_SE_VO_LI_DRINK);
            } else if (t == 50) {
                s32 heal = (sH.maxHealth * Tune("hero.potion_heal_percent", 50.0f, 0, 100)) / 100;
                Vec3f at = hero->actor.world.pos;

                hero->actor.colChkInfo.health = (u8)MIN(sH.maxHealth, hero->actor.colChkInfo.health + heal);
                sH.potions--;
                Audio_PlayActorSound2(&hero->actor, NA_SE_SY_HP_RECOVER);
                Sparkles(play, &at, 10, 255, 60, 60);
                Anim(play, hero, gPlayerAnim_link_bottle_drink_demo_end, false);
            } else if (t >= 70) {
                EndAct(play, hero);
            }
            break;
    }
}

// The stick, from a way in the world to his controller's (camera-relative, as Dark Link's)
void SetStick(PlayState* play, Input* input, s16 worldYaw, f32 tilt) {
    s16 rel = Play_GetCamera(play, 0)->camDir.y - worldYaw;

    input->cur.stick_x = (s8)(tilt * Math_SinS(rel));
    input->cur.stick_y = (s8)(tilt * Math_CosS(rel));
    PadUtils_UpdateRelXY(input);
    if (CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0)) {
        input->rel.stick_x = (s8)-input->rel.stick_x;
    }
}

// Spin attacks and jump attacks, on top of Dark Link's fencing
void SwordArts(PlayState* play, Player* hero, Input* input, bool ready) {
    if (sH.spin > 0) {
        s16 hold = (s16)Tune("hero.spin_hold", 34.0f, 12, 120);

        sH.spin++;
        input->cur.stick_x = input->cur.stick_y = 0;
        PadUtils_UpdateRelXY(input);
        // (charged for the Big Magic: let go as it comes in)
        bool keep = sH.spinForBall ? ((sH.bigDist > GdfTune("hero.spin_release_range", 170.0f)) && (sH.spin < 80))
                                   : (sH.spin < hold);
        if (keep) {
            input->cur.button = BTN_B | BTN_Z;
            input->press.button = 0;
            input->rel.button = 0;
            input->prev.button = BTN_B | BTN_Z;
        } else {
            input->cur.button = BTN_Z;
            input->press.button = 0;
            input->rel.button = BTN_B; // let go: the spin
            input->prev.button = BTN_Z;
            sH.spin = 0;
            if (sH.spinForBall) {
                sH.spinForBall = false;
                sH.spinReflectT = 14;
                sH.spinGood = Rand_ZeroOne() < GdfTune("hero.tennis_skill", 0.8f);
                Audio_PlayActorSound2(&hero->actor, NA_SE_VO_LI_SWORD_L);
            }
        }
        return;
    }
    if (!ready) {
        return;
    }
    f32 d = hero->actor.xzDistToPlayer;

    if ((d < 105.0f) && (Rand_ZeroOne() < GdfTune("hero.spin_chance", 0.006f))) {
        sH.spin = 1;
        input->cur.button = BTN_B | BTN_Z;
        input->press.button = BTN_B;
        input->prev.button = BTN_Z;
        return;
    }
    if ((sH.jumpCd == 0) && (d > 130.0f) && (d < 250.0f) && (Rand_ZeroOne() < GdfTune("hero.jump_chance", 0.012f))) {
        sH.jumpCd = (s16)Tune("hero.jump_wait", 150.0f, 10, 3000);
        hero->actor.world.rot.y = hero->actor.shape.rot.y = hero->actor.yawTowardsPlayer;
        input->cur.button = BTN_A | BTN_Z;
        input->press.button = BTN_A;
        input->prev.button = BTN_Z;
        SetStick(play, input, hero->actor.yawTowardsPlayer, 127.0f);
        Audio_PlayActorSound2(&hero->actor, NA_SE_VO_LI_SWORD_L);
    }
}

// Ganondorf's magic: his light balls sent back with a slash (and back again, if Ganondorf's cape is up, R, as they
// reach him), his Big Magic with a spin attack. True when it took his controller this frame.
bool Tennis(PlayState* play, Player* hero, Input* input, u8 attacking) {
    s32 big = false;
    Actor* ball;
    f32 d;

    if (!Tune("hero.tennis", 1.0f, 0, 1)) {
        return false;
    }
    // His released spin: every Big Magic close enough goes back
    if (sH.spinReflectT > 0) {
        sH.spinReflectT--;
        for (s32 i = 0; (i < 5) && sH.spinGood; i++) {
            s32 isBig = false;
            Actor* b = BossGanon_HeroIncomingBall(play, &hero->actor, GdfTune("hero.spin_reflect_range", 230.0f), &isBig);

            if ((b == nullptr) || !isBig) {
                break;
            }
            BossGanon_HeroReturnBall(play, b);
        }
    }
    ball = (sH.act == HA_NONE) ? BossGanon_HeroIncomingBall(play, &hero->actor, 900.0f, &big) : nullptr;
    d = (ball != nullptr) ? Math_Vec3f_DistXYZ(&hero->actor.world.pos, &ball->world.pos) : 9999.0f;
    sH.bigDist = ((ball != nullptr) && big) ? d : 9999.0f;
    if ((ball == nullptr) || !attacking || (hero->knockbackType != 0) || (sH.fallenT > 0)) {
        return false;
    }
    if (big) {
        // A spin attack, charged as it comes
        if ((sH.spin == 0) && (d < GdfTune("hero.spin_charge_range", 520.0f)) &&
            (hero->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (hero->meleeWeaponState == 0)) {
            sH.spin = 1;
            sH.spinForBall = true;
            input->cur.button = BTN_B | BTN_Z;
            input->press.button = BTN_B;
            input->prev.button = BTN_Z;
            return true;
        }
        return false;
    }
    // A light ball: a slash to meet it, and back it goes
    f32 speed = MAX(ball->speedXZ, 10.0f);

    if ((sH.swingBall != ball) && (d < MAX(GdfTune("hero.swing_range", 170.0f), speed * 7.0f))) {
        sH.swingBall = ball;
        sH.swingGood = Rand_ZeroOne() < GdfTune("hero.tennis_skill", 0.8f);
        sH.spin = 0;
        hero->actor.shape.rot.y = hero->actor.world.rot.y = hero->yaw =
            Math_Vec3f_Yaw(&hero->actor.world.pos, &ball->world.pos);
        input->cur.button = BTN_B;
        input->press.button = BTN_B;
        input->prev.button = 0;
        input->cur.stick_x = input->cur.stick_y = 0;
        PadUtils_UpdateRelXY(input);
        return true;
    }
    if ((sH.swingBall == ball) && sH.swingGood && (d < MAX(110.0f, speed * 2.5f))) {
        BossGanon_HeroReturnBall(play, ball);
        sH.swingBall = nullptr; // (ready for it when it comes back)
        return false;
    }
    // (swinging at it: no bow or bombs until it's dealt with)
    return sH.swingBall == ball;
}

// Ganondorf aloft: away from the middle, out along the edge of the floor, where his bow and his sword's returns do the
// work. (He comes in for the kill when Ganondorf is struck down, and duels him whenever he stands on the floor.)
void KeepToEdge(PlayState* play, Player* hero, Input* input) {
    f32 d = hero->actor.xzDistToPlayer;
    f32 keep = GdfTune("hero.keep_away", 380.0f);
    s16 away = (s16)(hero->actor.yawTowardsPlayer + 0x8000);

    input->cur.button &= (u16) ~(BTN_A | BTN_B);
    input->press.button &= (u16) ~(BTN_A | BTN_B);
    if (sH.spin > 0) {
        return;
    }
    if (d < keep) {
        SetStick(play, input, away, 127.0f); // back out
    } else {
        // Drifting round the edge, facing him
        if (--sH.strafe <= -90) {
            sH.strafe = 90;
        }
        if (d > keep + 160.0f) {
            input->cur.stick_x = input->cur.stick_y = 0;
            PadUtils_UpdateRelXY(input);
        } else {
            SetStick(play, input, (s16)(hero->actor.yawTowardsPlayer + ((sH.strafe > 0) ? 0x4000 : -0x4000)), 45.0f);
        }
        input->cur.button |= BTN_Z;
    }
}

// His Light Arrows reaching Ganondorf aloft (whom nothing touches then): struck down
void LightArrowsReach(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f chest = player->actor.world.pos;

    chest.y += 60.0f;
    for (Actor*& a : sH.arrows) {
        if ((a == nullptr) || !ActorStillThere(play, a, ACTORCAT_ITEMACTION)) {
            a = nullptr;
            continue;
        }
        if ((a->params == ARROW_LIGHT) && (Math_Vec3f_DistXYZ(&a->world.pos, &chest) < 70.0f) && Classic()) {
            EffectSsHitMark_SpawnFixedScale(play, 3, &chest);
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_EXPLOSION_LIGHT);
            GdfHero_StunGanondorf(play);
            Actor_Kill(a);
            a = nullptr;
        }
    }
}

void ResetState() {
    sH = HeroState();
}

s16 sPendingScene = -1; // the Hero is due here

void OnSpawnEntry(bool* should, va_list args) {
    ActorContext* actorCtx = va_arg(args, ActorContext*);
    ActorEntry* entry = va_arg(args, ActorEntry*);
    PlayState* play = va_arg(args, PlayState*);
    Actor** out = va_arg(args, Actor**);

    if (!*should || (play == nullptr) || (play->sceneNum != SCENE_GANONDORF_BOSS) || (entry->id != ACTOR_BOSS_GANON) ||
        (entry->params >= 0x64) || !HeroOn() || (gSaveContext.gameMode != GAMEMODE_NORMAL)) {
        return; // (the title screen is set in this room too: its Ganondorf stays)
    }
    // The tower Ganondorf's place: the Hero, waiting before the organ (its room, carpet and statues stay)
    *should = false;
    Flags_SetSwitch(play, 0x14);
    *out = Actor_Spawn(actorCtx, play, ACTOR_EN_GANON_ORGAN, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
    // He comes a moment later, once the room is loaded: Dark Link's actor needs an object this room doesn't load, and
    // while a room is loading the game spawns nothing without its object
    sPendingScene = play->sceneNum;
}

void SpawnPendingHero() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || (sPendingScene < 0)) {
        return;
    }
    if (play->sceneNum != sPendingScene) {
        sPendingScene = -1; // (gone before he came)
        return;
    }
    sPendingScene = -1;
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_TORCH2, 0.0f, 0.0f, GdfTune("hero.start_z", -150.0f), 0, 0, 0,
                GDF_HERO_PARAMS);
}

void RegisterHeroLink() {
    REGISTER_VB_SHOULD(VB_SPAWN_ACTOR_ENTRY, { OnSpawnEntry(should, args); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { ResetState(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() { SpawnPendingHero(); });
}

RegisterShipInitFunc initFunc(RegisterHeroLink, {});

} // namespace

extern "C" s32 GdfHero_Is(Actor* actor) {
    return (actor != nullptr) && (actor->id == ACTOR_EN_TORCH2) && (actor->params == GDF_HERO_PARAMS);
}

extern "C" void GdfHero_Init(Player* hero, PlayState* play) {
    ResetState();
    sH.maxHealth = (u8)Tune("hero.health", 48.0f, 1, 255);
    sH.potions = (s8)Tune("hero.potions", 2.0f, 0, 9);
    sH.fairy = Tune("hero.fairy", 1.0f, 0, 1) != 0;
    hero->currentShield = PLAYER_SHIELD_MIRROR;
    hero->currentTunic = PLAYER_TUNIC_KOKIRI;
    hero->currentBoots = PLAYER_BOOTS_KOKIRI;
    Player_SetModelGroup(hero, PLAYER_MODELGROUP_SWORD_AND_SHIELD);
    hero->actor.colChkInfo.health = sH.maxHealth;
    hero->meleeWeaponQuads[0].info.toucher.damage = hero->meleeWeaponQuads[1].info.toucher.damage =
        (u8)Tune("hero.sword_damage", 8.0f, 0, 255);
    hero->actor.shape.rot.y = hero->actor.world.rot.y = 0;
    hero->actor.naviEnemyId = 0xFF; // (not Dark Link's)
}

// Struck (and not killed): he's back to the fight once he's back on his feet. (Dark Link sinks into the floor and
// comes up elsewhere: not the Hero)
extern "C" s32 GdfHero_Recovered(Player* hero) {
    return !(hero->stateFlags1 & PLAYER_STATE1_DAMAGED) && (hero->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
           (hero->knockbackType == 0);
}

extern "C" s32 GdfHero_Engages(Player* hero, PlayState* play) {
    if (hero->actor.xzDistToPlayer > GdfTune("hero.notice_range", 520.0f)) {
        return false;
    }
    Audio_PlayActorSound2(&hero->actor, NA_SE_VO_LI_SWORD_L);
    return true;
}

extern "C" u16 GdfHero_Music(void) {
    return (u16)Tune("hero.music", (f32)NA_BGM_GANONDORF_BOSS, 0, 0x7F);
}

extern "C" void GdfHero_Brain(Player* hero, PlayState* play, Input* input, u8 attacking) {
    sH.seenFrame = play->gameplayFrames;
    sH.closeT = (hero->actor.xzDistToPlayer < 150.0f) ? (s16)MIN(sH.closeT + 1, 3000) : 0;
    sH.engaged = attacking && (sH.fallenT == 0);
    if (sH.stunT > 0) {
        sH.stunT--;
    }
    LightArrowsReach(play);
    if (sH.nayru > 0) {
        sH.nayru--;
        if ((sH.nayru % 5) == 0) {
            Vec3f at = hero->actor.world.pos;
            Sparkles(play, &at, 1, 60, 120, 255);
        }
    }
    if (sH.cooldown > 0) {
        sH.cooldown--;
    }
    if (sH.dinCd > 0) {
        sH.dinCd--;
    }
    if (sH.jumpCd > 0) {
        sH.jumpCd--;
    }
    if (sH.fallenT > 0) {
        return;
    }
    if (Tennis(play, hero, input, attacking)) {
        return;
    }
    if (sH.act != HA_NONE) {
        // Struck (or the fight paused): he lets it go
        if (!attacking || (hero->knockbackType != 0) || (hero->stateFlags2 & PLAYER_STATE2_FROZEN)) {
            EndAct(play, hero);
            return;
        }
        RunAct(play, hero, input);
        return;
    }
    if (!attacking) {
        return;
    }

    bool ready = (hero->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (hero->meleeWeaponState == 0) &&
                 (hero->knockbackType == 0) && (hero->invincibilityTimer <= 0) &&
                 !(hero->stateFlags1 & (PLAYER_STATE1_DAMAGED | PLAYER_STATE1_IN_CUTSCENE)) &&
                 !(hero->stateFlags2 & PLAYER_STATE2_FROZEN) && !(hero->stateFlags3 & PLAYER_STATE3_PAUSE_ACTION_FUNC) &&
                 (sH.spin == 0);
    f32 d = hero->actor.xzDistToPlayer;
    s32 hp = hero->actor.colChkInfo.health;

    if (ready) {
        if ((hp * 4 <= sH.maxHealth) && (sH.potions > 0) && (d > 200.0f)) {
            StartAct(play, hero, HA_DRINK);
            RunAct(play, hero, input);
            return;
        }
        if ((hp * 2 <= sH.maxHealth) && !sH.nayruUsed && Tune("hero.nayru", 1.0f, 0, 1)) {
            StartAct(play, hero, HA_NAYRU);
            RunAct(play, hero, input);
            return;
        }
        if ((d < 150.0f) && (sH.dinCd == 0) && Tune("hero.din", 1.0f, 0, 1) && !GanondorfAloft() &&
            (sH.closeT >= Tune("hero.din_crowded", 75.0f, 0, 2000)) &&
            (Rand_ZeroOne() < GdfTune("hero.din_chance", 0.006f))) {
            StartAct(play, hero, HA_DIN);
            RunAct(play, hero, input);
            return;
        }
        if ((d > GdfTune("hero.ranged_from", 300.0f)) && (sH.cooldown == 0)) {
            bool bomb = (d < 650.0f) && Tune("hero.bombs", 1.0f, 0, 1) && !GanondorfAloft() && (Rand_ZeroOne() < 0.4f);
            if (bomb || Tune("hero.bow", 1.0f, 0, 1)) {
                StartAct(play, hero, bomb ? HA_BOMB : HA_BOW);
                RunAct(play, hero, input);
                return;
            }
        }
    }
    if (GanondorfAloft()) {
        KeepToEdge(play, hero, input);
        return;
    }
    // Up close he presses the attack: slashes, and combos as he keeps at it
    if (ready && (sH.spin == 0) && (d < GdfTune("hero.sword_range", 100.0f)) &&
        (Rand_ZeroOne() < GdfTune("hero.sword_chance", 0.2f))) {
        hero->actor.world.rot.y = hero->actor.shape.rot.y = hero->actor.yawTowardsPlayer;
        input->cur.button |= BTN_B;
        input->press.button |= BTN_B;
        input->prev.button &= (u16)~BTN_B;
        return;
    }
    SwordArts(play, hero, input, ready);
}

extern "C" s32 GdfHero_TakeHit(Player* hero, PlayState* play) {
    Vec3f at = hero->actor.world.pos;
    s32 dmg = hero->actor.colChkInfo.damage;
    s32 div = Tune("hero.defence", 2.0f, 1, 16);

    if (sH.nayru > 0) {
        // Nayru's Love: nothing gets through
        Audio_PlayActorSound2(&hero->actor, NA_SE_IT_SHIELD_REFLECT_SW);
        Sparkles(play, &at, 6, 60, 120, 255);
        hero->actor.colChkInfo.damage = 0;
        return false;
    }
    if (dmg > 0) {
        dmg = MAX(1, (dmg + div - 1) / div); // double defence
    }
    if ((dmg >= hero->actor.colChkInfo.health) && sH.fairy) {
        // The fairy in his bottle brings him back
        sH.fairy = false;
        hero->actor.colChkInfo.health =
            (u8)MAX(1, (sH.maxHealth * Tune("hero.fairy_heal_percent", 50.0f, 1, 100)) / 100);
        hero->actor.colChkInfo.damage = 0;
        hero->invincibilityTimer = 40;
        Audio_PlayActorSound2(&hero->actor, NA_SE_EV_BUTTERFRY_TO_FAIRY); // (the fairy's heal sound loops)
        Audio_PlayActorSound2(&hero->actor, NA_SE_SY_HP_RECOVER);
        Sparkles(play, &at, 16, 255, 160, 255);
        return false;
    }
    hero->actor.colChkInfo.damage = (u8)dmg;
    return true;
}

extern "C" s32 GdfHero_Fallen(Player* hero, PlayState* play) {
    s16 t = ++sH.fallenT;

    if (sH.act != HA_NONE) {
        sH.act = HA_NONE;
        Player_SetModelGroup(hero, PLAYER_MODELGROUP_SWORD_AND_SHIELD);
    }
    hero->meleeWeaponQuads[0].base.atFlags &= ~AT_ON;
    hero->meleeWeaponQuads[1].base.atFlags &= ~AT_ON;
    hero->shieldQuad.base.atFlags &= ~AT_ON;
    hero->cylinder.base.acFlags &= ~AC_ON;
    if (t == 1) {
        GameInteractor_ExecuteOnEnemyDefeat(&hero->actor);
    }
    if (t == 26) {
        Anim(play, hero, gPlayerAnim_link_normal_front_downA, false, -4.0f);
        Audio_PlayActorSound2(&hero->actor, NA_SE_VO_LI_DOWN);
    }
    if (t >= 26) {
        hero->stateFlags3 |= PLAYER_STATE3_PAUSE_ACTION_FUNC;
        hero->linearVelocity = 0.0f;
        LinkAnimation_Update(play, &hero->skelAnime);
    }
    if (t == 50) {
        Audio_PlayActorSound2(&GET_PLAYER(play)->actor, NA_SE_EN_GANON_LAUGH);
    }
    if (t == 70) {
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_BOSS_CLEAR);
    }
    // And the game's ending (as after Ganon: the Chamber of Sages, then the credits)
    if ((t == (s16)Tune("hero.ending_after", 300.0f, 80, 3000)) && Tune("hero.ending", 1.0f, 0, 1) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF)) {
        play->nextEntranceIndex = ENTR_CHAMBER_OF_THE_SAGES_0;
        gSaveContext.nextCutsceneIndex = 0xFFF2;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        play->linkAgeOnLoad = 1;
    }
    if (t > 30000) {
        sH.fallenT = 100;
    }
    return true;
}

extern "C" void GdfHero_Draw(Player* hero, PlayState* play) {
    s32 age = gSaveContext.linkAge;
    u8 bItem = gSaveContext.equips.buttonItems[0];
    bool fog = false;

    OPEN_DISPS(play->state.gfxCtx);
    func_80093C80(play);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    if (hero->invincibilityTimer > 0) {
        hero->damageFlickerAnimCounter = (u8)(hero->damageFlickerAnimCounter + CLAMP(50 - hero->invincibilityTimer, 8, 40));
        POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP, 255, 0, 0, 0, 0,
                                    4000 - (s32)(Math_CosS(hero->damageFlickerAnimCounter * 256) * 2000.0f));
        fog = true;
    } else if (sH.nayru > 0) {
        // Nayru's Love: a blue glow
        POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP, 70, 130, 255, 0, 0,
                                    4300 - (s32)(Math_CosS((s16)(play->gameplayFrames * 0x600)) * 900.0f));
        fog = true;
    }
    func_8002EBCC(&hero->actor, play, 0);
    func_8002ED80(&hero->actor, play, 0);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH);
    // (adult Link's parts and the Master Sword, whatever the player is and has)
    gSaveContext.linkAge = LINK_AGE_ADULT;
    gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;
    Player_DrawGameplay(play, hero, 0, gCullBackDList, Player_OverrideLimbDrawGameplayDefault);
    gSaveContext.equips.buttonItems[0] = bItem;
    gSaveContext.linkAge = age;
    if (fog) {
        POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" s32 GdfHero_OwnsMagic(Actor* magicParent) {
    if (magicParent == nullptr) {
        return false;
    }
    if (GdfHero_Is(magicParent) || (magicParent == sH.din)) {
        return true;
    }
    for (Actor* a : sH.arrows) {
        if (a == magicParent) {
            return true;
        }
    }
    return false;
}

extern "C" u8 GdfHero_DinDamage(void) {
    return (u8)Tune("hero.din_damage", 16.0f, 0, 255);
}

extern "C" s32 GdfHero_GanondorfStunned(void) {
    PlayState* play = gPlayState;

    return (play != nullptr) && ((u32)(play->gameplayFrames - sH.seenFrame) < 3) && (sH.stunT > 0);
}

extern "C" s32 GdfHero_ShieldsGanondorf(PlayState* play) {
    return (play != nullptr) && ((u32)(play->gameplayFrames - sH.seenFrame) < 3) && sH.engaged && GanondorfAloft();
}

extern "C" void GdfHero_StunGanondorf(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f chest = player->actor.world.pos;

    if (!Classic() || (sH.stunT > 0)) {
        return;
    }
    sH.stunT = (s16)Tune("hero.stun_frames", 170.0f, 20, 2000);
    chest.y += 60.0f;
    EffectSsHitMark_SpawnFixedScale(play, 3, &chest);
    Audio_PlayActorSound2(&player->actor, NA_SE_EN_GANON_DAMAGE2);
    PlayAsGanondorf_KnockOutOfAir();
    BossGanon_Player_CsStagger(sH.stunT); // (down on his knees once he lands)
}
