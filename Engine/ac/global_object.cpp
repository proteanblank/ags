//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-2025 various contributors
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// https://opensource.org/license/artistic-2-0/
//
//=============================================================================
#include <algorithm>
#include <stdio.h>
#include "ac/global_object.h"
#include "ac/common.h"
#include "ac/object.h"
#include "ac/view.h"
#include "ac/character.h"
#include "ac/draw.h"
#include "ac/event.h"
#include "ac/game.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/global_character.h"
#include "ac/global_translation.h"
#include "ac/object.h"
#include "ac/properties.h"
#include "ac/roomobject.h"
#include "ac/roomstatus.h"
#include "ac/string.h"
#include "ac/viewframe.h"
#include "ac/dynobj/cc_object.h"
#include "debug/debug_log.h"
#include "main/game_run.h"
#include "script/script.h"
#include "ac/spritecache.h"
#include "gfx/graphicsdriver.h"
#include "gfx/bitmap.h"
#include "gfx/gfx_def.h"

using namespace AGS::Common;
using namespace AGS::Engine;

#define OVERLAPPING_OBJECT 1000

extern RoomStatus*croom;
extern RoomObject*objs;
extern std::vector<ViewStruct> views;
extern GameSetupStruct game;
extern RoomStruct thisroom;
extern CharacterInfo*playerchar;
extern int displayed_room;
extern SpriteCache spriteset;
extern IGraphicsDriver *gfxDriver;
extern ScriptObject scrObj[MAX_ROOM_OBJECTS];
extern CCObject ccDynamicObject;

// Used for deciding whether a char or obj was closer
int obj_lowest_yp;

int GetObjectIDAtScreen(int scrx, int scry)
{
    // translate screen co-ordinates to room co-ordinates
    VpPoint vpt = play.ScreenToRoomDivDown(scrx, scry);
    if (vpt.second < 0)
        return -1;
    return GetObjectIDAtRoom(vpt.first.X, vpt.first.Y);
}

int GetObjectIDAtRoom(int roomx, int roomy)
{
    int bestshotyp=-1,bestshotwas=-1;
    // Iterate through all objects in the room
    for (uint32_t aa=0;aa<croom->numobj;aa++) {
        if (objs[aa].on != 1) continue;
        if (objs[aa].flags & OBJF_NOINTERACT)
            continue;
        int xxx=objs[aa].x,yyy=objs[aa].y;
        int isflipped = 0;
        int spWidth = game_to_data_coord(objs[aa].get_width());
        int spHeight = game_to_data_coord(objs[aa].get_height());
        if (objs[aa].view != RoomObject::NoView)
            isflipped = views[objs[aa].view].loops[objs[aa].loop].frames[objs[aa].frame].flags & VFLG_FLIPSPRITE;

        bool is_original;
        Bitmap *theImage = GetObjectImage(aa, &is_original);
        if (!is_original)
            isflipped = 0; // transformed image is already flipped

        if (is_pos_in_sprite(roomx, roomy, xxx, yyy - spHeight, theImage,
            spWidth, spHeight, isflipped, is_original) == FALSE)
            continue;

        int usebasel = objs[aa].get_baseline();   
        if (usebasel < bestshotyp) continue;

        bestshotwas = aa;
        bestshotyp = usebasel;
    }
    obj_lowest_yp = bestshotyp;
    return bestshotwas;
}

void SetObjectTint(int obj, int red, int green, int blue, int opacity, int luminance) {
    if ((red < 0) || (green < 0) || (blue < 0) ||
        (red > 255) || (green > 255) || (blue > 255) ||
        (opacity < 0) || (opacity > 100) ||
        (luminance < 0) || (luminance > 100))
        quit("!SetObjectTint: invalid parameter. R,G,B must be 0-255, opacity & luminance 0-100");

    if (!is_valid_object(obj))
        quit("!SetObjectTint: invalid object number specified");

    debug_script_log("Set object %d tint RGB(%d,%d,%d) %d%%", obj, red, green, blue, opacity);

    objs[obj].tint_r = red;
    objs[obj].tint_g = green;
    objs[obj].tint_b = blue;
    objs[obj].tint_level = opacity;
    objs[obj].tint_light = (luminance * 25) / 10;
    objs[obj].flags &= ~OBJF_HASLIGHT;
    objs[obj].flags |= OBJF_HASTINT;
}

