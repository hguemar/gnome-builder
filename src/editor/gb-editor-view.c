/* gb-editor-view.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "editor-view"

#include <glib/gi18n.h>

#include "gb-animation.h"
#include "gb-editor-frame.h"
#include "gb-editor-frame-private.h"
#include "gb-editor-tweak-widget.h"
#include "gb-editor-view.h"
#include "gb-glib.h"
#include "gb-html-document.h"
#include "gb-log.h"
#include "gb-string.h"
#include "gb-widget.h"

struct _GbEditorViewPrivate
{
  /* References owned by view */
  GbEditorDocument *document;

  /* Weak references */
  GbAnimation     *progress_anim;

  /* References owned by GtkWidget template */
  GtkPaned        *paned;
  GtkToggleButton *split_button;
  GbEditorFrame   *frame;
  GtkProgressBar  *progress_bar;
  GtkLabel        *error_label;
  GtkButton       *error_close_button;
  GtkRevealer     *error_revealer;
  GtkLabel        *modified_label;
  GtkButton       *modified_reload_button;
  GtkButton       *modified_cancel_button;
  GtkRevealer     *modified_revealer;
  GtkMenuButton   *tweak_button;
  GtkMenuButton   *tweak_widget;

  guint8           tab_width;
  guint            auto_indent : 1;
  guint            highlight_current_line : 1;
  guint            show_line_numbers : 1;
  guint            show_right_margin : 1;
  guint            use_spaces : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorView, gb_editor_view, GB_TYPE_DOCUMENT_VIEW)

enum {
  PROP_0,
  PROP_AUTO_INDENT,
  PROP_DOCUMENT,
  PROP_HIGHLIGHT_CURRENT_LINE,
  PROP_SHOW_LINE_NUMBERS,
  PROP_SHOW_RIGHT_MARGIN,
  PROP_SPLIT_ENABLED,
  PROP_TAB_WIDTH,
  PROP_USE_SPACES,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void gb_editor_view_toggle_split (GbEditorView *view);

GtkWidget *
gb_editor_view_new (GbEditorDocument *document)
{
  return g_object_new (GB_TYPE_EDITOR_VIEW,
                       "document", document,
                       NULL);
}

static void
gb_editor_view_action_set_state (GbEditorView *view,
                                 const gchar  *action_name,
                                 GVariant     *state)
{
  GActionGroup *group;
  GAction *action;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (action_name);
  g_return_if_fail (state);

  group = gtk_widget_get_action_group (GTK_WIDGET (view), "editor-view");
  action = g_action_map_lookup_action (G_ACTION_MAP (group), action_name);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), state);
}

static void
apply_state_language (GSimpleAction *action,
                      GVariant      *param,
                      gpointer       user_data)
{
  GbEditorView *view = user_data;
  GtkSourceLanguage *l = NULL;
  const gchar *lang_id;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  lang_id = g_variant_get_string (param, NULL);

  if (!gb_str_empty0 (lang_id))
    {
      GtkSourceLanguageManager *m;

      m = gtk_source_language_manager_get_default ();
      l = gtk_source_language_manager_get_language (m, lang_id);
    }

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (view->priv->document), l);
}

gboolean
gb_editor_view_get_auto_indent (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  return view->priv->auto_indent;
}

void
gb_editor_view_set_auto_indent (GbEditorView *view,
                               gboolean      auto_indent)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  view->priv->auto_indent = !!auto_indent;
  gb_editor_view_action_set_state (view, "auto-indent",
                                   g_variant_new_boolean (auto_indent));
  g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs [PROP_AUTO_INDENT]);
}

gboolean
gb_editor_view_get_highlight_current_line (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  return view->priv->highlight_current_line;
}

void
gb_editor_view_set_highlight_current_line (GbEditorView *view,
                                           gboolean      highlight_current_line)
{
  GVariant *variant;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  view->priv->highlight_current_line = !!highlight_current_line;
  variant = g_variant_new_boolean (highlight_current_line);
  gb_editor_view_action_set_state (view, "highlight-current-line", variant);
  g_object_notify_by_pspec (G_OBJECT (view),
                            gParamSpecs [PROP_HIGHLIGHT_CURRENT_LINE]);
}

