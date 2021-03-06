/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpsessioninfo-dock.h
 * Copyright (C) 2001-2007 Michael Natterer <mitch@gimp.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GIMP_SESSION_INFO_DOCK_H__
#define __GIMP_SESSION_INFO_DOCK_H__


/**
 * GimpSessionInfoDock:
 *
 * Contains information about a dock in the interface.
 */
struct _GimpSessionInfoDock
{
  /* Identifier written to/read from sessionrc */
  gchar *identifier;

  /*  list of GimpSessionInfoBook  */
  GList *books;
};

GimpSessionInfoDock * gimp_session_info_dock_new         (const gchar          *identifier);
void                  gimp_session_info_dock_free        (GimpSessionInfoDock  *dock_info);
void                  gimp_session_info_dock_serialize   (GimpConfigWriter     *writer,
                                                          GimpSessionInfoDock  *dock);
GTokenType            gimp_session_info_dock_deserialize (GScanner             *scanner,
                                                          gint                  scope,
                                                          GimpSessionInfoDock **info,
                                                          const gchar          *identifier);
GimpSessionInfoDock * gimp_session_info_dock_from_widget (GimpDock             *dock);
void                  gimp_session_info_dock_restore     (GimpSessionInfoDock  *dock_info,
                                                          GimpDialogFactory    *factory,
                                                          GdkScreen            *screen,
                                                          GimpDockWindow       *dock_window);


#endif  /* __GIMP_SESSION_INFO_DOCK_H__ */