void RemoveObjectTint(int obj) {
    if (!is_valid_object(obj))
        quit("!RemoveObjectTint: invalid object");

    if (objs[obj].flags & (OBJF_HASTINT | OBJF_HASLIGHT)) {
        debug_script_log("Un-tint object %d", obj);
        objs[obj].flags &= ~(OBJF_HASTINT | OBJF_HASLIGHT);
    }
    else {
        debug_script_warn("RemoveObjectTint called but object was not tinted");
    }
}

void SetObjectView(int obn, int vii) {
    // According to the old AGS manual, the loop and frame should be both reset to 0
    SetObjectFrameSimple(obn, vii, 0, 0);
    debug_script_log("Object %d set to view %d", obn, vii);
}

bool SetObjectFrameSimple(int obn, int viw, int lop, int fra) {
    if (!is_valid_object(obn))
        quitprintf("!SetObjectFrame: invalid object number specified (%d, range is 0 - %d)", obn, 0, croom->numobj);
    viw--;
    AssertViewHasLoops("SetObjectFrame", viw);

    auto &obj = objs[obn];
    // Previous version of Object.SetView had negative loop and frame mean "use latest values",
    // which also caused SetObjectFrame to act similarly, starting with 2.70.
    if ((game.options[OPT_BASESCRIPTAPI] < kScriptAPI_v360) &&
        (loaded_game_file_version >= kGameVersion_270))
    {
        if (lop < 0) lop = obj.loop;
        if (fra < 0) fra = obj.frame;
    }

    // Fixup invalid loop & frame numbers by using default 0 value
    if (lop < 0 || lop >= views[viw].numLoops)
    {
        debug_script_warn("SetObjectFrame: invalid loop number used for view %d (%d, range is 0 - %d)",
            viw, lop, views[viw].numLoops - 1);
        lop = 0;
    }
    if (fra < 0 || fra >= views[viw].loops[lop].numFrames)
    {
        debug_script_warn("SetObjectFrame: frame index out of range (%d, must be 0 - %d)", fra, views[viw].loops[lop].numFrames - 1);
        fra = 0; // NOTE: we have 1 dummy frame allocated for empty loops
    }

    // Current engine's object data limitation by uint16_t
    if (viw > UINT16_MAX || lop > UINT16_MAX || fra > UINT16_MAX)
    {
        debug_script_warn("Warning: object's (id %d) view/loop/frame (%d/%d/%d) is outside of internal range (%d/%d/%d), reset to no view",
            obn, viw + 1, lop, fra, UINT16_MAX + 1, UINT16_MAX, UINT16_MAX);
        SetObjectGraphic(obn, 0);
        return false;
    }

    obj.view = static_cast<uint16_t>(viw);
    obj.loop = static_cast<uint16_t>(lop);
    obj.frame = static_cast<uint16_t>(fra);
    obj.cycling = 0; // reset anim
    int pic = views[viw].loops[lop].frames[fra].pic;
    obj.num = Math::InRangeOrDef<uint16_t>(pic, 0);
    if (pic > UINT16_MAX)
        debug_script_warn("Warning: object's (id %d) sprite %d is outside of internal range (%d), reset to 0", obn, pic, UINT16_MAX);
    return true;
}

void SetObjectFrame(int obn, int viw, int lop, int fra) {
    if (!SetObjectFrameSimple(obn, viw, lop, fra))
        return;
    objs[obn].CheckViewFrame();
}

// pass trans=0 for fully solid, trans=100 for fully transparent
void SetObjectTransparency(int obn,int trans) {
    if (!is_valid_object(obn)) quit("!SetObjectTransparent: invalid object number specified");
    if ((trans < 0) || (trans > 100)) quit("!SetObjectTransparent: transparency value must be between 0 and 100");

    objs[obn].transparent = GfxDef::Trans100ToLegacyTrans255(trans);
}



void SetObjectBaseline (int obn, int basel) {
    if (!is_valid_object(obn)) quit("!SetObjectBaseline: invalid object number specified");
    // baseline has changed, invalidate the cache
    if (objs[obn].baseline != basel) {
        objs[obn].baseline = basel;
        mark_object_changed(obn);
    }
}

int GetObjectBaseline(int obn) {
    if (!is_valid_object(obn)) quit("!GetObjectBaseline: invalid object number specified");

    if (objs[obn].baseline < 1)
        return 0;

    return objs[obn].baseline;
}