gboolean
gb_editor_view_get_show_right_margin (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  return view->priv->show_right_margin;
}

void
gb_editor_view_set_show_right_margin (GbEditorView *view,
                                      gboolean      show_right_margin)
{
  GVariant *variant;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  view->priv->show_right_margin = !!show_right_margin;
  variant = g_variant_new_boolean (show_right_margin);
  gb_editor_view_action_set_state (view, "show-right-margin", variant);
  g_object_notify_by_pspec (G_OBJECT (view),
                            gParamSpecs [PROP_SHOW_RIGHT_MARGIN]);
}

gboolean
gb_editor_view_get_show_line_numbers (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  return view->priv->show_line_numbers;
}

void
gb_editor_view_set_show_line_numbers (GbEditorView *view,
                                      gboolean      show_line_numbers)
{
  GVariant *variant;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  view->priv->show_line_numbers = !!show_line_numbers;
  variant = g_variant_new_boolean (show_line_numbers);
  gb_editor_view_action_set_state (view, "show-line-numbers", variant);
  g_object_notify_by_pspec (G_OBJECT (view),
                            gParamSpecs [PROP_SHOW_LINE_NUMBERS]);
}

guint
gb_editor_view_get_tab_width (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), 0);

  return view->priv->tab_width;
}

void
gb_editor_view_set_tab_width (GbEditorView *view,
                              guint         tab_width)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (tab_width >= 1);
  g_return_if_fail (tab_width <= 32);

  if (tab_width != view->priv->tab_width)
    {
      view->priv->tab_width = tab_width;
      gb_editor_view_action_set_state (view, "tab-width",
                                       g_variant_new_int32 (tab_width));
      g_object_notify_by_pspec (G_OBJECT (view),
                                gParamSpecs [PROP_TAB_WIDTH]);
    }
}

gboolean
gb_editor_view_get_use_spaces (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  return view->priv->use_spaces;
}

void
gb_editor_view_set_use_spaces (GbEditorView *view,
                               gboolean      use_spaces)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  view->priv->use_spaces = !!use_spaces;
  gb_editor_view_action_set_state (view, "use-spaces",
                                   g_variant_new_boolean (use_spaces));
  g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs [PROP_USE_SPACES]);
}

static void
gb_editor_view_notify_language (GbEditorView     *view,
                                GParamSpec       *pspec,
                                GbEditorDocument *document)
{
  GtkSourceLanguage *language;
  const gchar *lang_id = "";

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  g_object_notify (G_OBJECT (view), "can-preview");

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (document));
  if (language)
    lang_id = gtk_source_language_get_id (language);

  gb_editor_view_action_set_state (view, "language",
                                   g_variant_new_string (lang_id));
}

static void
gb_editor_view_notify_progress (GbEditorView     *view,
                                GParamSpec       *pspec,
                                GbEditorDocument *document)
{
  GbEditorViewPrivate *priv;
  gdouble progress;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  priv = view->priv;

  progress = gb_editor_document_get_progress (document);

  if (!gtk_widget_get_visible (GTK_WIDGET (priv->progress_bar)))
    {
      gtk_progress_bar_set_fraction (priv->progress_bar, 0.0);
      gtk_widget_set_opacity (GTK_WIDGET (priv->progress_bar), 1.0);
      gtk_widget_show (GTK_WIDGET (priv->progress_bar));
    }

  if (priv->progress_anim)
    gb_animation_stop (priv->progress_anim);

  gb_clear_weak_pointer (&priv->progress_anim);

  priv->progress_anim = gb_object_animate (priv->progress_bar,
                                           GB_ANIMATION_LINEAR,
                                           250,
                                           NULL,
                                           "fraction", progress,
                                           NULL);
  gb_set_weak_pointer (priv->progress_anim, &priv->progress_anim);

  if (progress == 1.0)
    gb_widget_fade_hide (GTK_WIDGET (priv->progress_bar));
}

