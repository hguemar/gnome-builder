/* ide-search-engine.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-search-engine.h"
#include "ide-search-provider.h"
#include "ide-search-result.h"

typedef struct
{
  GPtrArray *providers;
} IdeSearchEnginePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSearchEngine, ide_search_engine, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

IdeSearchEngine *
ide_search_engine_new (void)
{
  return g_object_new (IDE_TYPE_SEARCH_ENGINE, NULL);
}

static void
ide_search_engine_finalize (GObject *object)
{
  IdeSearchEngine *self = (IdeSearchEngine *)object;
  IdeSearchEnginePrivate *priv = ide_search_engine_get_instance_private (self);

  G_OBJECT_CLASS (ide_search_engine_parent_class)->finalize (object);
}

static void
ide_search_engine_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeSearchEngine *self = IDE_SEARCH_ENGINE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_engine_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeSearchEngine *self = IDE_SEARCH_ENGINE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_engine_class_init (IdeSearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_search_engine_finalize;
  object_class->get_property = ide_search_engine_get_property;
  object_class->set_property = ide_search_engine_set_property;
}

static void
ide_search_engine_init (IdeSearchEngine *self)
{
}