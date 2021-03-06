/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpnavigationeditor.c
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
 *
 * partly based on app/nav_window
 * Copyright (C) 1999 Andy Thomas <alt@gimp.org>
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

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "display-types.h"

#include "config/gimpdisplayconfig.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"

#include "widgets/gimpdocked.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimpmenufactory.h"
#include "widgets/gimpnavigationview.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpviewrenderer.h"

#include "gimpdisplay.h"
#include "gimpdisplayshell.h"
#include "gimpdisplayshell-scale.h"
#include "gimpdisplayshell-scroll.h"
#include "gimpnavigationeditor.h"

#include "gimp-intl.h"


static void        gimp_navigation_editor_docked_iface_init (GimpDockedInterface  *iface);

static void        gimp_navigation_editor_dispose           (GObject              *object);

static void        gimp_navigation_editor_set_context       (GimpDocked           *docked,
                                                             GimpContext          *context);

static GtkWidget * gimp_navigation_editor_new_private       (GimpMenuFactory      *menu_factory,
                                                             GimpDisplayShell     *shell);

static void        gimp_navigation_editor_set_shell         (GimpNavigationEditor *editor,
                                                             GimpDisplayShell     *shell);
static gboolean    gimp_navigation_editor_button_release    (GtkWidget            *widget,
                                                             GdkEventButton       *bevent,
                                                             GimpDisplayShell     *shell);
static void        gimp_navigation_editor_marker_changed    (GimpNavigationView   *view,
                                                             gdouble               x,
                                                             gdouble               y,
                                                             gdouble               width,
                                                             gdouble               height,
                                                             GimpNavigationEditor *editor);
static void        gimp_navigation_editor_zoom              (GimpNavigationView   *view,
                                                             GimpZoomType          direction,
                                                             GimpNavigationEditor *editor);
static void        gimp_navigation_editor_scroll            (GimpNavigationView   *view,
                                                             GdkScrollDirection    direction,
                                                             GimpNavigationEditor *editor);

static void        gimp_navigation_editor_zoom_adj_changed  (GtkAdjustment        *adj,
                                                             GimpNavigationEditor *editor);

static void        gimp_navigation_editor_shell_scaled      (GimpDisplayShell     *shell,
                                                             GimpNavigationEditor *editor);
static void        gimp_navigation_editor_shell_scrolled    (GimpDisplayShell     *shell,
                                                             GimpNavigationEditor *editor);
static void        gimp_navigation_editor_shell_reconnect   (GimpDisplayShell     *shell,
                                                             GimpNavigationEditor *editor);
static void        gimp_navigation_editor_update_marker     (GimpNavigationEditor *editor);


G_DEFINE_TYPE_WITH_CODE (GimpNavigationEditor, gimp_navigation_editor,
                         GIMP_TYPE_EDITOR,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_DOCKED,
                                                gimp_navigation_editor_docked_iface_init))

#define parent_class gimp_navigation_editor_parent_class


static void
gimp_navigation_editor_class_init (GimpNavigationEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gimp_navigation_editor_dispose;
}

static void
gimp_navigation_editor_docked_iface_init (GimpDockedInterface *iface)
{
  iface->set_context = gimp_navigation_editor_set_context;
}

static void
gimp_navigation_editor_init (GimpNavigationEditor *editor)
{
  GtkWidget *frame;

  editor->context = NULL;
  editor->shell   = NULL;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (editor), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  editor->view = gimp_view_new_by_types (NULL,
                                         GIMP_TYPE_NAVIGATION_VIEW,
                                         GIMP_TYPE_IMAGE,
                                         GIMP_VIEW_SIZE_MEDIUM, 0, TRUE);
  gtk_container_add (GTK_CONTAINER (frame), editor->view);
  gtk_widget_show (editor->view);

  g_signal_connect (editor->view, "marker-changed",
                    G_CALLBACK (gimp_navigation_editor_marker_changed),
                    editor);
  g_signal_connect (editor->view, "zoom",
                    G_CALLBACK (gimp_navigation_editor_zoom),
                    editor);
  g_signal_connect (editor->view, "scroll",
                    G_CALLBACK (gimp_navigation_editor_scroll),
                    editor);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);
}