static gboolean
gb_editor_view_get_can_preview (GbDocumentView *view)
{
  GbEditorViewPrivate *priv;
  GtkSourceLanguage *language;
  GtkSourceBuffer *buffer;
  const gchar *lang_id;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  priv = GB_EDITOR_VIEW (view)->priv;

  buffer = GTK_SOURCE_BUFFER (priv->document);
  language = gtk_source_buffer_get_language (buffer);
  if (!language)
    return FALSE;

  lang_id = gtk_source_language_get_id (language);
  if (!lang_id)
    return FALSE;

  return (g_str_equal (lang_id, "html") ||
          g_str_equal (lang_id, "markdown"));
}

/**
 * gb_editor_view_create_preview:
 * @view: A #GbEditorView.
 *
 * Creates a new document that can be previewed by calling
 * gb_document_create_view() on the document.
 *
 * Returns: (transfer full): A #GbDocument.
 */
static GbDocument *
gb_editor_view_create_preview (GbDocumentView *view)
{
  GbEditorView *self = (GbEditorView *)view;
  GbDocument *document;
  GbHtmlDocumentTransform transform = NULL;
  GtkSourceBuffer *buffer;
  GtkSourceLanguage *language;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (self), NULL);

  buffer = GTK_SOURCE_BUFFER (self->priv->document);
  language = gtk_source_buffer_get_language (buffer);

  if (language)
    {
      const gchar *lang_id;

      lang_id = gtk_source_language_get_id (language);

      if (g_strcmp0 (lang_id, "markdown") == 0)
        transform = gb_html_markdown_transform;
    }

  document = g_object_new (GB_TYPE_HTML_DOCUMENT,
                           "buffer", buffer,
                           NULL);

  if (transform)
    gb_html_document_set_transform_func (GB_HTML_DOCUMENT (document),
                                         transform);

  return document;
}

GbEditorFrame *
gb_editor_view_get_frame1 (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), NULL);

  return view->priv->frame;
}

GbEditorFrame *
gb_editor_view_get_frame2 (GbEditorView *view)
{
  GtkWidget *child2;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), NULL);

  child2 = gtk_paned_get_child2 (view->priv->paned);
  if (GB_IS_EDITOR_FRAME (child2))
    return GB_EDITOR_FRAME (child2);

  return NULL;
}

static void
gb_editor_view_hide_revealer_child (GtkRevealer *revealer)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  gtk_revealer_set_reveal_child (revealer, FALSE);
}

static void
gb_editor_view_file_changed_on_volume (GbEditorView     *view,
                                       GParamSpec       *pspec,
                                       GbEditorDocument *document)
{
  GtkSourceFile *source_file;
  GFile *location;
  gchar *path;
  gchar *str;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  source_file = gb_editor_document_get_file (document);
  location = gtk_source_file_get_location (source_file);

  if (!location)
    return;

  if (g_file_is_native (location))
    path = g_file_get_path (location);
  else
    path = g_file_get_uri (location);

  str = g_strdup_printf (_("The file “%s” was modified outside of Builder."),
                         path);

  gtk_label_set_label (view->priv->modified_label, str);
  gtk_revealer_set_reveal_child (view->priv->modified_revealer, TRUE);

  g_free (path);
  g_free (str);
}

static void
gb_editor_view_reload_document (GbEditorView *view,
                                GtkButton    *button)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  gb_editor_document_reload (view->priv->document);
  gtk_revealer_set_reveal_child (view->priv->modified_revealer, FALSE);
}

static void
gb_editor_view_notify_error (GbEditorView     *view,
                             GParamSpec       *pspec,
                             GbEditorDocument *document)
{
  const GError *error;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (pspec);
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  error = gb_editor_document_get_error (document);

  /* Ignore file not found errors */
  if (error &&
      (error->domain == G_IO_ERROR) &&
      (error->code == G_IO_ERROR_NOT_FOUND))
    error = NULL;

  if (!error)
    {
      if (gtk_revealer_get_reveal_child (view->priv->error_revealer))
        gtk_revealer_set_reveal_child (view->priv->error_revealer, FALSE);
    }
  else
    {
      gtk_label_set_label (view->priv->error_label, error->message);
      gtk_revealer_set_reveal_child (view->priv->error_revealer, TRUE);
    }
}

