/* gb-source-emacs.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Roberto Majadas <roberto.majadas@openshine.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#define G_LOG_DOMAIN "emacs"

#include <errno.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <stdio.h>
#include <stdlib.h>

#include "gb-source-emacs.h"
#include "gb-string.h"
#include "gb-widget.h"
#include "gb-editor-workspace.h"
#include "gb-workbench.h"


struct _GbSourceEmacsPrivate
{
  GtkTextView             *text_view;
  GString                 *cmd;
  GtkTextMark             *selection_begin;
  GtkTextMark             *selection_end;
  gint                     selection_line_offset;
  guint                    enabled : 1;
  guint                    connected : 1;
  guint                    select_region :1;
  gulong                   key_press_event_handler;
  gulong                   event_after_handler;
  gulong                   key_release_event_handler;
};

enum
{
  PROP_0,
  PROP_ENABLED,
  PROP_TEXT_VIEW,
  LAST_PROP
};

enum
{
  CLASS_0,
  CLASS_SPACE,
  CLASS_SPECIAL,
  CLASS_WORD,
};

typedef enum
{
  GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
} GbSourceEmacsCommandFlags;

typedef void (*GbSourceEmacsCommandFunc) (GbSourceEmacs           *emacs,
                                          GRegex                  *matcher,
                                          GbSourceEmacsCommandFlags flags
                                          );

typedef struct
{
  GbSourceEmacsCommandFunc   func;
  GRegex                    *matcher;
  GbSourceEmacsCommandFlags  flags;
} GbSourceEmacsCommand;

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceEmacs, gb_source_emacs, G_TYPE_OBJECT)

static GParamSpec *gParamSpecs [LAST_PROP];
static GList *gCommands;

GbSourceEmacs *
gb_source_emacs_new (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  return g_object_new (GB_TYPE_SOURCE_EMACS,
                       "text-view", text_view,
                       NULL);
}

static gboolean
gb_source_emacs_get_selection_bounds (GbSourceEmacs *emacs,
                                      GtkTextIter *insert_iter,
                                      GtkTextIter *selection_iter)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  selection = gtk_text_buffer_get_selection_bound (buffer);

  if (insert_iter)
    gtk_text_buffer_get_iter_at_mark (buffer, insert_iter, insert);

  if (selection_iter)
    gtk_text_buffer_get_iter_at_mark (buffer, selection_iter, selection);

  return gtk_text_buffer_get_has_selection (buffer);
}

static void
gb_source_emacs_clear_selection_data (GbSourceEmacs *emacs)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter end_iter;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  if (priv->select_region == TRUE && priv->selection_begin != NULL && priv->selection_end != NULL)
   {
    gtk_text_buffer_get_iter_at_mark (buffer, &end_iter,
                                      priv->selection_end);

    gtk_text_buffer_select_range (buffer, &end_iter, &end_iter);
   }

  priv->select_region = FALSE;
  priv->selection_line_offset = 0;
  if (priv->selection_begin != NULL)
    {
      gtk_text_buffer_delete_mark(buffer, priv->selection_begin);
      priv->selection_begin = NULL;
    }

  if (priv->selection_end != NULL)
    {
      gtk_text_buffer_delete_mark(buffer, priv->selection_end);
      priv->selection_end = NULL;
    }
}

static void
gb_source_emacs_delete_selection (GbSourceEmacs *emacs)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GtkClipboard *clipboard;
  gchar *text;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  /*
   * If there is no selection to delete, try to remove the next character
   * in the line. If there is no next character, delete the last character
   * in the line. It might look like there is no selection if the line
   * was empty.
   */
  if (gtk_text_iter_equal (&begin, &end))
    {
      if (gtk_text_iter_starts_line (&begin) &&
          gtk_text_iter_ends_line (&end) &&
          (0 == gtk_text_iter_get_line_offset (&end)))
        return;
      else if (!gtk_text_iter_ends_line (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            gtk_text_buffer_get_end_iter (buffer, &end);
        }
      else if (!gtk_text_iter_starts_line (&begin))
        {
          if (!gtk_text_iter_backward_char (&begin))
            return;
        }
      else
        return;
    }

  /*
   * Yank the selection text and apply it to the clipboard.
   */
  text = gtk_text_iter_get_slice (&begin, &end);
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (emacs->priv->text_view),
                                        GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, text, -1);
  g_free (text);

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_end_user_action (buffer);
}