static void
gimp_navigation_editor_dispose (GObject *object)
{
  GimpNavigationEditor *editor = GIMP_NAVIGATION_EDITOR (object);

  if (editor->shell)
    gimp_navigation_editor_set_shell (editor, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_navigation_editor_display_changed (GimpContext          *context,
                                        GimpDisplay          *display,
                                        GimpNavigationEditor *editor)
{
  GimpDisplayShell *shell = NULL;

  if (display)
    shell = gimp_display_get_shell (display);

  gimp_navigation_editor_set_shell (editor, shell);
}

static void
gimp_navigation_editor_set_context (GimpDocked  *docked,
                                    GimpContext *context)
{
  GimpNavigationEditor *editor  = GIMP_NAVIGATION_EDITOR (docked);
  GimpDisplay          *display = NULL;

  if (editor->context)
    {
      g_signal_handlers_disconnect_by_func (editor->context,
                                            gimp_navigation_editor_display_changed,
                                            editor);
    }

  editor->context = context;

  if (editor->context)
    {
      g_signal_connect (context, "display-changed",
                        G_CALLBACK (gimp_navigation_editor_display_changed),
                        editor);

      display = gimp_context_get_display (context);
    }

  gimp_view_renderer_set_context (GIMP_VIEW (editor->view)->renderer,
                                  context);

  gimp_navigation_editor_display_changed (editor->context,
                                          display,
                                          editor);
}


/*  public functions  */

GtkWidget *
gimp_navigation_editor_new (GimpMenuFactory *menu_factory)
{
  return gimp_navigation_editor_new_private (menu_factory, NULL);
}

void
gimp_navigation_editor_popup (GimpDisplayShell *shell,
                              GtkWidget        *widget,
                              gint              click_x,
                              gint              click_y)
{
  GtkStyle             *style = gtk_widget_get_style (widget);
  GimpNavigationEditor *editor;
  GimpNavigationView   *view;
  GdkScreen            *screen;
  gint                  x, y;
  gint                  view_marker_x, view_marker_y;
  gint                  view_marker_width, view_marker_height;

  g_return_if_fail (GIMP_IS_DISPLAY_SHELL (shell));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (! shell->nav_popup)
    {
      GtkWidget *frame;

      shell->nav_popup = gtk_window_new (GTK_WINDOW_POPUP);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
      gtk_container_add (GTK_CONTAINER (shell->nav_popup), frame);
      gtk_widget_show (frame);

      editor =
        GIMP_NAVIGATION_EDITOR (gimp_navigation_editor_new_private (NULL,
                                                                    shell));
      gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (editor));
      gtk_widget_show (GTK_WIDGET (editor));

      g_signal_connect (editor->view, "button-release-event",
                        G_CALLBACK (gimp_navigation_editor_button_release),
                        shell);
    }
  else
    {
      GtkWidget *bin = gtk_bin_get_child (GTK_BIN (shell->nav_popup));

      editor = GIMP_NAVIGATION_EDITOR (gtk_bin_get_child (GTK_BIN (bin)));
    }

  view = GIMP_NAVIGATION_VIEW (editor->view);

  /* Set poup screen */
  screen = gtk_widget_get_screen (widget);
  gtk_window_set_screen (GTK_WINDOW (shell->nav_popup), screen);

  gimp_navigation_view_get_local_marker (view,
                                         &view_marker_x,
                                         &view_marker_y,
                                         &view_marker_width,
                                         &view_marker_height);
  /* Position the popup */
  {
    gint x_origin, y_origin;
    gint popup_width, popup_height;
    gint border_width, border_height;
    gint screen_click_x, screen_click_y;

    gdk_window_get_origin (gtk_widget_get_window (widget),
                           &x_origin, &y_origin);

    screen_click_x = x_origin + click_x;
    screen_click_y = y_origin + click_y;
    border_width   = style->xthickness * 4;
    border_height  = style->ythickness * 4;
    popup_width    = GIMP_VIEW (view)->renderer->width  - border_width;
    popup_height   = GIMP_VIEW (view)->renderer->height - border_height;

    x = screen_click_x -
        border_width -
        view_marker_x -
        view_marker_width / 2;

    y = screen_click_y -
        border_height -
        view_marker_y -
        view_marker_height / 2;

    /* When the image is zoomed out and overscrolled, the above
     * calculation risks positioning the popup far far away from the
     * click coordinate. We don't want that, so perform some clamping.
     */
    x = CLAMP (x, screen_click_x - popup_width,  screen_click_x);
    y = CLAMP (y, screen_click_y - popup_height, screen_click_y);

    /* If the popup doesn't fit into the screen, we have a problem.
     * We move the popup onscreen and risk that the pointer is not
     * in the square representing the viewable area anymore. Moving
     * the pointer will make the image scroll by a large amount,
     * but then it works as usual. Probably better than a popup that
     * is completely unusable in the lower right of the screen.
     *
     * Warping the pointer would be another solution ...
     */
    x = CLAMP (x, 0, gdk_screen_get_width (screen)  - popup_width);
    y = CLAMP (y, 0, gdk_screen_get_height (screen) - popup_height);

    gtk_window_move (GTK_WINDOW (shell->nav_popup), x, y);
  }

  gtk_widget_show (shell->nav_popup);
  gdk_flush ();

  /* fill in then grab pointer */
  gimp_navigation_view_set_motion_offset (view,
                                          view_marker_width  / 2,
                                          view_marker_height / 2);
  gimp_navigation_view_grab_pointer (view);
}


/*  private functions  */

static GtkWidget *
gimp_navigation_editor_new_private (GimpMenuFactory  *menu_factory,
                                    GimpDisplayShell *shell)
{
  GimpNavigationEditor *editor;

  g_return_val_if_fail (menu_factory == NULL ||
                        GIMP_IS_MENU_FACTORY (menu_factory), NULL);
  g_return_val_if_fail (shell == NULL || GIMP_IS_DISPLAY_SHELL (shell), NULL);
  g_return_val_if_fail (menu_factory || shell, NULL);

  if (shell)
    {
      Gimp              *gimp   = shell->display->gimp;
      GimpDisplayConfig *config = shell->display->config;
      GimpView          *view;

      editor = g_object_new (GIMP_TYPE_NAVIGATION_EDITOR, NULL);

      view = GIMP_VIEW (editor->view);

      gimp_view_renderer_set_size (view->renderer,
                                   config->nav_preview_size * 3,
                                   view->renderer->border_width);
      gimp_view_renderer_set_context (view->renderer,
                                      gimp_get_user_context (gimp));

      gimp_navigation_editor_set_shell (editor, shell);

    }
  else
    {
      GtkWidget *hscale;
      GtkWidget *hbox;

      editor = g_object_new (GIMP_TYPE_NAVIGATION_EDITOR,
                             "menu-factory",    menu_factory,
                             "menu-identifier", "<NavigationEditor>",
                             NULL);

      gtk_widget_set_size_request (editor->view,
                                   GIMP_VIEW_SIZE_HUGE,
                                   GIMP_VIEW_SIZE_HUGE);
      gimp_view_set_expand (GIMP_VIEW (editor->view), TRUE);

      /* the editor buttons */

      editor->zoom_out_button =
        gimp_editor_add_action_button (GIMP_EDITOR (editor), "view",
                                       "view-zoom-out", NULL);

      editor->zoom_in_button =
        gimp_editor_add_action_button (GIMP_EDITOR (editor), "view",
                                       "view-zoom-in", NULL);

      editor->zoom_100_button =
        gimp_editor_add_action_button (GIMP_EDITOR (editor), "view",
                                       "view-zoom-1-1", NULL);

      editor->zoom_fit_in_button =
        gimp_editor_add_action_button (GIMP_EDITOR (editor), "view",
                                       "view-zoom-fit-in", NULL);

      editor->zoom_fill_button =
        gimp_editor_add_action_button (GIMP_EDITOR (editor), "view",
                                       "view-zoom-fill", NULL);

      editor->shrink_wrap_button =
        gimp_editor_add_action_button (GIMP_EDITOR (editor), "view",
                                       "view-shrink-wrap", NULL);

      /* the zoom scale */

      hbox = gtk_hbox_new (FALSE, 6);
      gtk_box_pack_end (GTK_BOX (editor), hbox, FALSE, FALSE, 0);
      gtk_widget_show (hbox);

      editor->zoom_adjustment =
        GTK_ADJUSTMENT (gtk_adjustment_new (0.0, -8.0, 8.0, 0.5, 1.0, 0.0));

      g_signal_connect (editor->zoom_adjustment, "value-changed",
                        G_CALLBACK (gimp_navigation_editor_zoom_adj_changed),
                        editor);

      hscale = gtk_hscale_new (GTK_ADJUSTMENT (editor->zoom_adjustment));
      gtk_range_set_update_policy (GTK_RANGE (hscale), GTK_UPDATE_DELAYED);
      gtk_scale_set_draw_value (GTK_SCALE (hscale), FALSE);
      gtk_box_pack_start (GTK_BOX (hbox), hscale, TRUE, TRUE, 0);
      gtk_widget_show (hscale);

      /* the zoom label */

      editor->zoom_label = gtk_label_new ("100%");
      gtk_label_set_width_chars (GTK_LABEL (editor->zoom_label), 7);
      gtk_box_pack_start (GTK_BOX (hbox), editor->zoom_label, FALSE, FALSE, 0);
      gtk_widget_show (editor->zoom_label);
    }

  gimp_view_renderer_set_background (GIMP_VIEW (editor->view)->renderer,
                                     GIMP_STOCK_TEXTURE);

  return GTK_WIDGET (editor);
}

static void
gimp_navigation_editor_set_shell (GimpNavigationEditor *editor,
                                  GimpDisplayShell     *shell)
{
  g_return_if_fail (GIMP_IS_NAVIGATION_EDITOR (editor));
  g_return_if_fail (! shell || GIMP_IS_DISPLAY_SHELL (shell));

  if (shell == editor->shell)
    return;

  if (editor->shell)
    {
      g_signal_handlers_disconnect_by_func (editor->shell,
                                            gimp_navigation_editor_shell_scaled,
                                            editor);
      g_signal_handlers_disconnect_by_func (editor->shell,
                                            gimp_navigation_editor_shell_scrolled,
                                            editor);
      g_signal_handlers_disconnect_by_func (editor->shell,
                                            gimp_navigation_editor_shell_reconnect,
                                            editor);
    }
  else if (shell)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);
    }

  editor->shell = shell;

  if (editor->shell)
    {
      GimpImage *image = gimp_display_get_image (shell->display);

      gimp_view_set_viewable (GIMP_VIEW (editor->view),
                              GIMP_VIEWABLE (image));

      g_signal_connect (editor->shell, "scaled",
                        G_CALLBACK (gimp_navigation_editor_shell_scaled),
                        editor);
      g_signal_connect (editor->shell, "scrolled",
                        G_CALLBACK (gimp_navigation_editor_shell_scrolled),
                        editor);
      g_signal_connect (editor->shell, "reconnect",
                        G_CALLBACK (gimp_navigation_editor_shell_reconnect),
                        editor);

      gimp_navigation_editor_shell_scaled (editor->shell, editor);
    }
  else
    {
      gimp_view_set_viewable (GIMP_VIEW (editor->view), NULL);
      gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);
    }

  if (GIMP_EDITOR (editor)->ui_manager)
    gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                            GIMP_EDITOR (editor)->popup_data);
}

