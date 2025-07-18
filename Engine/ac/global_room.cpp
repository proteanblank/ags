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
#include "ac/global_room.h"
#include "ac/common.h"
#include "ac/character.h"
#include "ac/characterinfo.h"
#include "ac/dialog.h"
#include "ac/draw.h"
#include "ac/event.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/global_game.h"
#include "ac/movelist.h"
#include "ac/properties.h"
#include "ac/room.h"
#include "ac/roomstatus.h"
#include "debug/debug_log.h"
#include "debug/debugger.h"
#include "script/script.h"

using namespace Common;

extern GameSetupStruct game;
extern RoomStatus *croom;
extern CharacterInfo*playerchar;
extern int displayed_room;
extern bool in_enters_screen;
extern int in_leaves_screen;
extern int in_inv_screen, inv_screen_newroom;
extern int gs_to_newroom;
extern bool new_room_placeonwalkable;
extern RoomStruct thisroom;

void SetAmbientTint (int red, int green, int blue, int opacity, int luminance) {
    if ((red < 0) || (green < 0) || (blue < 0) ||
        (red > 255) || (green > 255) || (blue > 255) ||
        (opacity < 0) || (opacity > 100) ||
        (luminance < 0) || (luminance > 100))
        quit("!SetTint: invalid parameter. R,G,B must be 0-255, opacity & luminance 0-100");

    debug_script_log("Set ambient tint RGB(%d,%d,%d) %d%%", red, green, blue, opacity);

    play.rtint_enabled = opacity > 0;
    play.rtint_red = red;
    play.rtint_green = green;
    play.rtint_blue = blue;
    play.rtint_level = opacity;
    play.rtint_light = (luminance * 25) / 10;
}

void SetAmbientLightLevel(int light_level)
{
    light_level = Math::Clamp(light_level, -100, 100);

    play.rtint_enabled = light_level != 0;
    play.rtint_level = 0;
    play.rtint_light = light_level;
}

void NewRoom(int nrnum) {
    if (nrnum < 0)
        quitprintf("!NewRoom: room change requested to invalid room number %d.", nrnum);

    if (displayed_room < 0) {
        // called from game_start; change the room where the game will start
        playerchar->room = nrnum;
        return;
    }


    debug_script_log("Room change requested to room %d", nrnum);
    EndSkippingUntilCharStops();

    can_run_delayed_command();

    if (handle_state_change_in_dialog_request("NewRoom", DIALOG_NEWROOM + nrnum))
        return; // handled

    if (in_leaves_screen >= 0) {
        // NewRoom called from the Player Leaves Screen event -- just
        // change which room it will go to
        in_leaves_screen = nrnum;
    }
    else if (in_enters_screen) {
        setevent(AGSEvent_NewRoom(nrnum));
        return;
    }
    else if (in_inv_screen) {
        inv_screen_newroom = nrnum;
        return;
    }
    else if ((inside_script==0) & (in_graph_script==0)) {
        // Compatibility: old games had a *possibly unintentional* effect:
        // if a character was walking, and a "change room" is called
        // *NOT* from a script, but by some other trigger,
        // they ended up forced to a walkable area in the next room.
        if (loaded_game_file_version < kGameVersion_300)
        {
            new_room_placeonwalkable = is_char_walking_ndirect(playerchar);
        }

        new_room(nrnum,playerchar);
        return;
    }
    else if (inside_script) {
        get_executingscript()->QueueAction(PostScriptAction(ePSANewRoom, nrnum, "NewRoom"));
        // we might be within a MoveCharacterBlocking -- the room
        // change should abort it
        if (is_char_walking_ndirect(playerchar)) {
            // stop the character, but doesn't fix the character to a walkable area
            Character_StopMovingEx(playerchar, false);
        }
    }
    else if (in_graph_script)
        gs_to_newroom = nrnum;
}


void NewRoomEx(int nrnum,int newx,int newy) {
    Character_ChangeRoom(playerchar, nrnum, newx, newy);
}

void NewRoomNPC(int charid, int nrnum, int newx, int newy) {
    if (!is_valid_character(charid))
        quit("!NewRoomNPC: invalid character");
    if (charid == game.playercharacter)
        quit("!NewRoomNPC: use NewRoomEx with the player character");

    Character_ChangeRoom(&game.chars[charid], nrnum, newx, newy);
}

void ResetRoom(int nrnum) {
    if (nrnum == displayed_room)
        quit("!ResetRoom: cannot reset current room");
    if ((nrnum<0) | (nrnum>=MAX_ROOMS))
        quit("!ResetRoom: invalid room number");

    if (isRoomStatusValid(nrnum))
    {
        RoomStatus* roomstat = getRoomStatus(nrnum);
        roomstat->FreeScriptData();
        roomstat->FreeProperties();
        roomstat->beenhere = 0;
    }

    debug_script_log("Room %d reset to original state", nrnum);
}

int HasPlayerBeenInRoom(int roomnum) {
    if ((roomnum < 0) || (roomnum >= MAX_ROOMS))
        return 0;
    if (isRoomStatusValid(roomnum))
        return getRoomStatus(roomnum)->beenhere;
    else
        return 0;
}

void CallRoomScript (int value) {
    can_run_delayed_command();

    if (!inside_script)
        quit("!CallRoomScript: not inside a script???");

    play.roomscript_finished = 0;
    RuntimeScriptValue params[]{ value , RuntimeScriptValue() };
    get_executingscript()->RunAnother(kScTypeRoom, "on_call", 1, params);
}

int HasBeenToRoom (int roomnum) {
    if ((roomnum < 0) || (roomnum >= MAX_ROOMS))
        quit("!HasBeenToRoom: invalid room number specified");

    if (isRoomStatusValid(roomnum))
        return getRoomStatus(roomnum)->beenhere;
    else
        return 0;
}

void GetRoomPropertyText (const char *property, char *bufer)
{
    get_text_property(thisroom.Properties, croom->roomProps, property, bufer);
}

void SetBackgroundFrame(int frnum) {
    if ((frnum < -1) || (frnum != -1 && (size_t)frnum >= thisroom.BgFrameCount))
    {
        debug_script_warn("SetBackgrondFrame: invalid background number specified: %d, valid range in this room is 0..%u", thisroom.BgFrameCount - 1);
        return;
    }

    if (frnum<0) {
        play.bg_frame_locked=0;
        return;
    }

    play.bg_frame_locked = 1;

    if (frnum == play.bg_frame)
    {
        // already on this frame, do nothing
        return;
    }

    play.bg_frame = frnum;
    on_background_frame_change ();
}

int GetBackgroundFrame() {
    return play.bg_frame;
}