static void
gb_source_emacs_cmd_exit_from_command_line  (GbSourceEmacs           *emacs,
                                             GRegex                  *matcher,
                                             GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;

  if (priv->cmd != NULL)
    g_string_free(priv->cmd, TRUE);
  priv->cmd = g_string_new(NULL);

  gb_source_emacs_clear_selection_data(emacs);
}

static void
gb_source_emacs_cmd_select_region  (GbSourceEmacs           *emacs,
                                    GRegex                  *matcher,
                                    GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  priv->select_region = TRUE;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  has_selection = gb_source_emacs_get_selection_bounds (emacs, &iter, &selection);
  if (has_selection) {
    gtk_text_buffer_select_range (buffer, &iter, &iter);
  }

  if (priv->selection_begin != NULL)
    {
      gtk_text_buffer_delete_mark(buffer, priv->selection_begin);
      priv->selection_begin = NULL;
    }

  if (priv->selection_end != NULL)
    {
      gtk_text_buffer_delete_mark(buffer, priv->selection_end);
      priv->selection_end = NULL;
    }

  priv->selection_begin = gtk_text_buffer_create_mark (buffer,
                                                      "selection-begin",
                                                      &iter,
                                                      TRUE);
  priv->selection_end = gtk_text_buffer_create_mark (buffer,
                                                     "selection-end",
                                                     &iter,
                                                     TRUE);
  priv->selection_line_offset = gtk_text_iter_get_visible_line_offset (&iter);
}

static void
gb_source_emacs_cmd_cut (GbSourceEmacs           *emacs,
                         GRegex                  *matcher,
                         GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_selection;
  GtkClipboard *clipboard;
  gchar *text;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  has_selection = gb_source_emacs_get_selection_bounds (emacs, &begin, &end);
  if (has_selection) {
    text = gtk_text_iter_get_slice (&begin, &end);
    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (priv->text_view),
                                          GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text (clipboard, text, -1);
    g_free (text);

    gtk_text_buffer_begin_user_action (buffer);
    gtk_text_buffer_delete (buffer, &begin, &end);
    gb_source_emacs_clear_selection_data(emacs);
    gtk_text_buffer_end_user_action (buffer);
  }
}

static void
gb_source_emacs_cmd_copy (GbSourceEmacs           *emacs,
                          GRegex                  *matcher,
                          GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_selection;
  GtkClipboard *clipboard;
  gchar *text;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  has_selection = gb_source_emacs_get_selection_bounds (emacs, &begin, &end);
  if (has_selection)
  {
    text = gtk_text_iter_get_slice (&begin, &end);
    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (priv->text_view),
                                        GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text (clipboard, text, -1);
    g_free (text);

    if (priv->select_region == TRUE && priv->selection_end != NULL)
      gtk_text_buffer_get_iter_at_mark (buffer, &end,
                                        priv->selection_end);

    gtk_text_buffer_select_range (buffer, &end, &end);
    gb_source_emacs_clear_selection_data(emacs);
  }
}

static void
gb_source_emacs_cmd_yank (GbSourceEmacs           *emacs,
                          GRegex                  *matcher,
                          GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  g_signal_emit_by_name (priv->text_view, "paste-clipboard");
}

static void
gb_source_emacs_cmd_select_region_go_up  (GbSourceEmacs           *emacs,
                                          GRegex                  *matcher,
                                          GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter begin_iter;
  GtkTextIter end_iter;
  gint current_line;

  if (priv->select_region == FALSE || priv->selection_begin == NULL || priv->selection_end == NULL)
    return;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &begin_iter,
                                    priv->selection_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &end_iter,
                                    priv->selection_end);

  current_line = gtk_text_iter_get_line (&end_iter);

  if (current_line == 0)
    return;

  gtk_text_iter_backward_line (&end_iter);
  gtk_text_iter_forward_to_line_end (&end_iter);

  if (current_line == gtk_text_iter_get_line (&end_iter))
    // Empty line fix position
    gtk_text_iter_backward_line (&end_iter);
  else
    while (priv->selection_line_offset < gtk_text_iter_get_visible_line_offset (&end_iter))
      gtk_text_iter_backward_char (&end_iter);

  gtk_text_buffer_move_mark (buffer, priv->selection_end, &end_iter);
  gtk_text_buffer_select_range (buffer, &begin_iter, &end_iter);
}