static gboolean
gimp_navigation_editor_button_release (GtkWidget        *widget,
                                       GdkEventButton   *bevent,
                                       GimpDisplayShell *shell)
{
  if (bevent->button == 1)
    {
      gtk_widget_hide (shell->nav_popup);
    }

  return FALSE;
}

static void
gimp_navigation_editor_marker_changed (GimpNavigationView   *view,
                                       gdouble               x,
                                       gdouble               y,
                                       gdouble               width,
                                       gdouble               height,
                                       GimpNavigationEditor *editor)
{
  if (editor->shell)
    {
      if (gimp_display_get_image (editor->shell->display))
        gimp_display_shell_scroll_center_image_coordinate (editor->shell,
                                                           x + width / 2,
                                                           y + height / 2);
    }
}

static void
gimp_navigation_editor_zoom (GimpNavigationView   *view,
                             GimpZoomType          direction,
                             GimpNavigationEditor *editor)
{
  g_return_if_fail (direction != GIMP_ZOOM_TO);

  if (editor->shell)
    {
      if (gimp_display_get_image (editor->shell->display))
        gimp_display_shell_scale (editor->shell,
                                  direction,
                                  0.0,
                                  GIMP_ZOOM_FOCUS_BEST_GUESS);
    }
}

static void
gimp_navigation_editor_scroll (GimpNavigationView   *view,
                               GdkScrollDirection    direction,
                               GimpNavigationEditor *editor)
{
  if (editor->shell)
    {
      GtkAdjustment *adj = NULL;
      gdouble        value;

      switch (direction)
        {
        case GDK_SCROLL_LEFT:
        case GDK_SCROLL_RIGHT:
          adj = editor->shell->hsbdata;
          break;

        case GDK_SCROLL_UP:
        case GDK_SCROLL_DOWN:
          adj = editor->shell->vsbdata;
          break;
        }

      g_assert (adj != NULL);

      value = gtk_adjustment_get_value (adj);

      switch (direction)
        {
        case GDK_SCROLL_LEFT:
        case GDK_SCROLL_UP:
          value -= gtk_adjustment_get_page_increment (adj) / 2;
          break;

        case GDK_SCROLL_RIGHT:
        case GDK_SCROLL_DOWN:
          value += gtk_adjustment_get_page_increment (adj) / 2;
          break;
        }

      value = CLAMP (value,
                     gtk_adjustment_get_lower (adj),
                     gtk_adjustment_get_upper (adj) -
                     gtk_adjustment_get_page_size (adj));

      gtk_adjustment_set_value (adj, value);
    }
}

