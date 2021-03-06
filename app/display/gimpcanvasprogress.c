/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcanvasprogress.c
 * Copyright (C) 2010 Michael Natterer <mitch@gimp.org>
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
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"

#include "display-types.h"

#include "core/gimpprogress.h"

#include "gimpcanvas.h"
#include "gimpcanvasitem-utils.h"
#include "gimpcanvasprogress.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-transform.h"
#include "gimpdisplayshell-style.h"


#define BORDER   5
#define RADIUS  20


enum
{
  PROP_0,
  PROP_ANCHOR,
  PROP_X,
  PROP_Y
};


typedef struct _GimpCanvasProgressPrivate GimpCanvasProgressPrivate;

struct _GimpCanvasProgressPrivate
{
  GimpHandleAnchor  anchor;
  gdouble           x;
  gdouble           y;

  gchar            *text;
  gdouble           value;
};

#define GET_PRIVATE(progress) \
        G_TYPE_INSTANCE_GET_PRIVATE (progress, \
                                     GIMP_TYPE_CANVAS_PROGRESS, \
                                     GimpCanvasProgressPrivate)


/*  local function prototypes  */

static void             gimp_canvas_progress_iface_init   (GimpProgressInterface *iface);

static void             gimp_canvas_progress_finalize     (GObject          *object);
static void             gimp_canvas_progress_set_property (GObject          *object,
                                                           guint             property_id,
                                                           const GValue     *value,
                                                           GParamSpec       *pspec);
static void             gimp_canvas_progress_get_property (GObject          *object,
                                                           guint             property_id,
                                                           GValue           *value,
                                                           GParamSpec       *pspec);
static void             gimp_canvas_progress_draw         (GimpCanvasItem   *item,
                                                           GimpDisplayShell *shell,
                                                           cairo_t          *cr);
static cairo_region_t * gimp_canvas_progress_get_extents  (GimpCanvasItem   *item,
                                                           GimpDisplayShell *shell);

static GimpProgress   * gimp_canvas_progress_start        (GimpProgress      *progress,
                                                           const gchar       *message,
                                                           gboolean           cancelable);
static void             gimp_canvas_progress_end          (GimpProgress      *progress);
static gboolean         gimp_canvas_progress_is_active    (GimpProgress      *progress);
static void             gimp_canvas_progress_set_text     (GimpProgress      *progress,
                                                           const gchar       *message);
static void             gimp_canvas_progress_set_value    (GimpProgress      *progress,
                                                           gdouble            percentage);
static gdouble          gimp_canvas_progress_get_value    (GimpProgress      *progress);
static void             gimp_canvas_progress_pulse        (GimpProgress      *progress);
static gboolean         gimp_canvas_progress_message      (GimpProgress      *progress,
                                                           Gimp              *gimp,
                                                           GimpMessageSeverity severity,
                                                           const gchar       *domain,
                                                           const gchar       *message);


G_DEFINE_TYPE_WITH_CODE (GimpCanvasProgress, gimp_canvas_progress,
                         GIMP_TYPE_CANVAS_ITEM,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_PROGRESS,
                                                gimp_canvas_progress_iface_init))

#define parent_class gimp_canvas_progress_parent_class


