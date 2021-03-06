bin_PROGRAMS += gnome-builder
noinst_LTLIBRARIES += libgnome-builder.la

libgnome_builder_la_SOURCES = \
	$(gnome_builder_built_sources) \
	src/animation/gb-animation.c \
	src/animation/gb-animation.h \
	src/animation/gb-frame-source.c \
	src/animation/gb-frame-source.h \
	src/app/gb-application.c \
	src/app/gb-application.h \
	src/auto-indent/c-parse-helper.c \
	src/auto-indent/c-parse-helper.h \
	src/auto-indent/gb-source-auto-indenter-c.c \
	src/auto-indent/gb-source-auto-indenter-c.h \
	src/auto-indent/gb-source-auto-indenter-python.c \
	src/auto-indent/gb-source-auto-indenter-python.h \
	src/auto-indent/gb-source-auto-indenter-xml.c \
	src/auto-indent/gb-source-auto-indenter-xml.h \
	src/auto-indent/gb-source-auto-indenter.c \
	src/auto-indent/gb-source-auto-indenter.h \
	src/code-assistant/gb-source-code-assistant-renderer.c \
	src/code-assistant/gb-source-code-assistant-renderer.h \
	src/code-assistant/gb-source-code-assistant.c \
	src/code-assistant/gb-source-code-assistant.h \
	src/commands/gb-command-bar-item.c \
	src/commands/gb-command-bar-item.h \
	src/commands/gb-command-bar.c \
	src/commands/gb-command-bar.h \
	src/commands/gb-command-gaction-provider.c \
	src/commands/gb-command-gaction-provider.h \
	src/commands/gb-command-gaction.c \
	src/commands/gb-command-gaction.h \
	src/commands/gb-command-manager.c \
	src/commands/gb-command-manager.h \
	src/commands/gb-command-provider.c \
	src/commands/gb-command-provider.h \
	src/commands/gb-command-result.c \
	src/commands/gb-command-result.h \
	src/commands/gb-command-vim-provider.c \
	src/commands/gb-command-vim-provider.h \
	src/commands/gb-command-vim.c \
	src/commands/gb-command-vim.h \
	src/commands/gb-command.c \
	src/commands/gb-command.h \
	src/credits/gb-credits-widget.c \
	src/credits/gb-credits-widget.h \
	src/devhelp/gb-devhelp-document.c \
	src/devhelp/gb-devhelp-document.h \
	src/devhelp/gb-devhelp-view.c \
	src/devhelp/gb-devhelp-view.h \
	src/dialogs/gb-close-confirmation-dialog.c \
	src/dialogs/gb-close-confirmation-dialog.h \
	src/documents/gb-document-grid.c \
	src/documents/gb-document-grid.h \
	src/documents/gb-document-manager.c \
	src/documents/gb-document-manager.h \
	src/documents/gb-document-menu-button.c \
	src/documents/gb-document-menu-button.h \
	src/documents/gb-document-private.h \
	src/documents/gb-document-split.c \
	src/documents/gb-document-split.h \
	src/documents/gb-document-stack.c \
	src/documents/gb-document-stack.h \
	src/documents/gb-document-view.c \
	src/documents/gb-document-view.h \
	src/documents/gb-document.c \
	src/documents/gb-document.h \
	src/editor/gb-editor-document.c \
	src/editor/gb-editor-document.h \
	src/editor/gb-editor-file-mark.c \
	src/editor/gb-editor-file-mark.h \
	src/editor/gb-editor-file-marks.c \
	src/editor/gb-editor-file-marks.h \
	src/editor/gb-editor-frame-private.h \
	src/editor/gb-editor-frame.c \
	src/editor/gb-editor-frame.h \
	src/editor/gb-editor-navigation-item.c \
	src/editor/gb-editor-navigation-item.h \
	src/editor/gb-editor-settings-widget.c \
	src/editor/gb-editor-settings-widget.h \
	src/editor/gb-editor-tweak-widget.c \
	src/editor/gb-editor-tweak-widget.h \
	src/editor/gb-editor-view.c \
	src/editor/gb-editor-view.h \
	src/editor/gb-editor-workspace.c \
	src/editor/gb-editor-workspace.h \
	src/editor/gb-source-change-gutter-renderer.c \
	src/editor/gb-source-change-gutter-renderer.h \
	src/editor/gb-source-change-monitor.c \
	src/editor/gb-source-change-monitor.h \
	src/editor/gb-source-formatter.c \
	src/editor/gb-source-formatter.h \
	src/editor/gb-source-highlight-menu.c \
	src/editor/gb-source-highlight-menu.h \
	src/editor/gb-source-search-highlighter.c \
	src/editor/gb-source-search-highlighter.h \
	src/editor/gb-source-view.c \
	src/editor/gb-source-view.h \
	src/emacs/gb-source-emacs.c \
	src/emacs/gb-source-emacs.h \
	src/fuzzy/fuzzy.c \
	src/fuzzy/fuzzy.h \
	src/gca/gca-diagnostics.c \
	src/gca/gca-diagnostics.h \
	src/gca/gca-service.c \
	src/gca/gca-service.h \
	src/gca/gca-structs.c \
	src/gca/gca-structs.h \
	src/gd/gd-tagged-entry.c \
	src/gd/gd-tagged-entry.h \
	src/gedit/gedit-close-button.c \
	src/gedit/gedit-close-button.h \
	src/gedit/gedit-menu-stack-switcher.c \
	src/gedit/gedit-menu-stack-switcher.h \
	src/git/gb-git-search-provider.c \
	src/git/gb-git-search-provider.h \
	src/html/gb-html-completion-provider.c \
	src/html/gb-html-completion-provider.h \
	src/html/gb-html-document.c \
	src/html/gb-html-document.h \
	src/html/gb-html-view.c \
	src/html/gb-html-view.h \
	src/keybindings/gb-keybindings.c \
	src/keybindings/gb-keybindings.h \
	src/log/gb-log.c \
	src/log/gb-log.h \
	src/nautilus/nautilus-floating-bar.c \
	src/nautilus/nautilus-floating-bar.h \
	src/navigation/gb-navigation-item.c \
	src/navigation/gb-navigation-item.h \
	src/navigation/gb-navigation-list.c \
	src/navigation/gb-navigation-list.h \
	src/preferences/gb-preferences-page-editor.c \
	src/preferences/gb-preferences-page-editor.h \
	src/preferences/gb-preferences-page-emacs.c \
	src/preferences/gb-preferences-page-emacs.h \
	src/preferences/gb-preferences-page-git.c \
	src/preferences/gb-preferences-page-git.h \
	src/preferences/gb-preferences-page-language.c \
	src/preferences/gb-preferences-page-language.h \
	src/preferences/gb-preferences-page-vim.c \
	src/preferences/gb-preferences-page-vim.h \
	src/preferences/gb-preferences-page.c \
	src/preferences/gb-preferences-page.h \
	src/preferences/gb-preferences-window.c \
	src/preferences/gb-preferences-window.h \
	src/scrolledwindow/gb-scrolled-window.c \
	src/scrolledwindow/gb-scrolled-window.h \
	src/search/gb-search-box.c \
	src/search/gb-search-box.h \
	src/search/gb-search-context.c \
	src/search/gb-search-context.h \
	src/search/gb-search-display.c \
	src/search/gb-search-display.h \
	src/search/gb-search-display-group.c \
	src/search/gb-search-display-group.h \
	src/search/gb-search-display-row.c \
	src/search/gb-search-display-row.h \
	src/search/gb-search-manager.c \
	src/search/gb-search-manager.h \
	src/search/gb-search-provider.c \
	src/search/gb-search-provider.h \
	src/search/gb-search-reducer.c \
	src/search/gb-search-reducer.h \
	src/search/gb-search-result.c \
	src/search/gb-search-result.h \
	src/search/gb-search-types.h \
	src/snippets/gb-source-snippet-chunk.c \
	src/snippets/gb-source-snippet-chunk.h \
	src/snippets/gb-source-snippet-completion-item.c \
	src/snippets/gb-source-snippet-completion-item.h \
	src/snippets/gb-source-snippet-completion-provider.c \
	src/snippets/gb-source-snippet-completion-provider.h \
	src/snippets/gb-source-snippet-context.c \
	src/snippets/gb-source-snippet-context.h \
	src/snippets/gb-source-snippet-parser.c \
	src/snippets/gb-source-snippet-parser.h \
	src/snippets/gb-source-snippet-private.h \
	src/snippets/gb-source-snippet.c \
	src/snippets/gb-source-snippet.h \
	src/snippets/gb-source-snippets-manager.c \
	src/snippets/gb-source-snippets-manager.h \
	src/snippets/gb-source-snippets.c \
	src/snippets/gb-source-snippets.h \
	src/support/gb-support.c \
	src/support/gb-support.h \
	src/theatrics/gb-box-theatric.c \
	src/theatrics/gb-box-theatric.h \
	src/tree/gb-tree-builder.c \
	src/tree/gb-tree-builder.h \
	src/tree/gb-tree-node.c \
	src/tree/gb-tree-node.h \
	src/tree/gb-tree.c \
	src/tree/gb-tree.h \
	src/trie/trie.c \
	src/trie/trie.h \
	src/util/gb-cairo.c \
	src/util/gb-cairo.h \
	src/util/gb-doc-seq.c \
	src/util/gb-doc-seq.h \
	src/util/gb-glib.h \
	src/util/gb-gtk.c \
	src/util/gb-gtk.h \
	src/util/gb-pango.c \
	src/util/gb-pango.h \
	src/util/gb-rgba.c \
	src/util/gb-rgba.h \
	src/util/gb-string.c \
	src/util/gb-string.h \
	src/util/gb-widget.c \
	src/util/gb-widget.h \
	src/util/gb-dnd.c \
	src/util/gb-dnd.h \
	src/vim/gb-source-vim.c \
	src/vim/gb-source-vim.h \
	src/workbench/gb-workbench-types.h \
	src/workbench/gb-workbench.c \
	src/workbench/gb-workbench.h \
	src/workbench/gb-workspace.c \
	src/workbench/gb-workspace.h