static gboolean
transform_language_to_string (GBinding     *binding,
                              const GValue *from_value,
                              GValue       *to_value,
                              gpointer      user_data)
{
  GtkSourceLanguage *language;
  const gchar *str = _("Plain Text");

  language = g_value_get_object (from_value);
  if (language)
    str = gtk_source_language_get_name (language);
  g_value_set_string (to_value, str);

  return TRUE;
}

static void
gb_editor_view_connect (GbEditorView     *view,
                        GbEditorDocument *document)
{
  GtkWidget *child2;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  gb_editor_frame_set_document (view->priv->frame, document);

  child2 = gtk_paned_get_child2 (view->priv->paned);
  if (GB_IS_EDITOR_FRAME (child2))
    gb_editor_frame_set_document (GB_EDITOR_FRAME (child2), document);

  g_signal_connect_object (document,
                           "notify::language",
                           G_CALLBACK (gb_editor_view_notify_language),
                           view,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (document,
                           "notify::progress",
                           G_CALLBACK (gb_editor_view_notify_progress),
                           view,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (view->priv->modified_cancel_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_hide_revealer_child),
                           view->priv->modified_revealer,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (view->priv->modified_reload_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_reload_document),
                           view,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (document,
                           "notify::error",
                           G_CALLBACK (gb_editor_view_notify_error),
                           view,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (view->priv->error_close_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_hide_revealer_child),
                           view->priv->error_revealer,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (document,
                           "notify::file-changed-on-volume",
                           G_CALLBACK (gb_editor_view_file_changed_on_volume),
                           view,
                           G_CONNECT_SWAPPED);

  g_object_bind_property_full (document, "language",
                               view->priv->tweak_button, "label",
                               G_BINDING_SYNC_CREATE,
                               transform_language_to_string,
                               NULL, NULL, NULL);
}

static void
gb_editor_view_disconnect (GbEditorView     *view,
                           GbEditorDocument *document)
{
  GtkWidget *child2;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  gb_editor_frame_set_document (view->priv->frame, NULL);

  child2 = gtk_paned_get_child2 (view->priv->paned);
  if (GB_IS_EDITOR_FRAME (child2))
    gb_editor_frame_set_document (GB_EDITOR_FRAME (child2), document);

  g_signal_handlers_disconnect_by_func (document,
                                        G_CALLBACK (gb_editor_view_notify_language),
                                        view);
  g_signal_handlers_disconnect_by_func (document,
                                        G_CALLBACK (gb_editor_view_notify_progress),
                                        view);
}

static GbDocument *
gb_editor_view_get_document (GbDocumentView *view)
{
  GbEditorViewPrivate *priv;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), NULL);

  priv = GB_EDITOR_VIEW (view)->priv;

  return GB_DOCUMENT (priv->document);
}

static void
gb_editor_view_set_document (GbEditorView     *view,
                             GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  if (document != view->priv->document)
    {
      if (view->priv->document)
        {
          gb_editor_view_disconnect (view, document);
          g_clear_object (&view->priv->document);
        }

      if (document)
        {
          view->priv->document = g_object_ref (document);
          gb_editor_view_connect (view, document);
        }

      g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs [PROP_DOCUMENT]);
    }
}

static void
gb_editor_view_switch_pane (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  GbEditorView *view = user_data;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  if (!gtk_widget_has_focus (GTK_WIDGET (view->priv->frame->priv->source_view)))
    gtk_widget_grab_focus (GTK_WIDGET (view->priv->frame));
  else
    {
      GtkWidget *child2;

      child2 = gtk_paned_get_child2 (view->priv->paned);
      if (child2)
        gtk_widget_grab_focus (child2);
    }

  EXIT;
}

