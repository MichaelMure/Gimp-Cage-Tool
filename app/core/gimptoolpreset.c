/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
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

#include <gegl.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpconfig/gimpconfig.h"

#include "core-types.h"

#include "gimp.h"
#include "gimptoolinfo.h"
#include "gimptooloptions.h"
#include "gimptoolpreset.h"
#include "gimptoolpreset-load.h"
#include "gimptoolpreset-save.h"

#include "gimp-intl.h"


#define DEFAULT_USE_FG_BG    FALSE
#define DEFAULT_USE_BRUSH    TRUE
#define DEFAULT_USE_DYNAMICS TRUE
#define DEFAULT_USE_GRADIENT TRUE
#define DEFAULT_USE_PATTERN  TRUE
#define DEFAULT_USE_PALETTE  TRUE
#define DEFAULT_USE_FONT     TRUE

enum
{
  PROP_0,
  PROP_NAME,
  PROP_GIMP,
  PROP_TOOL_OPTIONS,
  PROP_USE_FG_BG,
  PROP_USE_BRUSH,
  PROP_USE_DYNAMICS,
  PROP_USE_GRADIENT,
  PROP_USE_PATTERN,
  PROP_USE_PALETTE,
  PROP_USE_FONT
};


static void        gimp_tool_preset_config_iface_init (GimpConfigInterface *iface);

static void            gimp_tool_preset_constructed   (GObject      *object);
static void            gimp_tool_preset_finalize      (GObject      *object);
static void            gimp_tool_preset_set_property  (GObject      *object,
                                                       guint         property_id,
                                                       const GValue *value,
                                                       GParamSpec   *pspec);
static void            gimp_tool_preset_get_property  (GObject      *object,
                                                       guint         property_id,
                                                       GValue       *value,
                                                       GParamSpec   *pspec);
static void
         gimp_tool_preset_dispatch_properties_changed (GObject      *object,
                                                       guint         n_pspecs,
                                                       GParamSpec  **pspecs);

static const gchar   * gimp_tool_preset_get_extension (GimpData     *data);

static gboolean gimp_tool_preset_deserialize_property (GimpConfig   *config,
                                                       guint         property_id,
                                                       GValue       *value,
                                                       GParamSpec   *pspec,
                                                       GScanner     *scanner,
                                                       GTokenType   *expected);


G_DEFINE_TYPE_WITH_CODE (GimpToolPreset, gimp_tool_preset, GIMP_TYPE_DATA,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_CONFIG,
                                                gimp_tool_preset_config_iface_init))

#define parent_class gimp_tool_preset_parent_class


static void
gimp_tool_preset_class_init (GimpToolPresetClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);
  GimpDataClass *data_class   = GIMP_DATA_CLASS (klass);

  object_class->constructed                 = gimp_tool_preset_constructed;
  object_class->finalize                    = gimp_tool_preset_finalize;
  object_class->set_property                = gimp_tool_preset_set_property;
  object_class->get_property                = gimp_tool_preset_get_property;
  object_class->dispatch_properties_changed = gimp_tool_preset_dispatch_properties_changed;

  data_class->save                          = gimp_tool_preset_save;
  data_class->get_extension                 = gimp_tool_preset_get_extension;

  GIMP_CONFIG_INSTALL_PROP_STRING (object_class, PROP_NAME,
                                   "name", NULL,
                                   "Unnamed",
                                   GIMP_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_GIMP,
                                   g_param_spec_object ("gimp",
                                                        NULL, NULL,
                                                        GIMP_TYPE_GIMP,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  GIMP_CONFIG_INSTALL_PROP_OBJECT (object_class, PROP_TOOL_OPTIONS,
                                   "tool-options", NULL,
                                   GIMP_TYPE_TOOL_OPTIONS,
                                   GIMP_PARAM_STATIC_STRINGS);

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_FG_BG,
                                    "use-fg-bg", NULL,
                                    DEFAULT_USE_FG_BG,
                                    GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_BRUSH,
                                    "use-brush", NULL,
                                    DEFAULT_USE_BRUSH,
                                    GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_DYNAMICS,
                                    "use-dynamics", NULL,
                                    DEFAULT_USE_DYNAMICS,
                                    GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_PATTERN,
                                    "use-pattern", NULL,
                                    TRUE,
                                    GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_PALETTE,
                                    "use-palette", NULL,
                                    DEFAULT_USE_PALETTE,
                                    GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_GRADIENT,
                                    "use-gradient", NULL,
                                    DEFAULT_USE_GRADIENT,
                                    GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_USE_FONT,
                                    "use-font", NULL,
                                    DEFAULT_USE_FONT,
                                    GIMP_PARAM_STATIC_STRINGS);
}