void AnimateObjectImpl(int obn, int loopn, int spdd, int rept, int direction, int blocking,
    int sframe, int volume)
{
    if (!is_valid_object(obn))
        quit("!AnimateObject: invalid object number specified");

    RoomObject &obj = objs[obn];

    if (obj.view == RoomObject::NoView)
        quit("!AnimateObject: object has not been assigned a view");

    ValidateViewAnimVLF("Object.Animate", obj.view, loopn, sframe);
    ValidateViewAnimParams("Object.Animate", rept, blocking, direction);

    if (loopn > UINT16_MAX || sframe > UINT16_MAX)
    {
        debug_script_warn("Warning: object's (id %d) loop/frame (%d/%d) is outside of internal range (%d/%d), cancel animation",
            obn, loopn, sframe, UINT16_MAX, UINT16_MAX);
        return;
    }

    debug_script_log("Obj %d start anim view %d loop %d, speed %d, repeat %d, frame %d",
        obn, obj.view + 1, loopn, spdd, rept, sframe);

    obj.set_animating(rept, direction == 0, spdd);
    obj.loop = (uint16_t)loopn;
    obj.frame = (uint16_t)SetFirstAnimFrame(obj.view, loopn, sframe, direction);
    obj.wait = spdd+views[obj.view].loops[loopn].frames[obj.frame].speed;
    int pic = views[obj.view].loops[loopn].frames[obj.frame].pic;
    obj.num = Math::InRangeOrDef<uint16_t>(pic, 0);
    if (pic > UINT16_MAX)
        debug_script_warn("Warning: object's (id %d) sprite %d is outside of internal range (%d), reset to 0", obn, pic, UINT16_MAX);

    obj.cur_anim_volume = Math::Clamp(volume, 0, 100);


    objs[obn].CheckViewFrame();

    if (blocking)
        GameLoopUntilValueIsZero(&obj.cycling);
}

// A legacy variant of AnimateObject implementation: for pre-2.72 scripts;
// it has a quirk: for IDs >= 100 this actually calls AnimateCharacter(ID - 100)
static void LegacyAnimateObjectImpl(int obn, int loopn, int spdd, int rept,
                                    int direction, int blocking) {
    if (obn >= LEGACY_ANIMATE_CHARIDBASE) {
        AnimateCharacter4(obn - LEGACY_ANIMATE_CHARIDBASE, loopn, spdd, rept);
    } else {
        AnimateObjectImpl(obn, loopn, spdd, rept, direction, blocking, 0 /* first frame */, 100 /* full volume */);
    }
}

void AnimateObject6(int obn, int loopn, int spdd, int rept, int direction, int blocking) {
    LegacyAnimateObjectImpl(obn, loopn, spdd, rept, direction, blocking);
}

void AnimateObject4(int obn, int loopn, int spdd, int rept) {
    LegacyAnimateObjectImpl(obn, loopn, spdd, rept, 0 /* forward */, 0 /* non-blocking */);
}

void MergeObject(int obn) {
    if (!is_valid_object(obn)) quit("!MergeObject: invalid object specified");

    update_object_scale(obn); // make sure sprite transform is up to date
    construct_object_gfx(obn, true);
    Bitmap *actsp = get_cached_object_image(obn);

    PBitmap bg_frame = thisroom.BgFrames[play.bg_frame].Graphic;
    if (bg_frame->GetColorDepth() != actsp->GetColorDepth())
        quit("!MergeObject: unable to merge object due to color depth differences");

    int xpos = data_to_game_coord(objs[obn].x);
    int ypos = (data_to_game_coord(objs[obn].y) - objs[obn].last_height);

    draw_sprite_support_alpha(bg_frame.get(), false, xpos, ypos, actsp, (game.SpriteInfos[objs[obn].num].Flags & SPF_ALPHACHANNEL) != 0);
    invalidate_screen();
    mark_current_background_dirty();

    //abuf = oldabuf;
    // mark the sprite as merged
    objs[obn].on = 2;
    debug_script_log("Object %d merged into background", obn);
}

void StopObjectMoving(int objj) {
    if (!is_valid_object(objj))
        quit("!StopObjectMoving: invalid object number");
    objs[objj].moving = 0;

    debug_script_log("Object %d stop moving", objj);
}

void ObjectOff(int obn) {
    if (!is_valid_object(obn)) quit("!ObjectOff: invalid object specified");
    // don't change it if on == 2 (merged)
    if (objs[obn].on == 1) {
        objs[obn].on = 0;
        debug_script_log("Object %d turned off", obn);
        StopObjectMoving(obn);
    }
}