static void
gimp_canvas_progress_class_init (GimpCanvasProgressClass *klass)
{
  GObjectClass        *object_class = G_OBJECT_CLASS (klass);
  GimpCanvasItemClass *item_class   = GIMP_CANVAS_ITEM_CLASS (klass);

  object_class->finalize     = gimp_canvas_progress_finalize;
  object_class->set_property = gimp_canvas_progress_set_property;
  object_class->get_property = gimp_canvas_progress_get_property;

  item_class->draw           = gimp_canvas_progress_draw;
  item_class->get_extents    = gimp_canvas_progress_get_extents;

  g_object_class_install_property (object_class, PROP_ANCHOR,
                                   g_param_spec_enum ("anchor", NULL, NULL,
                                                      GIMP_TYPE_HANDLE_ANCHOR,
                                                      GIMP_HANDLE_ANCHOR_CENTER,
                                                      GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_X,
                                   g_param_spec_double ("x", NULL, NULL,
                                                        -GIMP_MAX_IMAGE_SIZE,
                                                        GIMP_MAX_IMAGE_SIZE, 0,
                                                        GIMP_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_Y,
                                   g_param_spec_double ("y", NULL, NULL,
                                                        -GIMP_MAX_IMAGE_SIZE,
                                                        GIMP_MAX_IMAGE_SIZE, 0,
                                                        GIMP_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GimpCanvasProgressPrivate));
}

static void
gimp_canvas_progress_iface_init (GimpProgressInterface *iface)
{
  iface->start     = gimp_canvas_progress_start;
  iface->end       = gimp_canvas_progress_end;
  iface->is_active = gimp_canvas_progress_is_active;
  iface->set_text  = gimp_canvas_progress_set_text;
  iface->set_value = gimp_canvas_progress_set_value;
  iface->get_value = gimp_canvas_progress_get_value;
  iface->pulse     = gimp_canvas_progress_pulse;
  iface->message   = gimp_canvas_progress_message;
}

static void
gimp_canvas_progress_init (GimpCanvasProgress *progress)
{
}

static void
gimp_canvas_progress_finalize (GObject *object)
{
  GimpCanvasProgressPrivate *private = GET_PRIVATE (object);

  if (private->text)
    {
      g_free (private->text);
      private->text = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_canvas_progress_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GimpCanvasProgressPrivate *private = GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_ANCHOR:
      private->anchor = g_value_get_enum (value);
      break;
    case PROP_X:
      private->x = g_value_get_double (value);
      break;
    case PROP_Y:
      private->y = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_canvas_progress_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GimpCanvasProgressPrivate *private = GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_ANCHOR:
      g_value_set_enum (value, private->anchor);
      break;
    case PROP_X:
      g_value_set_double (value, private->x);
      break;
    case PROP_Y:
      g_value_set_double (value, private->y);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static PangoLayout *
gimp_canvas_progress_transform (GimpCanvasItem   *item,
                                GimpDisplayShell *shell,
                                gdouble          *x,
                                gdouble          *y,
                                gint             *width,
                                gint             *height)
{
  GimpCanvasProgressPrivate *private = GET_PRIVATE (item);
  PangoLayout               *layout;

  layout = gimp_canvas_get_layout (GIMP_CANVAS (shell->canvas), "%s",
                                   private->text);

  pango_layout_get_pixel_size (layout, width, height);

  *width  += 2 * BORDER;
  *height += 3 * BORDER + 2 * RADIUS;

  gimp_display_shell_transform_xy_f (shell,
                                     private->x, private->y,
                                     x, y);

  gimp_canvas_item_shift_to_north_west (private->anchor,
                                        *x, *y,
                                        *width,
                                        *height,
                                        x, y);

  *x = floor (*x) + 0.5;
  *y = floor (*y) + 0.5;

  return layout;
}

static void
gimp_canvas_progress_draw (GimpCanvasItem   *item,
                           GimpDisplayShell *shell,
                           cairo_t          *cr)
{
  GimpCanvasProgressPrivate *private = GET_PRIVATE (item);
  gdouble                    x, y;
  gint                       width, height;

  gimp_canvas_progress_transform (item, shell, &x, &y, &width, &height);

  cairo_move_to (cr, x, y);
  cairo_line_to (cr, x + width, y);
  cairo_line_to (cr, x + width, y + height - BORDER - 2 * RADIUS);
  cairo_line_to (cr, x + 2 * BORDER + 2 * RADIUS, y + height - BORDER - 2 * RADIUS);
  cairo_arc (cr, x + BORDER + RADIUS, y + height - BORDER - RADIUS,
             BORDER + RADIUS, 0, G_PI);

  _gimp_canvas_item_fill (item, cr);

  cairo_move_to (cr, x + BORDER, y + BORDER);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
  pango_cairo_show_layout (cr,
                           gimp_canvas_get_layout (GIMP_CANVAS (shell->canvas),
                                                   "%s", private->text));

  gimp_display_shell_set_tool_bg_style (shell, cr);
  cairo_arc (cr, x + BORDER + RADIUS, y + height - BORDER - RADIUS,
             RADIUS, - G_PI / 2.0, 2 * G_PI - G_PI / 2.0);
  cairo_fill (cr);

  cairo_set_source_rgba (cr, 0.0, 1.0, 0.0, 1.0);
  cairo_move_to (cr, x + BORDER + RADIUS, y + height - BORDER - RADIUS);
  cairo_arc (cr, x + BORDER + RADIUS, y + height - BORDER - RADIUS,
             RADIUS, - G_PI / 2.0, 2 * G_PI * private->value - G_PI / 2.0);
  cairo_fill (cr);
}

static cairo_region_t *
gimp_canvas_progress_get_extents (GimpCanvasItem   *item,
                                  GimpDisplayShell *shell)
{
  GdkRectangle rectangle;
  gdouble      x, y;
  gint         width, height;

  gimp_canvas_progress_transform (item, shell, &x, &y, &width, &height);

  rectangle.x      = x;
  rectangle.y      = y;
  rectangle.width  = width;
  rectangle.height = height;

  return cairo_region_create_rectangle ((cairo_rectangle_int_t *) &rectangle);
}

static GimpProgress *
gimp_canvas_progress_start (GimpProgress *progress,
                            const gchar  *message,
                            gboolean      cancelable)
{
  gimp_canvas_progress_set_text (progress, message);

  return progress;
}

static void
gimp_canvas_progress_end (GimpProgress *progress)
{
}

static gboolean
gimp_canvas_progress_is_active (GimpProgress *progress)
{
  return TRUE;
}

static void
gimp_canvas_progress_set_text (GimpProgress *progress,
                               const gchar  *message)
{
  GimpCanvasProgressPrivate *private = GET_PRIVATE (progress);
  cairo_region_t            *old_region;
  cairo_region_t            *new_region;

  old_region = gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (progress));

  if (private->text)
    g_free (private->text);

  private->text = g_strdup (message);

  new_region = gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (progress));

  cairo_region_union (new_region, old_region);
  cairo_region_destroy (old_region);

  _gimp_canvas_item_update (GIMP_CANVAS_ITEM (progress), new_region);

  cairo_region_destroy (new_region);
}

static void
gimp_canvas_progress_set_value (GimpProgress *progress,
                                gdouble       percentage)
{
  GimpCanvasProgressPrivate *private = GET_PRIVATE (progress);

  if (percentage != private->value)
    {
      cairo_region_t *region;

      private->value = percentage;

      region = gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (progress));

      _gimp_canvas_item_update (GIMP_CANVAS_ITEM (progress), region);

      cairo_region_destroy (region);
    }
}

static gdouble
gimp_canvas_progress_get_value (GimpProgress *progress)
{
  GimpCanvasProgressPrivate *private = GET_PRIVATE (progress);

  return private->value;
}

static void
gimp_canvas_progress_pulse (GimpProgress *progress)
{
}

static gboolean
gimp_canvas_progress_message (GimpProgress        *progress,
                              Gimp                *gimp,
                              GimpMessageSeverity  severity,
                              const gchar         *domain,
                              const gchar         *message)
{
  return FALSE;
}

GimpCanvasItem *
gimp_canvas_progress_new (GimpDisplayShell *shell,
                          GimpHandleAnchor  anchor,
                          gdouble           x,
                          gdouble           y)
{
  g_return_val_if_fail (GIMP_IS_DISPLAY_SHELL (shell), NULL);

  return g_object_new (GIMP_TYPE_CANVAS_PROGRESS,
                       "shell",  shell,
                       "anchor", anchor,
                       "x",      x,
                       "y",      y,
                       NULL);
}