static void
gb_source_emacs_cmd_select_region_go_down  (GbSourceEmacs           *emacs,
                                            GRegex                  *matcher,
                                            GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter begin_iter;
  GtkTextIter end_iter;
  gint current_line;
  gint lines_number;

  if (priv->select_region == FALSE || priv->selection_begin == NULL || priv->selection_end == NULL)
    return;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &begin_iter,
                                    priv->selection_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &end_iter,
                                    priv->selection_end);


  lines_number = gtk_text_buffer_get_line_count (buffer);
  current_line = gtk_text_iter_get_line (&end_iter);

  if (lines_number == current_line + 1)
    return;

  gtk_text_iter_forward_line (&end_iter);
  gtk_text_iter_forward_to_line_end (&end_iter);

  if (current_line + 2 == gtk_text_iter_get_line (&end_iter))
    // Empty line fix position
    gtk_text_iter_backward_line (&end_iter);
  else
    while (priv->selection_line_offset < gtk_text_iter_get_visible_line_offset (&end_iter))
      gtk_text_iter_backward_char (&end_iter);

  gtk_text_buffer_move_mark (buffer, priv->selection_end, &end_iter);
  gtk_text_buffer_select_range (buffer, &begin_iter, &end_iter);
}

static void
gb_source_emacs_cmd_select_region_go_left  (GbSourceEmacs           *emacs,
                                            GRegex                  *matcher,
                                            GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter begin_iter;
  GtkTextIter end_iter;

  if (priv->select_region == FALSE || priv->selection_begin == NULL || priv->selection_end == NULL)
    return;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &begin_iter,
                                    priv->selection_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &end_iter,
                                    priv->selection_end);

  gtk_text_iter_backward_char(&end_iter);
  gtk_text_buffer_move_mark (buffer, priv->selection_end, &end_iter);
  gtk_text_buffer_select_range (buffer, &begin_iter, &end_iter);

  priv->selection_line_offset = gtk_text_iter_get_visible_line_offset (&end_iter);
}

static void
gb_source_emacs_cmd_select_region_go_right  (GbSourceEmacs           *emacs,
                                             GRegex                  *matcher,
                                             GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter begin_iter;
  GtkTextIter end_iter;

  if (priv->select_region == FALSE || priv->selection_begin == NULL || priv->selection_end == NULL)
    return;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &begin_iter,
                                    priv->selection_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &end_iter,
                                    priv->selection_end);

  gtk_text_iter_forward_char(&end_iter);
  gtk_text_buffer_move_mark (buffer, priv->selection_end, &end_iter);
  gtk_text_buffer_select_range (buffer, &begin_iter, &end_iter);

  priv->selection_line_offset = gtk_text_iter_get_visible_line_offset (&end_iter);
}

static void
gb_source_emacs_cmd_exit  (GbSourceEmacs           *emacs,
                           GRegex                  *matcher,
                           GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;

  gb_widget_activate_action (GTK_WIDGET (priv->text_view), "app", "quit", NULL);
}

static void
gb_source_emacs_cmd_close_document  (GbSourceEmacs           *emacs,
                                     GRegex                  *matcher,
                                     GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;

  gb_widget_activate_action (GTK_WIDGET (priv->text_view), "stack", "close", NULL);
}

static void
gb_source_emacs_cmd_open_file  (GbSourceEmacs           *emacs,
                                GRegex                  *matcher,
                                GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;

  gb_widget_activate_action (GTK_WIDGET (priv->text_view), "workspace", "open", NULL);
}

static void
gb_source_emacs_cmd_save_file  (GbSourceEmacs           *emacs,
                                GRegex                  *matcher,
                                GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;

  gb_widget_activate_action (GTK_WIDGET (priv->text_view), "stack", "save", NULL);
}