void ObjectOn(int obn) {
    if (!is_valid_object(obn)) quit("!ObjectOn: invalid object specified");
    if (objs[obn].on == 0) {
        objs[obn].on = 1;
        debug_script_log("Object %d turned on", obn);
    }
}

int IsObjectOn (int objj) {
    if (!is_valid_object(objj)) quit("!IsObjectOn: invalid object number");

    // ==1 is on, ==2 is merged into background
    if (objs[objj].on == 1)
        return 1;

    return 0;
}

void SetObjectGraphic(int obn,int slott) {
    if (!is_valid_object(obn)) quit("!SetObjectGraphic: invalid object specified");

    if (objs[obn].num != slott) {
        objs[obn].num = Math::InRangeOrDef<uint16_t>(slott, 0);
        if (slott > UINT16_MAX)
            debug_script_warn("Warning: object's (id %d) sprite %d is outside of internal range (%d), reset to 0", obn, slott, UINT16_MAX);
        debug_script_log("Object %d graphic changed to slot %d", obn, slott);
    }
    objs[obn].cycling=0;
    objs[obn].frame = 0;
    objs[obn].loop = 0;
    objs[obn].view = RoomObject::NoView;
}

int GetObjectGraphic(int obn) {
    if (!is_valid_object(obn)) quit("!GetObjectGraphic: invalid object specified");
    return objs[obn].num;
}

int GetObjectY (int objj) {
    if (!is_valid_object(objj)) quit("!GetObjectY: invalid object number");
    return objs[objj].y;
}

int IsObjectAnimating(int objj) {
    if (!is_valid_object(objj)) quit("!IsObjectAnimating: invalid object number");
    return (objs[objj].cycling != 0) ? 1 : 0;
}

int IsObjectMoving(int objj) {
    if (!is_valid_object(objj)) quit("!IsObjectMoving: invalid object number");
    return (objs[objj].moving > 0) ? 1 : 0;
}

void SetObjectPosition(int objj, int tox, int toy) {
    if (!is_valid_object(objj))
        quit("!SetObjectPosition: invalid object number");

    if (objs[objj].moving > 0)
    {
        debug_script_warn("Object.SetPosition: cannot set position while object is moving");
        return;
    }

    objs[objj].x = tox;
    objs[objj].y = toy;
}

void GetObjectName(int obj, char *buffer) {
    VALIDATE_STRING(buffer);
    if (!is_valid_object(obj))
        quit("!GetObjectName: invalid object number");

    snprintf(buffer, MAX_MAXSTRLEN, "%s", get_translation(croom->obj[obj].name.GetCStr()));
}

void MoveObject(int objj,int xx,int yy,int spp) {
    move_object(objj,xx,yy,spp,0);
}
void MoveObjectDirect(int objj,int xx,int yy,int spp) {
    move_object(objj,xx,yy,spp,1);
}

void SetObjectClickable (int cha, int clik) {
    if (!is_valid_object(cha))
        quit("!SetObjectClickable: Invalid object specified");
    objs[cha].flags&=~OBJF_NOINTERACT;
    if (clik == 0)
        objs[cha].flags|=OBJF_NOINTERACT;
}

void SetObjectIgnoreWalkbehinds (int cha, int clik) {
    if (!is_valid_object(cha))
        quit("!SetObjectIgnoreWalkbehinds: Invalid object specified");
    if (game.options[OPT_BASESCRIPTAPI] >= kScriptAPI_v350)
        debug_script_warn("IgnoreWalkbehinds is not recommended for use, consider other solutions");
    objs[cha].flags&=~OBJF_NOWALKBEHINDS;
    if (clik)
        objs[cha].flags|=OBJF_NOWALKBEHINDS;
    mark_object_changed(cha);
}

