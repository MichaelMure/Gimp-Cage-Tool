/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2003 Peter Mattis and Spencer Kimball
 *
 * gimpparasite_pdb.h
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

/* NOTE: This file is auto-generated by pdbgen.pl */

#ifndef __GIMP_PARASITE_PDB_H__
#define __GIMP_PARASITE_PDB_H__

G_BEGIN_DECLS

/* For information look into the C source or the html documentation */


GimpParasite* gimp_parasite_find         (const gchar          *name);
gboolean      gimp_parasite_attach       (const GimpParasite   *parasite);
gboolean      gimp_parasite_detach       (const gchar          *name);
gboolean      gimp_parasite_list         (gint                 *num_parasites,
                                          gchar              ***parasites);
GimpParasite* gimp_image_parasite_find   (gint32                image_ID,
                                          const gchar          *name);
gboolean      gimp_image_parasite_attach (gint32                image_ID,
                                          const GimpParasite   *parasite);
gboolean      gimp_image_parasite_detach (gint32                image_ID,
                                          const gchar          *name);
gboolean      gimp_image_parasite_list   (gint32                image_ID,
                                          gint                 *num_parasites,
                                          gchar              ***parasites);


G_END_DECLS

#endif /* __GIMP_PARASITE_PDB_H__ */