static void
gb_source_emacs_cmd_save_file_as  (GbSourceEmacs           *emacs,
                                   GRegex                  *matcher,
                                   GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;

  gb_widget_activate_action (GTK_WIDGET (priv->text_view), "stack", "save-as", NULL);
}

static void
gb_source_emacs_cmd_save_all  (GbSourceEmacs           *emacs,
                               GRegex                  *matcher,
                               GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;

  gb_widget_activate_action (GTK_WIDGET (priv->text_view), "win", "save-all", NULL);
}

static void
gb_source_emacs_cmd_find  (GbSourceEmacs           *emacs,
                             GRegex                  *matcher,
                             GbSourceEmacsCommandFlags flags)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;

  gb_widget_activate_action (GTK_WIDGET (priv->text_view), "editor-frame", "find", NULL);
}

static void
gb_source_emacs_cmd_undo (GbSourceEmacs           *emacs,
                          GRegex                  *matcher,
                          GbSourceEmacsCommandFlags flags)
{
  GtkSourceUndoManager *undo;
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  undo = gtk_source_buffer_get_undo_manager (GTK_SOURCE_BUFFER (buffer));
  if (gtk_source_undo_manager_can_undo (undo))
    gtk_source_undo_manager_undo (undo);
}

static void
gb_source_emacs_cmd_redo (GbSourceEmacs           *emacs,
                          GRegex                  *matcher,
                          GbSourceEmacsCommandFlags flags)
{
  GtkSourceUndoManager *undo;
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  undo = gtk_source_buffer_get_undo_manager (GTK_SOURCE_BUFFER (buffer));
  if (gtk_source_undo_manager_can_redo (undo))
    gtk_source_undo_manager_redo (undo);
}

static void
gb_source_emacs_cmd_move_forward_char (GbSourceEmacs           *emacs,
                                       GRegex                  *matcher,
                                       GbSourceEmacsCommandFlags flags)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  gb_source_emacs_get_selection_bounds (emacs, &iter, &selection);
  if(gtk_text_iter_forward_char(&iter))
    gtk_text_buffer_select_range (buffer, &iter, &iter);
}

static void
gb_source_emacs_cmd_move_backward_char (GbSourceEmacs           *emacs,
                                        GRegex                  *matcher,
                                        GbSourceEmacsCommandFlags flags)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  gb_source_emacs_get_selection_bounds (emacs, &iter, &selection);
  if(gtk_text_iter_backward_char(&iter))
    gtk_text_buffer_select_range (buffer, &iter, &iter);
}

static void
gb_source_emacs_cmd_delete_forward_char (GbSourceEmacs           *emacs,
                                         GRegex                  *matcher,
                                         GbSourceEmacsCommandFlags flags)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  has_selection = gb_source_emacs_get_selection_bounds (emacs, &iter, &selection);
  if (has_selection)
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  gb_source_emacs_delete_selection (emacs);
}

static int
gb_source_emacs_classify (gunichar ch)
{
  switch (ch)
    {
    case ' ':
    case '\t':
    case '\n':
      return CLASS_SPACE;

    case '"': case '\'':
    case '(': case ')':
    case '{': case '}':
    case '[': case ']':
    case '<': case '>':
    case '-': case '+': case '*': case '/':
    case '!': case '@': case '#': case '$': case '%':
    case '^': case '&': case ':': case ';': case '?':
    case '|': case '=': case '\\': case '.': case ',':
    case '_':
      return CLASS_SPECIAL;

    default:
      return CLASS_WORD;
    }
}

static gboolean
text_iter_forward_emacs_word (GtkTextIter *iter)
{
  gint begin_class;
  gint cur_class;
  gunichar ch;

  g_assert (iter);

  ch = gtk_text_iter_get_char (iter);
  begin_class = gb_source_emacs_classify (ch);

  /* Move to the first non-whitespace character if necessary. */
  if (begin_class == CLASS_SPACE)
    {
      for (;;)
        {
          if (!gtk_text_iter_forward_char (iter))
            return FALSE;

          ch = gtk_text_iter_get_char (iter);
          cur_class = gb_source_emacs_classify (ch);
          if (cur_class != CLASS_SPACE)
            return TRUE;
        }
    }

  /* move to first character not at same class level. */
  while (gtk_text_iter_forward_char (iter))
    {
      ch = gtk_text_iter_get_char (iter);
      cur_class = gb_source_emacs_classify (ch);

      if (cur_class == CLASS_SPACE)
        {
          begin_class = CLASS_0;
          continue;
        }

      if (cur_class != begin_class)
        return TRUE;
    }

  return FALSE;
}

