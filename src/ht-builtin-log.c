/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2011 Colin Walters <walters@verbum.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Colin Walters <walters@verbum.org>
 */

#include "config.h"

#include "ht-builtins.h"
#include "hacktree.h"

#include <glib/gi18n.h>

static char *repo_path;

static GOptionEntry options[] = {
  { "repo", 0, 0, G_OPTION_ARG_FILENAME, &repo_path, "Repository path", "repo" },
  { NULL }
};

gboolean
hacktree_builtin_log (int argc, char **argv, const char *prefix, GError **error)
{
  GOptionContext *context;
  gboolean ret = FALSE;
  HacktreeRepo *repo = NULL;
  GOutputStream *pager = NULL;
  GVariant *commit = NULL;
  char *head;

  context = g_option_context_new ("- Show revision log");
  g_option_context_add_main_entries (context, options, NULL);

  if (!g_option_context_parse (context, &argc, &argv, error))
    goto out;

  if (repo_path == NULL)
    repo_path = ".";
  if (prefix == NULL)
    prefix = ".";

  repo = hacktree_repo_new (repo_path);
  if (!hacktree_repo_check (repo, error))
    goto out;

  head = g_strdup (hacktree_repo_get_head (repo));
  if (!head)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No HEAD exists");
      goto out;
    }

  if (!ht_util_spawn_pager (&pager, error))
    goto out;

  while (TRUE)
    {
      HacktreeSerializedVariantType type;
      char *formatted = NULL;
      guint32 version;
      const char *parent;
      const char *subject;
      const char *body;
      guint64 timestamp;
      const char *contents;
      const char *root_metadata;
      GDateTime *time_obj = NULL;
      char *formatted_date = NULL;
      const char *body_newline;
      gsize bytes_written;
      GVariant *commit_metadata = NULL;
      char *formatted_metadata = NULL;
      
      if (commit)
        g_variant_unref (commit);
      if (!hacktree_repo_load_variant (repo, head, &type, &commit, error))
        goto out;

      /* Ignore commit metadata for now */
      g_variant_get (commit, "(u@a{sv}&s&s&st&s&s)",
                     &version, &commit_metadata, &parent, &subject, &body,
                     &timestamp, &contents, &root_metadata);
      time_obj = g_date_time_new_from_unix_utc (timestamp);
      formatted_date = g_date_time_format (time_obj, "%a %b %d %H:%M:%S %Y %z");
      g_date_time_unref (time_obj);
      time_obj = NULL;

      formatted_metadata = g_variant_print (commit_metadata, TRUE);
      g_variant_unref (commit_metadata);
      formatted = g_strdup_printf ("commit %s\nSubject: %s\nDate: %s\nMetadata: %s\n\n",
                                   head, subject, formatted_date, formatted_metadata);
      g_free (formatted_metadata);
      g_free (formatted_date);
      formatted_date = NULL;
      
      if (!g_output_stream_write_all (pager, formatted, strlen (formatted), &bytes_written, NULL, error))
        {
          g_free (formatted);
          goto out;
        }
      g_free (formatted);
      
      body_newline = strchr (body, '\n');
      do {
        gsize len;
        if (!g_output_stream_write_all (pager, "    ", 4, &bytes_written, NULL, error))
          goto out;
        len = body_newline ? body_newline - body : strlen (body);
        if (!g_output_stream_write_all (pager, body, len, &bytes_written, NULL, error))
          goto out;
        if (!g_output_stream_write_all (pager, "\n\n", 2, &bytes_written, NULL, error))
          goto out;
        body_newline = strchr (body, '\n');
        if (!body_newline)
          break;
        else
          body_newline += 1;
      } while (*body_newline);

      if (strcmp (parent, "") == 0)
        break;
      g_free (head);
      head = g_strdup (parent);
    }

  if (!g_output_stream_close (pager, NULL, error))
    goto out;
 
  ret = TRUE;
 out:
  if (context)
    g_option_context_free (context);
  if (commit)
    g_variant_unref (commit);
  g_clear_object (&repo);
  return ret;
}