libgnome_builder_la_LIBADD = \
	$(BUILDER_LIBS) \
	-lm

libgnome_builder_la_CFLAGS = \
	-DPACKAGE_DATADIR="\"${datadir}\"" \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	$(BUILDER_CFLAGS) \
	$(MAINTAINER_CFLAGS) \
	-I$(top_builddir)/src/util \
	-I$(top_srcdir)/src/animation \
	-I$(top_srcdir)/src/app \
	-I$(top_srcdir)/src/auto-indent \
	-I$(top_srcdir)/src/commands \
	-I$(top_srcdir)/src/code-assistant \
	-I$(top_srcdir)/src/credits \
	-I$(top_srcdir)/src/devhelp \
	-I$(top_srcdir)/src/dialogs \
	-I$(top_srcdir)/src/documents \
	-I$(top_srcdir)/src/editor \
	-I$(top_srcdir)/src/emacs  \
	-I$(top_srcdir)/src/fuzzy \
	-I$(top_srcdir)/src/gca \
	-I$(top_srcdir)/src/gd \
	-I$(top_srcdir)/src/gedit \
	-I$(top_srcdir)/src/git \
	-I$(top_srcdir)/src/html \
	-I$(top_srcdir)/src/keybindings \
	-I$(top_srcdir)/src/log \
	-I$(top_srcdir)/src/nautilus \
	-I$(top_srcdir)/src/navigation \
	-I$(top_srcdir)/src/preferences \
	-I$(top_srcdir)/src/resources \
	-I$(top_builddir)/src/resources \
	-I$(top_srcdir)/src/scrolledwindow \
	-I$(top_srcdir)/src/search \
	-I$(top_srcdir)/src/snippets \
	-I$(top_srcdir)/src/support \
	-I$(top_srcdir)/src/tree \
	-I$(top_srcdir)/src/trie \
	-I$(top_srcdir)/src/theatrics \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src/vim \
	-I$(top_srcdir)/src/workbench