static gboolean
gb_editor_view_on_execute_command (GbEditorView *self,
                                   const gchar  *command_text,
                                   GbSourceVim  *vim)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (self), FALSE);
  g_return_val_if_fail (command_text, FALSE);
  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), FALSE);

  if (g_str_equal (command_text, "w"))
    {
      gb_widget_activate_action (GTK_WIDGET (self), "stack", "save", NULL);
      return TRUE;
    }
  else if (g_str_equal (command_text, "wq"))
    {
      gb_widget_activate_action (GTK_WIDGET (self), "stack", "save", NULL);
      gb_widget_activate_action (GTK_WIDGET (self), "stack", "close", NULL);
      return TRUE;
    }
  else if (g_str_equal (command_text, "q"))
    {
      if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (self->priv->document)))
        {
          /* TODO: Plumb warning message */
        }
      else
        gb_widget_activate_action (GTK_WIDGET (self), "stack", "close", NULL);
      return TRUE;
    }
  else if (g_str_equal (command_text, "q!"))
    {
      /* TODO: don't prompt about saving */
      gb_widget_activate_action (GTK_WIDGET (self), "stack", "close", NULL);
      return TRUE;
    }

  return FALSE;
}

static gboolean
gb_editor_view_on_vim_split (GbEditorView     *self,
                             GbSourceVimSplit  split,
                             GbSourceVim      *vim)
{
  GtkWidget *toplevel;
  GtkWidget *focus = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (self), FALSE);
  g_return_val_if_fail (split, FALSE);
  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), FALSE);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

  switch (split)
    {
    case GB_SOURCE_VIM_SPLIT_HORIZONTAL:
      focus = gtk_window_get_focus (GTK_WINDOW (toplevel));
      if (!gb_editor_view_get_split_enabled (self))
        {
          gb_editor_view_toggle_split (self);
          ret = TRUE;
        }
      break;

    case GB_SOURCE_VIM_SPLIT_VERTICAL:
      focus = gtk_window_get_focus (GTK_WINDOW (toplevel));
      gb_widget_activate_action (GTK_WIDGET (self),
                                 "stack", "split-document-right",
                                 NULL);
      ret = TRUE;
      break;

    case GB_SOURCE_VIM_SPLIT_CYCLE_NEXT:
      if (gb_editor_view_get_split_enabled (self) &&
          gtk_widget_has_focus (GTK_WIDGET (self->priv->frame->priv->source_view)))
        gb_editor_view_switch_pane (NULL, NULL, self);
      else
        gb_widget_activate_action (GTK_WIDGET (self), "stack", "focus-right",
                                   NULL);
      break;

    case GB_SOURCE_VIM_SPLIT_CYCLE_PREVIOUS:
      {
        GbEditorFrame *frame2;

        frame2 = gb_editor_view_get_frame2 (self);

        if (frame2 &&
            gtk_widget_has_focus (GTK_WIDGET (frame2->priv->source_view)))
          gb_editor_view_switch_pane (NULL, NULL, self);
        else
          gb_widget_activate_action (GTK_WIDGET (self), "stack", "focus-left",
                                     NULL);
      }
      break;

    case GB_SOURCE_VIM_SPLIT_CLOSE:
      if (gb_editor_view_get_split_enabled (self))
        {
          /*
           * TODO: copy state from frame2 to frame1 if frame2 was focused.
           */
          gb_editor_view_toggle_split (self);
          ret = TRUE;
        }
      else
        {
          gb_widget_activate_action (GTK_WIDGET (self), "stack", "close", NULL);
          ret = TRUE;
        }
      break;

    default:
      break;
    }

  if (focus)
    gtk_widget_grab_focus (focus);

  return ret;
}

