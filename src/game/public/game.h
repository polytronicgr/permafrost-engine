/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2017-2018 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#ifndef GAME_H
#define GAME_H

#include "../../map/public/map.h"
#include "../../lib/public/kvec.h"
#include "../../lib/public/khash.h"

#include <stdbool.h>
#include <SDL.h>

struct entity;
struct map;
struct tile_desc;
struct tile;

enum cam_mode{
    CAM_MODE_FPS,
    CAM_MODE_RTS
};

typedef kvec_t(struct entity*) pentity_kvec_t;
KHASH_DECLARE(entity, khint32_t, struct entity*)

/*###########################################################################*/
/* GAME GENERAL                                                              */
/*###########################################################################*/

bool G_Init(void);
bool G_NewGameWithMap(const char *dir, const char *pfmap);
bool G_NewGameWithMapString(const char *mapstr);
void G_Shutdown(void);

void G_Update(void);
void G_Render(void);

void G_SetMapRenderMode(enum chunk_render_mode mode);
void G_SetMinimapPos(float x, float y);
bool G_MouseOverMinimap(void);
bool G_MapHeightAtPoint(vec2_t xz, float *out_height);

void G_MakeStaticObjsImpassable(void);

bool G_AddEntity(struct entity *ent);
bool G_RemoveEntity(struct entity *ent);

bool G_ActivateCamera(int idx, enum cam_mode mode);
void G_MoveActiveCamera(vec2_t xz_ground_pos);

bool G_UpdateMinimapChunk(int chunk_r, int chunk_c);
bool G_UpdateChunkMats(int chunk_r, int chunk_c, const char *mats_string);
bool G_UpdateTile(const struct tile_desc *desc, const struct tile *tile);

/*###########################################################################*/
/* GAME SELECTION                                                            */
/*###########################################################################*/

void                  G_Sel_Enable(void);
void                  G_Sel_Disable(void);

void                  G_Sel_Clear(void);
void                  G_Sel_Add(struct entity *ent);
void                  G_Sel_Remove(struct entity *ent);
const pentity_kvec_t *G_Sel_Get(void);

#endif

