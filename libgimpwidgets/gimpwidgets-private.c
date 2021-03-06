/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpwidgets-private.c
 * Copyright (C) 2003 Sven Neumann <sven@gimp.org>
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

#include "config.h"

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "gimpwidgetstypes.h"

#include "gimpstock.h"
#include "gimpwidgets-private.h"

#include "libgimp/libgimp-intl.h"

#include "gimp-wilber-pixbufs.h"


GimpHelpFunc          _gimp_standard_help_func  = NULL;
GimpGetColorFunc      _gimp_get_foreground_func = NULL;
GimpGetColorFunc      _gimp_get_background_func = NULL;
GimpEnsureModulesFunc _gimp_ensure_modules_func = NULL;


static void
gimp_widgets_init_foreign_enums (void)
{
  static const GimpEnumDesc input_mode_descs[] =
  {
    { GDK_MODE_DISABLED, NC_("input-mode", "Disabled"), NULL },
    { GDK_MODE_SCREEN,   NC_("input-mode", "Screen"),   NULL },
    { GDK_MODE_WINDOW,   NC_("input-mode", "Window"),   NULL },
    { 0, NULL, NULL }
  };

  gimp_type_set_translation_domain (GDK_TYPE_INPUT_MODE,
                                    GETTEXT_PACKAGE "-libgimp");
  gimp_type_set_translation_context (GDK_TYPE_INPUT_MODE, "input-mode");
  gimp_enum_set_value_descriptions (GDK_TYPE_INPUT_MODE, input_mode_descs);
}

void
gimp_widgets_init (GimpHelpFunc          standard_help_func,
                   GimpGetColorFunc      get_foreground_func,
                   GimpGetColorFunc      get_background_func,
                   GimpEnsureModulesFunc ensure_modules_func)
{
  static gboolean  gimp_widgets_initialized = FALSE;

  GdkPixbuf *pixbuf;
  GList     *icon_list = NULL;
  gint       i;

  const guint8 *inline_pixbufs[] =
  {
    wilber_64,
    wilber_48,
    wilber_32,
    wilber_16
  };

  g_return_if_fail (standard_help_func != NULL);

  if (gimp_widgets_initialized)
    g_error ("gimp_widgets_init() must only be called once!");

  _gimp_standard_help_func  = standard_help_func;
  _gimp_get_foreground_func = get_foreground_func;
  _gimp_get_background_func = get_background_func;
  _gimp_ensure_modules_func = ensure_modules_func;

  gimp_stock_init ();

  for (i = 0; i < G_N_ELEMENTS (inline_pixbufs); i++)
    {
      pixbuf = gdk_pixbuf_new_from_inline (-1, inline_pixbufs[i], FALSE, NULL);
      icon_list = g_list_prepend (icon_list, pixbuf);
    }

  gtk_window_set_default_icon_list (icon_list);

  g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
  g_list_free (icon_list);

  gimp_widgets_init_foreign_enums ();

  gimp_widgets_initialized = TRUE;
}