static void
gb_editor_view_toggle_split (GbEditorView *view)
{
  GbEditorViewPrivate *priv;
  GtkWidget *child2;
  gboolean active;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  priv = view->priv;

  if ((child2 = gtk_paned_get_child2 (priv->paned)))
    {
      gtk_widget_destroy (child2);
      gtk_widget_grab_focus (GTK_WIDGET (priv->frame));
      active = FALSE;
    }
  else
    {
      GbSourceVim *vim;

      child2 = g_object_new (GB_TYPE_EDITOR_FRAME,
                             "document", view->priv->document,
                             "visible", TRUE,
                             NULL);
      vim = gb_source_view_get_vim (GB_EDITOR_FRAME (child2)->priv->source_view);
      g_signal_connect_object (vim,
                               "execute-command",
                               G_CALLBACK (gb_editor_view_on_execute_command),
                               view,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (vim,
                               "split",
                               G_CALLBACK (gb_editor_view_on_vim_split),
                               view,
                               G_CONNECT_SWAPPED);

      g_object_bind_property (GB_EDITOR_FRAME (child2),
                              "search-direction",
                              vim, "search-direction",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (GB_EDITOR_FRAME (child2)->priv->search_settings,
                              "search-text",
                              vim, "search-text",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (view, "auto-indent",
                              GB_EDITOR_FRAME (child2)->priv->source_view,
                              "auto-indent",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      g_object_bind_property (view, "highlight-current-line",
                              GB_EDITOR_FRAME (child2)->priv->source_view,
                              "highlight-current-line",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      g_object_bind_property (view, "show-line-numbers",
                              GB_EDITOR_FRAME (child2)->priv->source_view,
                              "show-line-numbers",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      g_object_bind_property (view, "show-right-margin",
                              GB_EDITOR_FRAME (child2)->priv->source_view,
                              "show-right-margin",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      g_object_bind_property (view, "tab-width",
                              GB_EDITOR_FRAME (child2)->priv->source_view,
                              "tab-width",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      g_object_bind_property (view, "use-spaces",
                              GB_EDITOR_FRAME (child2)->priv->source_view,
                              "insert-spaces-instead-of-tabs",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_container_add_with_properties (GTK_CONTAINER (priv->paned), child2,
                                         "shrink", TRUE,
                                         "resize", TRUE,
                                         NULL);
      gtk_widget_grab_focus (child2);
      active = TRUE;
    }

  gb_editor_view_action_set_state (view, "toggle-split",
                                   g_variant_new_boolean (active));

  EXIT;
}

gboolean
gb_editor_view_get_split_enabled (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  return !!gb_editor_view_get_frame2 (view);
}

void
gb_editor_view_set_split_enabled (GbEditorView *view,
                                  gboolean      split_enabled)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  if (split_enabled == gb_editor_view_get_split_enabled (view))
    return;

  gb_editor_view_toggle_split (view);
  g_object_notify_by_pspec (G_OBJECT (view),
                            gParamSpecs [PROP_SPLIT_ENABLED]);
}

static void
gb_editor_view_grab_focus (GtkWidget *widget)
{
  GbEditorView *view = (GbEditorView *)widget;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  gtk_widget_grab_focus (GTK_WIDGET (view->priv->frame));

  EXIT;
}

#define STATE_HANDLER_BOOLEAN(name) \
  static void \
  apply_state_##name (GSimpleAction *action, \
                      GVariant      *param, \
                      gpointer       user_data) \
  { \
    GbEditorView *view = user_data; \
    gboolean value; \
    g_return_if_fail (GB_IS_EDITOR_VIEW (view)); \
    value = g_variant_get_boolean (param); \
    gb_editor_view_set_##name (view, value); \
  }

#define STATE_HANDLER_INT(name) \
  static void \
  apply_state_##name (GSimpleAction *action, \
                      GVariant      *param, \
                      gpointer       user_data) \
  { \
    GbEditorView *view = user_data; \
    guint value; \
    g_return_if_fail (GB_IS_EDITOR_VIEW (view)); \
    value = g_variant_get_int32 (param); \
    gb_editor_view_set_##name (view, value); \
  }

STATE_HANDLER_BOOLEAN (auto_indent)
STATE_HANDLER_BOOLEAN (highlight_current_line)
STATE_HANDLER_BOOLEAN (show_line_numbers)
STATE_HANDLER_BOOLEAN (show_right_margin)
STATE_HANDLER_BOOLEAN (split_enabled)
STATE_HANDLER_INT     (tab_width)
STATE_HANDLER_BOOLEAN (use_spaces)

static void
gb_editor_view_finalize (GObject *object)
{
  GbEditorView *view = (GbEditorView *)object;

  g_clear_object (&view->priv->document);

  G_OBJECT_CLASS (gb_editor_view_parent_class)->finalize (object);
}

static void
gb_editor_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      g_value_set_boolean (value, gb_editor_view_get_auto_indent (self));
      break;

    case PROP_DOCUMENT:
      g_value_set_object (value, self->priv->document);
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      g_value_set_boolean (value,
                           gb_editor_view_get_highlight_current_line (self));
      break;

    case PROP_SHOW_LINE_NUMBERS:
      g_value_set_boolean (value, gb_editor_view_get_show_line_numbers (self));
      break;

    case PROP_SHOW_RIGHT_MARGIN:
      g_value_set_boolean (value, gb_editor_view_get_show_right_margin (self));
      break;

    case PROP_SPLIT_ENABLED:
      g_value_set_boolean (value, gb_editor_view_get_split_enabled (self));
      break;

    case PROP_TAB_WIDTH:
      g_value_set_uint (value, gb_editor_view_get_tab_width (self));
      break;

    case PROP_USE_SPACES:
      g_value_set_boolean (value, gb_editor_view_get_use_spaces (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      gb_editor_view_set_auto_indent (self, g_value_get_boolean (value));
      break;

    case PROP_DOCUMENT:
      gb_editor_view_set_document (self, g_value_get_object (value));
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      gb_editor_view_set_highlight_current_line (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_NUMBERS:
      gb_editor_view_set_show_line_numbers (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_RIGHT_MARGIN:
      gb_editor_view_set_show_right_margin (self, g_value_get_boolean (value));
      break;

    case PROP_SPLIT_ENABLED:
      gb_editor_view_set_split_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_TAB_WIDTH:
      gb_editor_view_set_tab_width (self, g_value_get_uint (value));
      break;

    case PROP_USE_SPACES:
      gb_editor_view_set_use_spaces (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_class_init (GbEditorViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbDocumentViewClass *view_class = GB_DOCUMENT_VIEW_CLASS (klass);

  object_class->finalize = gb_editor_view_finalize;
  object_class->get_property = gb_editor_view_get_property;
  object_class->set_property = gb_editor_view_set_property;

  widget_class->grab_focus = gb_editor_view_grab_focus;

  view_class->get_document = gb_editor_view_get_document;
  view_class->get_can_preview = gb_editor_view_get_can_preview;
  view_class->create_preview = gb_editor_view_create_preview;

  gParamSpecs [PROP_AUTO_INDENT] =
    g_param_spec_boolean ("auto-indent",
                         _("Auto Indent"),
                         _("If we should use the auto-indentation engine."),
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_AUTO_INDENT,
                                   gParamSpecs [PROP_AUTO_INDENT]);

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The document edited by the view."),
                         GB_TYPE_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT,
                                   gParamSpecs [PROP_DOCUMENT]);

  gParamSpecs [PROP_HIGHLIGHT_CURRENT_LINE] =
    g_param_spec_boolean ("highlight-current-line",
                          _("Highlight Current Line"),
                          _("If the current line should be highlighted."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_HIGHLIGHT_CURRENT_LINE,
                                   gParamSpecs [PROP_HIGHLIGHT_CURRENT_LINE]);

  gParamSpecs [PROP_SHOW_LINE_NUMBERS] =
    g_param_spec_boolean ("show-line-numbers",
                         _("Show Line Numbers"),
                         _("If the line numbers should be shown."),
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_LINE_NUMBERS,
                                   gParamSpecs [PROP_SHOW_LINE_NUMBERS]);

  gParamSpecs [PROP_SHOW_RIGHT_MARGIN] =
    g_param_spec_boolean ("show-right-margin",
                         _("Show Right Margin"),
                         _("If we should show the right margin."),
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_RIGHT_MARGIN,
                                   gParamSpecs [PROP_SHOW_RIGHT_MARGIN]);

  gParamSpecs [PROP_SPLIT_ENABLED] =
    g_param_spec_boolean ("split-enabled",
                         _("Split Enabled"),
                         _("If the view split is enabled."),
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SPLIT_ENABLED,
                                   gParamSpecs [PROP_SPLIT_ENABLED]);

  gParamSpecs [PROP_TAB_WIDTH] =
    g_param_spec_uint ("tab-width",
                         _("Tab Width"),
                         _("The width a tab should be drawn as."),
                         1,
                         32,
                         8,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TAB_WIDTH,
                                   gParamSpecs [PROP_TAB_WIDTH]);

  gParamSpecs [PROP_USE_SPACES] =
    g_param_spec_boolean ("use-spaces",
                         _("Use Spaces"),
                         _("If spaces should be used instead of tabs."),
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_USE_SPACES,
                                   gParamSpecs [PROP_USE_SPACES]);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-view.ui");
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, frame);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, paned);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, progress_bar);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, split_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, tweak_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, tweak_widget);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, modified_revealer);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, modified_label);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, modified_cancel_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, modified_reload_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, error_label);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, error_revealer);
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, error_close_button);

  g_type_ensure (GB_TYPE_EDITOR_FRAME);
  g_type_ensure (GB_TYPE_EDITOR_TWEAK_WIDGET);
}

static void
gb_editor_view_init (GbEditorView *self)
{
  const GActionEntry entries[] = {
    { "auto-indent", NULL, NULL, "false", apply_state_auto_indent },
    { "highlight-current-line", NULL, NULL, "false",
      apply_state_highlight_current_line },
    { "language", NULL, "s", "''", apply_state_language },
    { "show-line-numbers", NULL, NULL, "false", apply_state_show_line_numbers },
    { "show-right-margin", NULL, NULL, "false", apply_state_show_right_margin },
    { "switch-pane",  gb_editor_view_switch_pane },
    { "tab-width", NULL, "i", "8", apply_state_tab_width },
    { "toggle-split", NULL, NULL, "false", apply_state_split_enabled },
    { "use-spaces", NULL, "b", "false", apply_state_use_spaces },
  };
  GSimpleActionGroup *actions;
  GbSourceVim *vim;
  GtkWidget *controls;

  self->priv = gb_editor_view_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  actions = g_simple_action_group_new ();

  /*
   * Unfortunately, we need to manually attach the action group in
   * a few places due to the complexity of how they are handled.
   */
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries,
                                   G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor-view",
                                  G_ACTION_GROUP (actions));
  controls = gb_document_view_get_controls (GB_DOCUMENT_VIEW (self));
  gtk_widget_insert_action_group (GTK_WIDGET (controls), "editor-view",
                                  G_ACTION_GROUP (actions));
  gtk_widget_insert_action_group (GTK_WIDGET (self->priv->tweak_widget),
                                  "editor-view", G_ACTION_GROUP (actions));

  g_clear_object (&actions);

  vim = gb_source_view_get_vim (self->priv->frame->priv->source_view);
  g_signal_connect_object (vim,
                           "execute-command",
                           G_CALLBACK (gb_editor_view_on_execute_command),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (vim,
                           "split",
                           G_CALLBACK (gb_editor_view_on_vim_split),
                           self,
                           G_CONNECT_SWAPPED);
  g_object_bind_property (self->priv->frame->priv->source_view, "auto-indent",
                          self, "auto-indent",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->priv->frame->priv->source_view,
                          "highlight-current-line",
                          self, "highlight-current-line",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->priv->frame->priv->source_view,
                          "show-line-numbers",
                          self, "show-line-numbers",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->priv->frame->priv->source_view,
                          "show-right-margin",
                          self, "show-right-margin",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->priv->frame->priv->source_view,
                          "tab-width",
                          self, "tab-width",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (self->priv->frame->priv->source_view,
                          "insert-spaces-instead-of-tabs",
                          self, "use-spaces",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self->priv->frame,
                          "search-direction",
                          vim, "search-direction",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->priv->frame->priv->search_settings,
                          "search-text",
                          vim, "search-text",
                          G_BINDING_SYNC_CREATE);
}
