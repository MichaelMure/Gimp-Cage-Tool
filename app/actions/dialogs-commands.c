/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#include "config.h"

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "actions-types.h"

#include "widgets/gimpdialogfactory.h"

#include "actions.h"
#include "dialogs-commands.h"


/*  public functions  */

void
dialogs_create_toplevel_cmd_callback (GtkAction   *action,
                                      const gchar *value,
                                      gpointer     data)
{
  GtkWidget *widget;
  return_if_no_widget (widget, data);

  if (value)
    gimp_dialog_factory_dialog_new (gimp_dialog_factory_get_singleton (),
                                    gtk_widget_get_screen (widget),
                                    NULL /*ui_manager*/,
                                    value, -1, TRUE);
}

void
dialogs_create_dockable_cmd_callback (GtkAction   *action,
                                      const gchar *value,
                                      gpointer     data)
{
  GtkWidget *widget;
  return_if_no_widget (widget, data);

  if (value)
    gimp_dialog_factory_dialog_raise (gimp_dialog_factory_get_singleton (),
                                      gtk_widget_get_screen (widget),
                                      value, -1);
}