static void
gimp_tool_preset_config_iface_init (GimpConfigInterface *iface)
{
  iface->deserialize_property = gimp_tool_preset_deserialize_property;
}

static void
gimp_tool_preset_init (GimpToolPreset *tool_preset)
{
  tool_preset->tool_options = NULL;
}

static void
gimp_tool_preset_constructed (GObject *object)
{
  GimpToolPreset *preset = GIMP_TOOL_PRESET (object);

  if (G_OBJECT_CLASS (parent_class)->constructed)
    G_OBJECT_CLASS (parent_class)->constructed (object);

  g_assert (GIMP_IS_GIMP (preset->gimp));
}

static void
gimp_tool_preset_finalize (GObject *object)
{
  GimpToolPreset *tool_preset = GIMP_TOOL_PRESET (object);

  if (tool_preset->tool_options)
    {
      g_object_unref (tool_preset->tool_options);
      tool_preset->tool_options = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_tool_preset_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GimpToolPreset *tool_preset = GIMP_TOOL_PRESET (object);

  switch (property_id)
    {
    case PROP_NAME:
      gimp_object_set_name (GIMP_OBJECT (tool_preset),
                            g_value_get_string (value));
      break;

    case PROP_GIMP:
      tool_preset->gimp = g_value_get_object (value); /* don't ref */
      break;

    case PROP_TOOL_OPTIONS:
      if (tool_preset->tool_options)
        g_object_unref (tool_preset->tool_options);
      tool_preset->tool_options =
        gimp_config_duplicate (g_value_get_object (value));
      break;

    case PROP_USE_FG_BG:
      tool_preset->use_fg_bg = g_value_get_boolean (value);
      break;
    case PROP_USE_BRUSH:
      tool_preset->use_brush = g_value_get_boolean (value);
      break;
    case PROP_USE_DYNAMICS:
      tool_preset->use_dynamics = g_value_get_boolean (value);
      break;
    case PROP_USE_PATTERN:
      tool_preset->use_pattern = g_value_get_boolean (value);
      break;
    case PROP_USE_PALETTE:
      tool_preset->use_palette = g_value_get_boolean (value);
      break;
    case PROP_USE_GRADIENT:
      tool_preset->use_gradient = g_value_get_boolean (value);
      break;
    case PROP_USE_FONT:
      tool_preset->use_font = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_tool_preset_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GimpToolPreset *tool_preset = GIMP_TOOL_PRESET (object);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, gimp_object_get_name (tool_preset));
      break;

    case PROP_GIMP:
      g_value_set_object (value, tool_preset->gimp);
      break;

    case PROP_TOOL_OPTIONS:
      g_value_set_object (value, tool_preset->tool_options);
      break;

    case PROP_USE_FG_BG:
      g_value_set_boolean (value, tool_preset->use_fg_bg);
      break;
    case PROP_USE_BRUSH:
      g_value_set_boolean (value, tool_preset->use_brush);
      break;
    case PROP_USE_DYNAMICS:
      g_value_set_boolean (value, tool_preset->use_dynamics);
      break;
    case PROP_USE_PATTERN:
      g_value_set_boolean (value, tool_preset->use_pattern);
      break;
    case PROP_USE_PALETTE:
      g_value_set_boolean (value, tool_preset->use_palette);
      break;
    case PROP_USE_GRADIENT:
      g_value_set_boolean (value, tool_preset->use_gradient);
      break;
    case PROP_USE_FONT:
      g_value_set_boolean (value, tool_preset->use_font);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_tool_preset_dispatch_properties_changed (GObject     *object,
                                              guint        n_pspecs,
                                              GParamSpec **pspecs)
{
  gint i;

  G_OBJECT_CLASS (parent_class)->dispatch_properties_changed (object,
                                                              n_pspecs, pspecs);

  for (i = 0; i < n_pspecs; i++)
    {
      if (pspecs[i]->flags & GIMP_CONFIG_PARAM_SERIALIZE)
        {
          gimp_data_dirty (GIMP_DATA (object));
          break;
        }
    }
}

static const gchar *
gimp_tool_preset_get_extension (GimpData *data)
{
  return GIMP_TOOL_PRESET_FILE_EXTENSION;
}

static gboolean
gimp_tool_preset_deserialize_property (GimpConfig *config,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec,
                                       GScanner   *scanner,
                                       GTokenType *expected)
{
  GimpToolPreset *tool_preset = GIMP_TOOL_PRESET (config);

  switch (property_id)
    {
    case PROP_TOOL_OPTIONS:
      {
        GObject *options;
        gchar   *type_name;
        GType    type;

        if (! gimp_scanner_parse_string (scanner, &type_name))
          {
            *expected = G_TOKEN_STRING;
            break;
          }

        type = g_type_from_name (type_name);

        if (! type)
          {
            g_scanner_error (scanner,
                             "unable to determine type of '%s'",
                             type_name);
            *expected = G_TOKEN_STRING;
            g_free (type_name);
            break;
          }

        if (! g_type_is_a (type, GIMP_TYPE_TOOL_OPTIONS))
          {
            g_scanner_error (scanner,
                             "'%s' is not a subclass of GimpToolOptions",
                             type_name);
            *expected = G_TOKEN_STRING;
            g_free (type_name);
            break;
          }

        g_free (type_name);

        options = g_object_new (type,
                                "gimp", tool_preset->gimp,
                                NULL);

        if (! GIMP_CONFIG_GET_INTERFACE (options)->deserialize (GIMP_CONFIG (options),
                                                                scanner, 1,
                                                                NULL))
          {
            g_object_unref (options);
            break;
          }

        /* this is a hack */
        g_object_set (options,
                      "tool-info", gimp_context_get_tool (GIMP_CONTEXT (options)),
                      NULL);

        g_value_take_object (value, options);
      }
      break;

    default:
      return FALSE;
    }

  return TRUE;
}


/*  public functions  */

GimpData *
gimp_tool_preset_new (GimpContext *context,
                      const gchar *name)
{
  GimpToolInfo *tool_info;
  const gchar  *stock_id;

  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (name[0] != '\0', NULL);

  tool_info = gimp_context_get_tool (context);

  g_return_val_if_fail (tool_info != NULL, NULL);

  stock_id = gimp_viewable_get_stock_id (GIMP_VIEWABLE (tool_info));

  return g_object_new (GIMP_TYPE_TOOL_PRESET,
                       "name",         name,
                       "stock-id",     stock_id,
                       "gimp",         context->gimp,
                       "tool-options", tool_info->tool_options,
                       NULL);
}

GimpContextPropMask
gimp_tool_preset_get_prop_mask (GimpToolPreset *preset)
{
  GimpContextPropMask  use_props = 0;

  g_return_val_if_fail (GIMP_IS_TOOL_PRESET (preset), 0);

  if (preset->use_fg_bg)
    {
      use_props |= GIMP_CONTEXT_FOREGROUND_MASK;
      use_props |= GIMP_CONTEXT_BACKGROUND_MASK;
    }
  if (preset->use_brush)
    use_props |= GIMP_CONTEXT_BRUSH_MASK;
  if (preset->use_dynamics)
    use_props |= GIMP_CONTEXT_DYNAMICS_MASK;
  if (preset->use_pattern)
    use_props |= GIMP_CONTEXT_PATTERN_MASK;
  if (preset->use_palette)
    use_props |= GIMP_CONTEXT_PALETTE_MASK;
  if (preset->use_gradient)
    use_props |= GIMP_CONTEXT_GRADIENT_MASK;
  if (preset->use_font)
    use_props |= GIMP_CONTEXT_FONT_MASK;

  return use_props;
}