if ENABLE_TRACING
libgnome_builder_la_CFLAGS += -DGB_ENABLE_TRACE
endif

gnome_builder_SOURCES = src/main.c
gnome_builder_CFLAGS = $(libgnome_builder_la_CFLAGS)
gnome_builder_LDADD = libgnome-builder.la

# XXX: Workaround for now, need to find a more automated way to do this
# in how we build projects inside of Builder.
gnome_builder_built_sources = \
	src/resources/gb-resources.c \
	src/resources/gb-resources.h

resource_files = $(shell glib-compile-resources --sourcedir=$(top_srcdir)/src/resources --generate-dependencies $(top_srcdir)/src/resources/gnome-builder.gresource.xml)
src/resources/gb-resources.c: src/resources/gnome-builder.gresource.xml $(resource_files)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/src/resources --generate-source --c-name gb $(top_srcdir)/src/resources/gnome-builder.gresource.xml
src/resources/gb-resources.h: src/resources/gnome-builder.gresource.xml $(resource_files)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/src/resources --generate-header --c-name gb $(top_srcdir)/src/resources/gnome-builder.gresource.xml

nodist_gnome_builder_SOURCES = \
	$(gnome_builder_built_sources) \
	$(NULL)

BUILT_SOURCES += $(gnome_builder_built_sources)

EXTRA_DIST += $(resource_files)
EXTRA_DIST += src/resources/gnome-builder.gresource.xml
EXTRA_DIST += $(gnome_builder_built_sources)

DISTCLEANFILES += $(gnome_builder_built_sources)

