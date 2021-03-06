/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2009 Martin Nordholts
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

#include <gegl.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <utime.h>

#include "libgimpbase/gimpbase.h"

#include "dialogs/dialogs-types.h"

#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpsessioninfo.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"

#include "tests.h"

#include "gimp-app-test-utils.h"


typedef struct
{
  int dummy;
} GimpTestFixture;

typedef struct
{
  gchar    *filename;
  gchar    *md5;
  GTimeVal  modtime;
} GimpTestFileState;


static gboolean  gimp_test_get_file_state_verbose (const gchar       *filename,
                                                   GimpTestFileState *filestate);
static gboolean  gimp_test_file_state_changes     (const gchar       *filename,
                                                   GimpTestFileState *state1,
                                                   GimpTestFileState *state2);

static Gimp *gimp = NULL;


int main(int argc, char **argv)
{
  GimpTestFileState  initial_sessionrc_state = { NULL, NULL, { 0, 0 } };
  GimpTestFileState  initial_dockrc_state    = { NULL, NULL, { 0, 0 } };
  GimpTestFileState  final_sessionrc_state   = { NULL, NULL, { 0, 0 } };
  GimpTestFileState  final_dockrc_state      = { NULL, NULL, { 0, 0 } };
  gchar             *sessionrc_filename      = NULL;
  gchar             *dockrc_filename         = NULL;
  gint               result                  = 0;

  gimp_test_bail_if_no_display ();
  gtk_test_init (&argc, &argv, NULL);

  /* Make sure to run this before we use any GIMP functions */
  gimp_test_utils_set_gimp2_directory ("GIMP_TESTING_ABS_TOP_SRCDIR",
                                       "app/tests/gimpdir");
  gimp_test_utils_setup_menus_dir ();

  /* Note that we expect the resulting sessionrc to be different from
   * the read file, which is why we check the MD5 of the -expected
   * variant
   */
  sessionrc_filename = gimp_personal_rc_file ("sessionrc-expected");
  dockrc_filename    = gimp_personal_rc_file ("dockrc-expected");

  /* Remeber the modtimes and MD5s */
  if (!gimp_test_get_file_state_verbose (sessionrc_filename,
                                         &initial_sessionrc_state))
    result = -1;
  if (!gimp_test_get_file_state_verbose (dockrc_filename,
                                         &initial_dockrc_state))
    result = -1;

  /* Start up GIMP */
  gimp = gimp_init_for_gui_testing (TRUE /*show_gui*/);

  /* Let the main loop run until idle to let things stabilize. This
   * includes parsing sessionrc and dockrc
   */
  gimp_test_run_mainloop_until_idle ();

  /* Change the gimp dir to the output dir so files are written there,
   * we don't want to (can't always) write to files in the source
   * dir. There is a hook in Makefile.am that makes sure the output
   * dir exists
   */
  gimp_test_utils_set_gimp2_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR",
                                       "app/tests/gimpdir-output");
  g_free (sessionrc_filename);
  g_free (dockrc_filename);
  sessionrc_filename = gimp_personal_rc_file ("sessionrc");
  dockrc_filename    = gimp_personal_rc_file ("dockrc");

  /* Exit. This includes writing sessionrc and dockrc*/
  gimp_exit (gimp, TRUE);

  /* Now get the new modtimes and MD5s */
  if (!gimp_test_get_file_state_verbose (sessionrc_filename,
                                         &final_sessionrc_state))
    result = -1;
  if (!gimp_test_get_file_state_verbose (dockrc_filename,
                                         &final_dockrc_state))
    result = -1;

  /* If things have gone our way, GIMP will have deserialized
   * sessionrc and dockrc, shown the GUI, and then serialized the new
   * files. To make sure we have new files we check the modtime, and
   * to make sure that their content remains the same we compare their
   * MD5
   */
  if (!gimp_test_file_state_changes ("sessionrc",
                                     &initial_sessionrc_state,
                                     &final_sessionrc_state))
    result = -1;
  if (!gimp_test_file_state_changes ("dockrc",
                                     &initial_dockrc_state,
                                     &final_dockrc_state))
    result = -1;

  if (result == 0)
    g_print ("/gimp-session-management/sessionrc_compatibility: OK\n");

  /* Don't bother freeing stuff, the process is short-lived */
  return result;
}

static gboolean
gimp_test_get_file_state_verbose (const gchar       *filename,
                                  GimpTestFileState *filestate)
{
  gboolean success = TRUE;

  filestate->filename = g_strdup (filename);

  /* Get checksum */
  if (success)
    {
      gchar *contents = NULL;
      gsize  length   = 0;

      success = g_file_get_contents (filename,
                                     &contents,
                                     &length,
                                     NULL);
      if (success)
        {
          filestate->md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5,
                                                          contents,
                                                          length);
        }

      g_free (contents);
    }

  /* Get modification time */
  if (success)
    {
      GFile     *file = g_file_new_for_path (filename);
      GFileInfo *info = g_file_query_info (file,
                                           G_FILE_ATTRIBUTE_TIME_MODIFIED, 0,
                                           NULL, NULL);
      if (info)
        {
          g_file_info_get_modification_time (info, &filestate->modtime);
          success = TRUE;
          g_object_unref (info);
        }
      else
        {
          success = FALSE;
        }

      g_object_unref (file);
    }

  if (! success)
    g_printerr ("Failed to get initial file info for '%s'\n", filename);

  return success;
}

static gboolean
gimp_test_file_state_changes (const gchar       *filename,
                              GimpTestFileState *state1,
                              GimpTestFileState *state2)
{
  if (state1->modtime.tv_sec  == state2->modtime.tv_sec &&
      state1->modtime.tv_usec == state2->modtime.tv_usec)
    {
      g_printerr ("A new '%s' was not created\n", filename);
      return FALSE;
    }

  if (strcmp (state1->md5, state2->md5) != 0)
    {
      char *diff_argv[5] = {
        "diff",
        "-u",
        state1->filename,
        state2->filename,
        NULL
      };

      g_printerr ("'%s' was changed but should not have been. Reason, using "
                  "`diff -u $expected $actual`\n", filename);

      g_spawn_sync (NULL /*working_directory*/,
                    diff_argv,
                    NULL /*envp*/,
                    G_SPAWN_SEARCH_PATH,
                    NULL /*child_setup*/,
                    NULL /*user_data*/,
                    NULL /*standard_output*/,
                    NULL /*standard_error*/,
                    NULL /*exist_status*/,
                    NULL /*error*/);

      return FALSE;
    }

  return TRUE;
}