static void
gimp_navigation_editor_zoom_adj_changed (GtkAdjustment        *adj,
                                         GimpNavigationEditor *editor)
{
  if (gimp_display_get_image (editor->shell->display))
    gimp_display_shell_scale (editor->shell,
                              GIMP_ZOOM_TO,
                              pow (2.0, gtk_adjustment_get_value (adj)),
                              GIMP_ZOOM_FOCUS_BEST_GUESS);
}

static void
gimp_navigation_editor_shell_scaled (GimpDisplayShell     *shell,
                                     GimpNavigationEditor *editor)
{
  if (editor->zoom_label)
    {
      gchar *str;

      g_object_get (shell->zoom,
                    "percentage", &str,
                    NULL);
      gtk_label_set_text (GTK_LABEL (editor->zoom_label), str);
      g_free (str);
    }

  if (editor->zoom_adjustment)
    {
      gdouble val;

      val = log (gimp_zoom_model_get_factor (shell->zoom)) / G_LN2;

      g_signal_handlers_block_by_func (editor->zoom_adjustment,
                                       gimp_navigation_editor_zoom_adj_changed,
                                       editor);

      gtk_adjustment_set_value (editor->zoom_adjustment, val);

      g_signal_handlers_unblock_by_func (editor->zoom_adjustment,
                                         gimp_navigation_editor_zoom_adj_changed,
                                         editor);
    }

  gimp_navigation_editor_update_marker (editor);

  if (GIMP_EDITOR (editor)->ui_manager)
    gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                            GIMP_EDITOR (editor)->popup_data);
}