static gboolean
text_iter_backward_emacs_word (GtkTextIter *iter)
{
  gunichar ch;
  gint begin_class;
  gint cur_class;

  g_assert (iter);

  if (!gtk_text_iter_backward_char (iter))
    return FALSE;

  /*
   * If we are on space, walk until we get to non-whitespace. Then work our way
   * back to the beginning of the word.
   */
  ch = gtk_text_iter_get_char (iter);
  if (gb_source_emacs_classify (ch) == CLASS_SPACE)
    {
      for (;;)
        {
          if (!gtk_text_iter_backward_char (iter))
            return FALSE;

          ch = gtk_text_iter_get_char (iter);
          if (gb_source_emacs_classify (ch) != CLASS_SPACE)
            break;
        }

      ch = gtk_text_iter_get_char (iter);
      begin_class = gb_source_emacs_classify (ch);

      for (;;)
        {
          if (!gtk_text_iter_backward_char (iter))
            return FALSE;

          ch = gtk_text_iter_get_char (iter);
          cur_class = gb_source_emacs_classify (ch);

          if (cur_class != begin_class)
            {
              gtk_text_iter_forward_char (iter);
              return TRUE;
            }
        }

      return FALSE;
    }

  ch = gtk_text_iter_get_char (iter);
  begin_class = gb_source_emacs_classify (ch);

  for (;;)
    {
      if (!gtk_text_iter_backward_char (iter))
        return FALSE;

      ch = gtk_text_iter_get_char (iter);
      cur_class = gb_source_emacs_classify (ch);

      if (cur_class != begin_class)
        {
          gtk_text_iter_forward_char (iter);
          return TRUE;
        }
    }

  return FALSE;
}

static void
gb_source_emacs_cmd_move_forward_word (GbSourceEmacs           *emacs,
                                       GRegex                  *matcher,
                                       GbSourceEmacsCommandFlags flags)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  gb_source_emacs_get_selection_bounds (emacs, &iter, &selection);

  if (!text_iter_forward_emacs_word (&iter))
    gtk_text_buffer_get_end_iter (buffer, &iter);

  gtk_text_buffer_select_range (buffer, &iter, &iter);

}

static void
gb_source_emacs_cmd_move_backward_word  (GbSourceEmacs           *emacs,
                                         GRegex                  *matcher,
                                         GbSourceEmacsCommandFlags flags)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;

  g_assert (GB_IS_SOURCE_EMACS (emacs));

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  buffer = gtk_text_view_get_buffer (emacs->priv->text_view);
  gb_source_emacs_get_selection_bounds (emacs, &iter, &selection);

  if (!text_iter_backward_emacs_word (&iter))
    gtk_text_buffer_get_start_iter (buffer, &iter);

  gtk_text_buffer_select_range (buffer, &iter, &iter);
}

static gboolean
gb_source_emacs_eval_cmd (GbSourceEmacs *emacs)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GMatchInfo *match_info;
  GList *iter;

  for (iter = gCommands; iter; iter = iter->next)
    {
      GbSourceEmacsCommand *cmd = iter->data;

      g_regex_match (cmd->matcher, priv->cmd->str, 0, &match_info);
      if (g_match_info_matches(match_info))
        {
          cmd->func (emacs, cmd->matcher, cmd->flags);
          g_match_info_free (match_info);
          if (priv->cmd != NULL)
            g_string_free(priv->cmd, TRUE);
          priv->cmd = g_string_new(NULL);
          break;
        }
      g_match_info_free (match_info);
    }
  return TRUE;
}

