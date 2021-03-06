/* gb-command-vim.h
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

#ifndef GB_COMMAND_VIM_H
#define GB_COMMAND_VIM_H

#include "gb-command.h"

G_BEGIN_DECLS

#define GB_TYPE_COMMAND_VIM            (gb_command_vim_get_type())
#define GB_COMMAND_VIM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_VIM, GbCommandVim))
#define GB_COMMAND_VIM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_VIM, GbCommandVim const))
#define GB_COMMAND_VIM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_COMMAND_VIM, GbCommandVimClass))
#define GB_IS_COMMAND_VIM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_COMMAND_VIM))
#define GB_IS_COMMAND_VIM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_COMMAND_VIM))
#define GB_COMMAND_VIM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_COMMAND_VIM, GbCommandVimClass))

typedef struct _GbCommandVim        GbCommandVim;
typedef struct _GbCommandVimClass   GbCommandVimClass;
typedef struct _GbCommandVimPrivate GbCommandVimPrivate;

struct _GbCommandVim
{
  GbCommand parent;

  /*< private >*/
  GbCommandVimPrivate *priv;
};

struct _GbCommandVimClass
{
  GbCommandClass parent;
};

GType gb_command_vim_get_type (void);

G_END_DECLS

#endif /* GB_COMMAND_VIM_H */