void RunObjectInteraction (int aa, int mood) {
    if (!is_valid_object(aa))
        quit("!RunObjectInteraction: invalid object number for current room");

    // convert cursor mode to event index (in character event table)
    // TODO: probably move this conversion table elsewhere? should be a global info
    int evnt;
    switch (mood)
    {
    case MODE_LOOK: evnt = 0; break;
    case MODE_HAND: evnt = 1; break;
    case MODE_TALK: evnt = 2; break;
    case MODE_USE: evnt = 3; break;
    case MODE_PICKUP: evnt = 5; break;
    case MODE_CUSTOM1: evnt = 6; break;
    case MODE_CUSTOM2: evnt = 7; break;
    default: evnt = -1; break;
    }
    const int anyclick_evt = 4; // TODO: make global constant (character any-click evt)

    // For USE verb: remember active inventory
    if (mood == MODE_USE)
    {
        play.usedinv = playerchar->activeinv;
    }

    const auto obj_evt = ObjectEvent(kScTypeRoom, "object%d", aa,
        RuntimeScriptValue().SetScriptObject(&scrObj[aa], &ccDynamicObject), mood);
    if (loaded_game_file_version > kGameVersion_272)
    {
        if ((evnt >= 0) &&
                run_interaction_script(obj_evt, thisroom.Objects[aa].EventHandlers.get(), evnt, anyclick_evt) < 0)
            return; // game state changed, don't do "any click"
        run_interaction_script(obj_evt, thisroom.Objects[aa].EventHandlers.get(), anyclick_evt); // any click on obj
    }
    else
    {
        if ((evnt >= 0) &&
                run_interaction_event(obj_evt, &croom->intrObject[aa], evnt, anyclick_evt, (mood == MODE_USE)) < 0)
            return; // game state changed, don't do "any click"
        run_interaction_event(obj_evt, &croom->intrObject[aa], anyclick_evt); // any click on obj
    }
}

int AreObjectsColliding(int obj1,int obj2) {
    if (!is_valid_object(obj1) || !is_valid_object(obj2))
        quit("!AreObjectsColliding: invalid object specified");

    return (AreThingsOverlapping(obj1 + OVERLAPPING_OBJECT, obj2 + OVERLAPPING_OBJECT)) ? 1 : 0;
}

static int GetThingRect(int thing, Rect *rect)
{
    if (is_valid_character(thing))
    {
        if (game.chars[thing].room != displayed_room)
            return 0;

        const int charw = game_to_data_coord(GetCharacterWidth(thing));
        const int charh = game_to_data_coord(GetCharacterHeight(thing));
        *rect = RectWH(
            game.chars[thing].x - (charw / 2),
            charextra[thing].GetEffectiveY(&game.chars[thing]) - charh + 1,
            charw, charh);
    }
    else if (is_valid_object(thing - OVERLAPPING_OBJECT))
    {
        int objid = thing - OVERLAPPING_OBJECT;
        if (objs[objid].on != 1)
            return 0;

        const int objw = game_to_data_coord(objs[objid].get_width());
        const int objh = game_to_data_coord(objs[objid].get_height());
        *rect = RectWH(objs[objid].x, objs[objid].y - objh + 1, objw, objh);
    }
    else
    {
        quit("!AreThingsOverlapping: invalid parameter");
    }

    return 1;
}

int AreThingsOverlapping(int thing1, int thing2)
{
    Rect r1, r2;
    // get the bounding rectangles, and return 0 if the object/char
    // is currently turned off
    if (GetThingRect(thing1, &r1) == 0)
        return 0;
    if (GetThingRect(thing2, &r2) == 0)
        return 0;

    if (AreRectsIntersecting(r1, r2))
    {
        // determine how far apart they are
        // take the smaller of the X distances as the overlapping amount
        int xdist = abs(r1.Right - r2.Left) + 1;
        if (abs(r1.Left - r2.Right) < xdist)
            xdist = abs(r1.Left - r2.Right) + 1;
        // take the smaller of the Y distances
        int ydist = abs(r1.Bottom - r2.Top) + 1;
        if (abs(r1.Top - r2.Bottom) < ydist)
            ydist = abs(r1.Top - r2.Bottom) + 1;
        // the overlapping amount is the smaller of the X and Y ovrlap
        if (xdist < ydist)
            return xdist;
        else
            return ydist;
    }
    return 0;
}

int GetObjectProperty (int hss, const char *property)
{
    if (!is_valid_object(hss))
        quit("!GetObjectProperty: invalid object");
    return get_int_property(thisroom.Objects[hss].Properties, croom->objProps[hss], property);
}

void GetObjectPropertyText (int item, const char *property, char *bufer)
{
    if (!AssertObject("GetObjectPropertyText", item))
        return;
    get_text_property(thisroom.Objects[item].Properties, croom->objProps[item], property, bufer);
}

Bitmap *GetObjectImage(int obj, bool *is_original)
{
    // NOTE: the cached image will only be present in software render mode
    Bitmap *actsp = get_cached_object_image(obj);
    if (is_original)
        *is_original = !actsp; // no cached means we use original sprite
    if (actsp)
        return actsp;

    return spriteset[objs[obj].num];
}