static void
gb_source_emacs_check_select_region (GbSourceEmacs *emacs) {
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  GtkTextBuffer *buffer;
  GtkTextIter selected_begin_iter;
  GtkTextIter selected_end_iter;
  GtkTextIter real_begin_iter;
  GtkTextIter real_end_iter;

  if (priv->select_region == FALSE || priv->selection_begin == NULL || priv->selection_end == NULL)
    return;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &selected_begin_iter,
                                    priv->selection_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &selected_end_iter,
                                    priv->selection_end);

  gb_source_emacs_get_selection_bounds (emacs, &real_begin_iter, &real_end_iter);

  if (gtk_text_iter_compare(&selected_begin_iter, &real_begin_iter) != 0 &&
      gtk_text_iter_compare(&selected_end_iter, &real_end_iter) != 0)
    {
       gb_source_emacs_clear_selection_data(emacs);
    }
}

static void
gb_source_emacs_event_after_cb (GtkTextView *text_view,
                                GdkEventKey *event,
                                GbSourceEmacs *emacs)
{
  g_return_if_fail (GB_IS_SOURCE_EMACS (emacs));
}

static gboolean
gb_source_emacs_key_press_event_cb (GtkTextView *text_view,
                                    GdkEventKey *event,
                                    GbSourceEmacs *emacs)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (emacs)->priv;
  gboolean eval_cmd = FALSE;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GB_IS_SOURCE_EMACS (emacs), FALSE);

  gb_source_emacs_check_select_region(emacs);

  if (priv->select_region == TRUE && event->keyval >= GDK_KEY_Left && event->keyval <= GDK_KEY_Down)
    {
      g_string_append_printf(priv->cmd, "%s", gdk_keyval_name(event->keyval));
      eval_cmd = TRUE;
    }

  if ((event->keyval >= GDK_KEY_A && event->keyval <= GDK_KEY_Z) ||
      (event->keyval >= GDK_KEY_a && event->keyval <= GDK_KEY_z) ||
      (event->keyval == GDK_KEY_underscore) || (event->keyval == GDK_KEY_Escape ) ||
      (event->keyval == GDK_KEY_space)
     )
    {
      if (event->keyval == GDK_KEY_Escape)
        {
          if (priv->cmd->len != 0 )
            g_string_append_printf(priv->cmd, " ");
          g_string_append_printf(priv->cmd, "ESC");
          eval_cmd = TRUE;
        }
      else if (event->state == (GDK_CONTROL_MASK | GDK_MOD1_MASK))
        {
          if (priv->cmd->len != 0 )
            g_string_append_printf(priv->cmd, " ");
          g_string_append_printf(priv->cmd, "C-M-%s", gdk_keyval_name(event->keyval));
          eval_cmd = TRUE;
        }
      else if (event->state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
        {
          if (priv->cmd->len != 0 )
            g_string_append_printf(priv->cmd, " ");

          if (g_strcmp0(gdk_keyval_name(event->keyval), "underscore") == 0)
            g_string_append_printf(priv->cmd, "C-_");
          else
            g_string_append_printf(priv->cmd, "C-%s", gdk_keyval_name(event->keyval));

          eval_cmd = TRUE;
        }
      else if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          if (priv->cmd->len != 0 )
            g_string_append_printf(priv->cmd, " ");
          g_string_append_printf(priv->cmd, "C-%s", gdk_keyval_name(event->keyval));
          eval_cmd = TRUE;
        }
      else if ((event->state & GDK_MOD1_MASK) != 0)
        {
          if (priv->cmd->len != 0 )
            g_string_append_printf(priv->cmd, " ");
          g_string_append_printf(priv->cmd, "M-%s", gdk_keyval_name(event->keyval));
          eval_cmd = TRUE;
        }
      else
        {
          if (g_str_has_prefix(priv->cmd->str, "C-x") == TRUE)
            {
              if (priv->cmd->len != 0 )
                g_string_append_printf(priv->cmd, " ");
              g_string_append_printf(priv->cmd, "%s", gdk_keyval_name(event->keyval));
              eval_cmd = TRUE;
            }
        }
    }

  if (eval_cmd)
    return gb_source_emacs_eval_cmd(emacs);

  return FALSE;
}