static void
gimp_navigation_editor_shell_scrolled (GimpDisplayShell     *shell,
                                       GimpNavigationEditor *editor)
{
  gimp_navigation_editor_update_marker (editor);

  if (GIMP_EDITOR (editor)->ui_manager)
    gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                            GIMP_EDITOR (editor)->popup_data);
}

static void
gimp_navigation_editor_shell_reconnect (GimpDisplayShell     *shell,
                                        GimpNavigationEditor *editor)
{
  GimpImage *image = gimp_display_get_image (shell->display);

  gimp_view_set_viewable (GIMP_VIEW (editor->view),
                          GIMP_VIEWABLE (image));

  if (GIMP_EDITOR (editor)->ui_manager)
    gimp_ui_manager_update (GIMP_EDITOR (editor)->ui_manager,
                            GIMP_EDITOR (editor)->popup_data);
}

static void
gimp_navigation_editor_update_marker (GimpNavigationEditor *editor)
{
  GimpViewRenderer *renderer = GIMP_VIEW (editor->view)->renderer;
  GimpDisplayShell *shell    = editor->shell;

  if (renderer->dot_for_dot != shell->dot_for_dot)
    gimp_view_renderer_set_dot_for_dot (renderer, shell->dot_for_dot);

  if (renderer->viewable)
    {
      GimpNavigationView *view = GIMP_NAVIGATION_VIEW (editor->view);
      gdouble             x, y;
      gdouble             w, h;

      gimp_display_shell_scroll_get_viewport (shell, &x, &y, &w, &h);

      gimp_navigation_view_set_marker (view, x, y, w, h);
    }
}
