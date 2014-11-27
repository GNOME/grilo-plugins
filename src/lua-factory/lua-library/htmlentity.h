/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* htmlentity.c
 *
 * This file is part of the GtkHTML library.
 *
 * Copyright (C) 1999  Helix Code, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef _HTMLENTITY_H
#define _HTMLENTITY_H

#include <glib.h>

/* We name it with correct unicode name, but OK, later... Lauris */
/* gchar used for &nbsp; - must correspond to table below */
#define ENTITY_NBSP 160
#define INVALID_ENTITY_CHARACTER_MARKER '?'
#define IS_UTF8_NBSP(s) (*s == (guchar)0xc2 && *(s + 1) == (guchar)0xa0)

gulong html_entity_parse (const gchar *s, guint len);

#if 0
/* We do not need that - 0x160 is valid unicode character after all... Lauris */
/* prepares text to draw/get_width, returned text is allocated using g_strdup so it could be g_free'ed */
gchar * html_entity_prepare (const gchar *s);
#endif

#endif