static gboolean
gb_source_emacs_key_release_event_cb (GtkTextView *text_view,
                                      GdkEventKey *event,
                                      GbSourceEmacs *emacs)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GB_IS_SOURCE_EMACS (emacs), FALSE);

  return FALSE;
}

static void
gb_source_emacs_connect (GbSourceEmacs *emacs)
{
  g_return_if_fail (GB_IS_SOURCE_EMACS (emacs));
  g_return_if_fail (!emacs->priv->connected);

  emacs->priv->key_press_event_handler =
    g_signal_connect_object (emacs->priv->text_view,
                             "key-press-event",
                             G_CALLBACK (gb_source_emacs_key_press_event_cb),
                             emacs,
                             0);

  emacs->priv->event_after_handler =
    g_signal_connect_object (emacs->priv->text_view,
                             "event-after",
                             G_CALLBACK (gb_source_emacs_event_after_cb),
                             emacs,
                             0);

  emacs->priv->key_release_event_handler =
    g_signal_connect_object (emacs->priv->text_view,
                             "key-release-event",
                             G_CALLBACK (gb_source_emacs_key_release_event_cb),
                             emacs,
                             0);

  emacs->priv->connected = TRUE;
}

static void
gb_source_emacs_disconnect (GbSourceEmacs *emacs)
{
  g_return_if_fail (GB_IS_SOURCE_EMACS (emacs));
  g_return_if_fail (emacs->priv->connected);

  g_signal_handler_disconnect (emacs->priv->text_view,
                               emacs->priv->key_press_event_handler);
  emacs->priv->key_press_event_handler = 0;

  g_signal_handler_disconnect (emacs->priv->text_view,
                               emacs->priv->event_after_handler);
  emacs->priv->event_after_handler = 0;

  g_signal_handler_disconnect (emacs->priv->text_view,
                               emacs->priv->key_release_event_handler);
  emacs->priv->key_release_event_handler = 0;
  emacs->priv->connected = FALSE;
}

gboolean
gb_source_emacs_get_enabled (GbSourceEmacs *emacs)
{
  g_return_val_if_fail (GB_IS_SOURCE_EMACS (emacs), FALSE);

  return emacs->priv->enabled;
}

void
gb_source_emacs_set_enabled (GbSourceEmacs *emacs,
                           gboolean     enabled)
{
  GbSourceEmacsPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_EMACS (emacs));

  priv = emacs->priv;

  if (priv->enabled == enabled)
    return;

  if (enabled)
    {
      gb_source_emacs_connect (emacs);
      priv->enabled = TRUE;
    }
  else
    {
      gb_source_emacs_disconnect (emacs);
      priv->enabled = FALSE;
    }

  g_object_notify_by_pspec (G_OBJECT (emacs), gParamSpecs [PROP_ENABLED]);
}

GtkWidget *
gb_source_emacs_get_text_view (GbSourceEmacs *emacs)
{
  g_return_val_if_fail (GB_IS_SOURCE_EMACS (emacs), NULL);

  return (GtkWidget *)emacs->priv->text_view;
}

static void
gb_source_emacs_set_text_view (GbSourceEmacs *emacs,
                             GtkTextView *text_view)
{
  GbSourceEmacsPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_EMACS (emacs));
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = emacs->priv;

  if (priv->text_view == text_view)
    return;

  if (priv->text_view)
    {
      if (priv->enabled)
        gb_source_emacs_disconnect (emacs);
      g_object_remove_weak_pointer (G_OBJECT (priv->text_view),
                                    (gpointer *)&priv->text_view);
      priv->text_view = NULL;
    }

  if (text_view)
    {
      priv->text_view = text_view;
      g_object_add_weak_pointer (G_OBJECT (text_view),
                                 (gpointer *)&priv->text_view);
      if (priv->enabled)
        gb_source_emacs_connect (emacs);
    }

  g_object_notify_by_pspec (G_OBJECT (emacs), gParamSpecs [PROP_TEXT_VIEW]);
}

