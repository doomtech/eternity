// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//  OpenGL Primitives
//  haleyjd 05/15/11
//
//-----------------------------------------------------------------------------

#ifndef GL_PRIMITIVES_H__
#define GL_PRIMITIVES_H__

#ifdef EE_FEATURE_OPENGL

void GL_OrthoQuadTextured(GLfloat x, GLfloat y, GLfloat w, GLfloat h,
                          GLfloat smax, GLfloat tmax);

void GL_OrthoQuadFlat(GLfloat x, GLfloat y, GLfloat w, GLfloat h,
                      GLfloat r, GLfloat b, GLfloat g);


#endif

#endif

// EOF