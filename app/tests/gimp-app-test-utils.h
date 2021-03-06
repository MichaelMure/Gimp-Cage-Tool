/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2009 Martin Nordholts <martinn@src.gnome.org>
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

#ifndef  __GIMP_RECTANGLE_SELECT_TOOL_H__
#define  __GIMP_RECTANGLE_SELECT_TOOL_H__


void            gimp_test_utils_set_gimp2_directory  (const gchar *root_env_var,
                                                      const gchar *subdir);
void            gimp_test_utils_setup_menus_dir      (void);
void            gimp_test_utils_create_image         (Gimp        *gimp,
                                                      gint         width,
                                                      gint         height);
void            gimp_test_utils_synthesize_key_event (GtkWidget   *widget,
                                                      guint        keyval);
GimpUIManager * gimp_test_utils_get_ui_manager       (Gimp        *gimp);
GimpImage     * gimp_test_utils_create_image_from_dalog
                                                     (Gimp        *gimp);


#endif /* __GIMP_RECTANGLE_SELECT_TOOL_H__ */
