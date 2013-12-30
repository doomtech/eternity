// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------

#ifndef HU_OVER_H__
#define HU_OVER_H__

struct vfont_t;

// heads up font
void HU_LoadFont();
void HU_WriteText(const char *s, int x, int y);
int  HU_StringWidth(const char *s);
int  HU_StringHeight(const char *s);

extern char    *hud_overfontname;
extern vfont_t *hud_overfont;

// overlay interface
void HU_OverlayDraw();
void HU_ToggleHUD();
void HU_DisableHUD();

extern int hud_overlaystyle;
extern int hud_enabled;
extern int hud_hidestatus;

#endif

// EOF