static void
gb_source_emacs_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbSourceEmacs *emacs = GB_SOURCE_EMACS (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, gb_source_emacs_get_enabled (emacs));
      break;

    case PROP_TEXT_VIEW:
      g_value_set_object (value, gb_source_emacs_get_text_view (emacs));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_emacs_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GbSourceEmacs *emacs = GB_SOURCE_EMACS (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      gb_source_emacs_set_enabled (emacs, g_value_get_boolean (value));
      break;

    case PROP_TEXT_VIEW:
      gb_source_emacs_set_text_view (emacs, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_emacs_finalize (GObject *object)
{
  GbSourceEmacsPrivate *priv = GB_SOURCE_EMACS (object)->priv;
  GtkTextBuffer *buffer;

	if (priv->text_view)
    {
      buffer = gtk_text_view_get_buffer (priv->text_view);

      if (priv->selection_begin != NULL)
        {
          gtk_text_buffer_delete_mark(buffer, priv->selection_begin);
          priv->selection_begin = NULL;
        }

      if (priv->selection_end != NULL)
        {
          gtk_text_buffer_delete_mark(buffer, priv->selection_end);
          priv->selection_end = NULL;
        }

      gb_source_emacs_disconnect (GB_SOURCE_EMACS (object));
      g_object_remove_weak_pointer (G_OBJECT (priv->text_view),
                                    (gpointer *)&priv->text_view);
      priv->text_view = NULL;
    }
  if (priv->cmd != NULL)
    g_string_free(priv->cmd, TRUE);

	G_OBJECT_CLASS (gb_source_emacs_parent_class)->finalize (object);
}

static void
gb_source_emacs_class_register_command (GbSourceEmacsClass          *klass,
                                        GRegex                      *matcher,
                                        GbSourceEmacsCommandFlags   flags,
                                        GbSourceEmacsCommandFunc    func)
{
  GbSourceEmacsCommand *cmd;

  g_assert (GB_IS_SOURCE_EMACS_CLASS (klass));

  cmd = g_new0 (GbSourceEmacsCommand, 1);
  cmd->matcher = matcher;
  cmd->func = func;
  cmd->flags = flags;

  gCommands = g_list_append(gCommands, cmd);
}


static void
gb_source_emacs_class_init (GbSourceEmacsClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_source_emacs_finalize;
  object_class->get_property = gb_source_emacs_get_property;
  object_class->set_property = gb_source_emacs_set_property;

  gParamSpecs [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          _("Enabled"),
                          _("If the EMACS engine is enabled."),
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ENABLED,
                                   gParamSpecs [PROP_ENABLED]);

  gParamSpecs [PROP_TEXT_VIEW] =
    g_param_spec_object ("text-view",
                         _("Text View"),
                         _("The text view the EMACS engine is managing."),
                         GTK_TYPE_TEXT_VIEW,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TEXT_VIEW,
                                   gParamSpecs [PROP_TEXT_VIEW]);

  /* Register emacs commands */
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("C-g$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_exit_from_command_line);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("ESC ESC ESC$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_exit_from_command_line);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-space$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_select_region);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-w$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_cut);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^M-w$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_copy);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-y$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_yank);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^Up$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_select_region_go_up);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^Down$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_select_region_go_down);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^Left$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_select_region_go_left);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^Right$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_select_region_go_right);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-x C-c$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_exit);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-x k$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_close_document);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-x C-f$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_open_file);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-x C-s$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_save_file);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-x s$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_save_all);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-s$", 0, 0, NULL ),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_find);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-x C-w$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_save_file_as);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-_$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_undo);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-x u$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_redo);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-f$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_move_forward_char);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-b$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_move_backward_char);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^C-d$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_delete_forward_char);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^M-f$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_move_forward_word);
  gb_source_emacs_class_register_command (klass,
                                          g_regex_new("^M-b$", 0, 0, NULL),
                                          GB_SOURCE_EMACS_COMMAND_FLAG_NONE,
                                          gb_source_emacs_cmd_move_backward_word);

}

static void
gb_source_emacs_init (GbSourceEmacs *emacs)
{
  emacs->priv = gb_source_emacs_get_instance_private (emacs);
  emacs->priv->enabled = FALSE;
  emacs->priv->select_region = FALSE;
  emacs->priv->selection_begin = NULL;
  emacs->priv->selection_end = NULL;
  emacs->priv->selection_line_offset = 0;
  emacs->priv->cmd = g_string_new(NULL);
}


