/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimptagged.c
 * Copyright (C) 2008  Sven Neumann <sven@gimp.org>
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

#include <glib-object.h>

#include "core-types.h"

#include "gimpmarshal.h"
#include "gimptag.h"
#include "gimptagged.h"


enum
{
  TAG_ADDED,
  TAG_REMOVED,
  LAST_SIGNAL
};


static void  gimp_tagged_base_init (gpointer klass);

static guint gimp_tagged_signals[LAST_SIGNAL] = { 0, };


GType
gimp_tagged_interface_get_type (void)
{
  static GType tagged_iface_type = 0;

  if (! tagged_iface_type)
    {
      const GTypeInfo tagged_iface_info =
      {
        sizeof (GimpTaggedInterface),
        gimp_tagged_base_init,
        (GBaseFinalizeFunc) NULL,
      };

      tagged_iface_type = g_type_register_static (G_TYPE_INTERFACE,
                                                  "GimpTaggedInterface",
                                                  &tagged_iface_info,
                                                  0);
   }

  return tagged_iface_type;
}

static void
gimp_tagged_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      gimp_tagged_signals[TAG_ADDED] =
        g_signal_new ("tag-added",
                      GIMP_TYPE_TAGGED,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GimpTaggedInterface, tag_added),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      GIMP_TYPE_TAG);

      gimp_tagged_signals[TAG_REMOVED] =
        g_signal_new ("tag-removed",
                      GIMP_TYPE_TAGGED,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (GimpTaggedInterface, tag_removed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      GIMP_TYPE_TAG);

      initialized = TRUE;
    }
}

/**
 * gimp_tagged_add_tag:
 * @tagged: an object that implements the %GimpTagged interface
 * @tag:    a %GimpTag
 *
 * Adds @tag to the @tagged object. The GimpTagged::tag-added signal
 * is emitted if and only if the @tag was not already assigned to this
 * object.
 **/
void
gimp_tagged_add_tag (GimpTagged *tagged,
                     GimpTag    *tag)
{
  g_return_if_fail (GIMP_IS_TAGGED (tagged));

  if (GIMP_TAGGED_GET_INTERFACE (tagged)->add_tag (tagged, tag))
    {
      g_signal_emit (tagged, gimp_tagged_signals[TAG_ADDED], 0, tag);
    }
}

/**
 * gimp_tagged_remove_tag:
 * @tagged: an object that implements the %GimpTagged interface
 * @tag:    a %GimpTag
 *
 * Removes @tag from the @tagged object. The GimpTagged::tag-removed
 * signal is emitted if and only if the @tag was actually assigned to
 * this object.
 **/
void
gimp_tagged_remove_tag (GimpTagged *tagged,
                        GimpTag    *tag)
{
  GList        *tag_iter;

  g_return_if_fail (GIMP_IS_TAGGED (tagged));

  for (tag_iter = gimp_tagged_get_tags (tagged); tag_iter;
       tag_iter = g_list_next (tag_iter))
    {
      GimpTag *tag_ref = GIMP_TAG (tag_iter->data);

      if (gimp_tag_equals (tag_ref, tag))
        {
          g_object_ref (tag_ref);
          if (GIMP_TAGGED_GET_INTERFACE (tagged)->remove_tag (tagged, tag_ref))
            {
              g_signal_emit (tagged, gimp_tagged_signals[TAG_REMOVED], 0, tag_ref);
            }
          g_object_unref (tag_ref);
        }
    }
}

/**
 * gimp_tagged_set_tags:
 * @tagged: an object that implements the %GimpTagged interface
 * @tags: a list of tags
 *
 * Sets the list of tags assigned to this object. The passed list of
 * tags is copied and should be freed by the caller.
 **/
void
gimp_tagged_set_tags (GimpTagged *tagged,
                      GList      *tags)
{
  GList *old_tags;
  GList *list;

  g_return_if_fail (GIMP_IS_TAGGED (tagged));

  old_tags = g_list_copy (gimp_tagged_get_tags (tagged));

  for (list = old_tags; list; list = g_list_next (list))
    {
      gimp_tagged_remove_tag (tagged, list->data);
    }

  g_list_free (old_tags);

  for (list = tags; list; list = g_list_next (list))
    {
      g_return_if_fail (GIMP_IS_TAG (list->data));

      gimp_tagged_add_tag (tagged, list->data);
    }
}

/**
 * gimp_tagged_get_tags:
 * @tagged: an object that implements the %GimpTagged interface
 *
 * Returns the list of tags assigned to this object. The returned %GList
 * is owned by the @tagged object and must not be modified or destroyed.
 *
 * Return value: a list of tags
 **/
GList *
gimp_tagged_get_tags (GimpTagged *tagged)
{
  g_return_val_if_fail (GIMP_IS_TAGGED (tagged), NULL);

  return GIMP_TAGGED_GET_INTERFACE (tagged)->get_tags (tagged);
}

/**
 * gimp_tagged_get_identifier:
 * @tagged: an object that implements the %GimpTagged interface
 *
 * Returns an identifier string which uniquely identifies the tagged
 * object. Two different objects must have unique identifiers but may
 * have the same checksum (which will be the case if one object is a
 * copy of the other). The identifier must be the same across
 * sessions, so for example an instance pointer cannot be used as an
 * identifier.
 *
 * Return value: a newly allocated string containing unique identifier
 * of the object. It must be freed using #g_free.
 **/
gchar *
gimp_tagged_get_identifier (GimpTagged *tagged)
{
  g_return_val_if_fail (GIMP_IS_TAGGED (tagged), NULL);

  return GIMP_TAGGED_GET_INTERFACE (tagged)->get_identifier (tagged);
}

/**
 * gimp_tagged_get_checksum:
 * @tagged: an object that implements the %GimpTagged interface
 *
 * Returns the checksum of the @tagged object. It is used to remap the
 * tags for an object for which the identifier has changed, for
 * example if the user has renamed a data file since the last session.
 *
 * If the object does not want to support such remapping (objects not
 * stored in file for example) it can return #NULL.
 *
 * Return value: checksum string if object needs identifier remapping,
 * #NULL otherwise. Returned string must be freed with #g_free().
 **/
gchar *
gimp_tagged_get_checksum (GimpTagged *tagged)
{
  g_return_val_if_fail (GIMP_IS_TAGGED (tagged), FALSE);

  return GIMP_TAGGED_GET_INTERFACE (tagged)->get_checksum (tagged);
}
