/* -*- mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gcal-year-view.c
 *
 * Copyright (C) 2012 - Erick Pérez Castellanos
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gcal-year-view.h"
#include "gcal-subscriber-private.h"
#include "gcal-utils.h"
#include "gcal-view.h"
#include "gcal-event-widget.h"

#include <glib/gi18n.h>

typedef struct
{

  GdkWindow      *event_window;

  /* input events flags */
  gint            clicked_cell;
  gint            start_mark_cell;
  gint            end_mark_cell;

  /* text direction factors */
  gint            k;

  /* property */
  icaltimetype   *date;
  GcalManager    *manager; /* weak reference */
} GcalYearViewPrivate;

enum
{
  PROP_0,
  PROP_DATE,  //active-date inherited property
  PROP_MANAGER  //manager inherited property
};

static void           event_opened                                (GcalEventWidget *event_widget,
                                                                   gpointer         user_data);

static void           gcal_view_interface_init                    (GcalViewIface  *iface);

static void           gcal_year_view_set_property                 (GObject        *object,
                                                                   guint           property_id,
                                                                   const GValue   *value,
                                                                   GParamSpec     *pspec);

static void           gcal_year_view_get_property                 (GObject        *object,
                                                                   guint           property_id,
                                                                   GValue         *value,
                                                                   GParamSpec     *pspec);

static void           gcal_year_view_finalize                     (GObject        *object);

static void           gcal_year_view_realize                      (GtkWidget      *widget);

static void           gcal_year_view_unrealize                    (GtkWidget      *widget);

static void           gcal_year_view_map                          (GtkWidget      *widget);

static void           gcal_year_view_unmap                        (GtkWidget      *widget);

static void           gcal_year_view_size_allocate                (GtkWidget      *widget,
                                                                   GtkAllocation  *allocation);

static gboolean       gcal_year_view_draw                         (GtkWidget      *widget,
                                                                   cairo_t        *cr);

static gboolean       gcal_year_view_button_press                 (GtkWidget      *widget,
                                                                   GdkEventButton *event);

static gboolean       gcal_year_view_motion_notify_event          (GtkWidget      *widget,
                                                                   GdkEventMotion *event);

static gboolean       gcal_year_view_button_release               (GtkWidget      *widget,
                                                                   GdkEventButton *event);

static void           gcal_year_view_direction_changed            (GtkWidget        *widget,
                                                                   GtkTextDirection  previous_direction);
static icaltimetype*  gcal_year_view_get_initial_date             (GcalView       *view);

static icaltimetype*  gcal_year_view_get_final_date               (GcalView       *view);

static void           gcal_year_view_clear_marks                  (GcalView       *view);

static gchar*         gcal_year_view_get_left_header              (GcalView       *view);

static gchar*         gcal_year_view_get_right_header             (GcalView       *view);

static GtkWidget*     gcal_year_view_get_by_uuid                  (GcalView       *view,
                                                                   const gchar    *uuid);

G_DEFINE_TYPE_WITH_CODE (GcalYearView,
                         gcal_year_view,
                         GCAL_TYPE_SUBSCRIBER,
                         G_ADD_PRIVATE (GcalYearView)
                         G_IMPLEMENT_INTERFACE (GCAL_TYPE_VIEW,
                                                gcal_view_interface_init));


static void
event_opened (GcalEventWidget *event_widget,
              gpointer         user_data)
{
  g_signal_emit_by_name (GCAL_VIEW (user_data),
                         "event-activated",
                         event_widget);
}

static void
gcal_year_view_class_init (GcalYearViewClass *klass)
{
  GtkWidgetClass *widget_class;
  GObjectClass *object_class;


  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gcal_year_view_realize;
  widget_class->unrealize = gcal_year_view_unrealize;
  widget_class->map = gcal_year_view_map;
  widget_class->unmap = gcal_year_view_unmap;
  widget_class->size_allocate = gcal_year_view_size_allocate;
  widget_class->draw = gcal_year_view_draw;
  widget_class->button_press_event = gcal_year_view_button_press;
  widget_class->motion_notify_event = gcal_year_view_motion_notify_event;
  widget_class->button_release_event = gcal_year_view_button_release;
  widget_class->direction_changed = gcal_year_view_direction_changed;

  object_class = G_OBJECT_CLASS (klass);
  object_class->set_property = gcal_year_view_set_property;
  object_class->get_property = gcal_year_view_get_property;
  object_class->finalize = gcal_year_view_finalize;

  g_object_class_override_property (object_class, PROP_DATE, "active-date");
  g_object_class_override_property (object_class, PROP_MANAGER, "manager");
}

static void
gcal_year_view_init (GcalYearView *self)
{
  GcalYearViewPrivate *priv;
  gint i;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv = gcal_year_view_get_instance_private (self);

  priv->clicked_cell = -1;

  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR)
    priv->k = 0;
  else if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    priv->k = 1;

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "calendar-view");
}

static void
gcal_view_interface_init (GcalViewIface *iface)
{
  /* New API */
  iface->get_initial_date = gcal_year_view_get_initial_date;
  iface->get_final_date = gcal_year_view_get_final_date;

  iface->clear_marks = gcal_year_view_clear_marks;

  iface->get_left_header = gcal_year_view_get_left_header;
  iface->get_right_header = gcal_year_view_get_right_header;

  iface->get_by_uuid = gcal_year_view_get_by_uuid;
}

static void
gcal_year_view_set_property (GObject       *object,
                             guint          property_id,
                             const GValue  *value,
                             GParamSpec    *pspec)
{
  GcalYearViewPrivate *priv;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      {
        time_t range_start, range_end;
        icaltimetype *date;
        icaltimezone* default_zone;

        if (priv->date != NULL)
          g_free (priv->date);

        priv->date = g_value_dup_boxed (value);

        default_zone =
          gcal_manager_get_system_timezone (priv->manager);
        date = gcal_view_get_initial_date (GCAL_VIEW (object));
        range_start = icaltime_as_timet_with_zone (*date,
                                                   default_zone);
        g_free (date);
        date = gcal_view_get_final_date (GCAL_VIEW (object));
        range_end = icaltime_as_timet_with_zone (*date,
                                                 default_zone);
        g_free (date);
        gcal_manager_set_subscriber (priv->manager,
                                     E_CAL_DATA_MODEL_SUBSCRIBER (object),
                                     range_start,
                                     range_end);
        gtk_widget_queue_draw (GTK_WIDGET (object));
        break;
      }
    case PROP_MANAGER:
      {
        priv->manager = g_value_get_pointer (value);
        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_year_view_get_property (GObject       *object,
                             guint          property_id,
                             GValue        *value,
                             GParamSpec    *pspec)
{
  GcalYearViewPrivate *priv;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (object));

  switch (property_id)
    {
    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gcal_year_view_finalize (GObject       *object)
{
  GcalYearViewPrivate *priv;
  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (object));

  if (priv->date != NULL)
    g_free (priv->date);

  /* Chain up to parent's finalize() method. */
  G_OBJECT_CLASS (gcal_year_view_parent_class)->finalize (object);
}

static void
gcal_year_view_realize (GtkWidget *widget)
{
  GcalYearViewPrivate *priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));
  gtk_widget_set_realized (widget, TRUE);

  parent_window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, parent_window);
  g_object_ref (parent_window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON1_MOTION_MASK |
                            GDK_POINTER_MOTION_HINT_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gcal_year_view_unrealize (GtkWidget *widget)
{
  GcalYearViewPrivate *priv;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));
  if (priv->event_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gcal_year_view_parent_class)->unrealize (widget);
}

static void
gcal_year_view_map (GtkWidget *widget)
{
  GcalYearViewPrivate *priv;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));
  if (priv->event_window)
    gdk_window_show (priv->event_window);

  GTK_WIDGET_CLASS (gcal_year_view_parent_class)->map (widget);
}

static void
gcal_year_view_unmap (GtkWidget *widget)
{
  GcalYearViewPrivate *priv;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));
  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gcal_year_view_parent_class)->unmap (widget);
}

static void
gcal_year_view_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GcalYearViewPrivate *priv;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));

  gtk_widget_set_allocation (widget, allocation);
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->event_window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }
}

static gboolean
gcal_year_view_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GcalYearViewPrivate *priv;

  GtkStyleContext *context;
  GtkStateFlags state;

  GtkBorder padding;
  GtkAllocation alloc;

  /* new declares */
  PangoLayout *layout;
  PangoFontDescription *font_desc;
  GdkRGBA color;

  gdouble cell_width, cell_height, sw;
  gint i, j, pos_x, pos_y;
  int font_width, font_height;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));
  gtk_widget_get_allocation (widget, &alloc);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  layout = gtk_widget_create_pango_layout (widget, NULL);

  /* calculations */
  cell_width = alloc.width / 6.0;
  cell_height = alloc.height / 2.0;
  sw = 1 - 2 * priv->k;

  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_get (context, state, "font", &font_desc, NULL);

  pango_layout_set_font_description (layout, font_desc);
  for (i = 0; i < 12; i++)
    {
      gint column = i % 6;
      gint row = i / 6;

      j = 6 * ((i + 6 * priv->k) / 6) + sw * (i % 6) + (1 - priv->k);
      g_debug ("cell: %d numbered: %d: %s", i, j, gcal_get_month_name (j - 1));

      if (priv->date->month == j)
        {
          PangoLayout *clayout;
          PangoFontDescription *cfont_desc;

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, "current");

          clayout = gtk_widget_create_pango_layout (widget, gcal_get_month_name (j - 1));
          gtk_style_context_get (context, state, "font", &cfont_desc, NULL);
          pango_layout_set_font_description (clayout, cfont_desc);
          pango_layout_get_pixel_size (clayout, &font_width, &font_height);

          /* FIXME: hardcoded padding of the number background */
          gtk_render_background (context, cr,
                                 cell_width * (column + 1 - priv->k) - sw * padding.right + (priv->k - 1) * font_width - 2.0,
                                 cell_height * (row + 1) - font_height - padding.bottom,
                                 font_width + 4, font_height + 2);
          gtk_render_layout (context, cr,
                             cell_width * (column + 1 - priv->k) - sw * padding.right + (priv->k - 1) * font_width,
                             cell_height * (row + 1) - font_height - padding.bottom,
                             clayout);

          gtk_style_context_restore (context);
          pango_font_description_free (cfont_desc);
          g_object_unref (clayout);
        }
      else
        {
          pango_layout_set_text (layout, gcal_get_month_name (j - 1), -1);
          pango_layout_get_pixel_size (layout, &font_width, &font_height);

          gtk_render_layout (context, cr,
                             cell_width * (column + 1 - priv->k) - sw * padding.right + (priv->k - 1) * font_width,
                             cell_height * (row + 1) - font_height - padding.bottom,
                             layout);

        }
    }
  pango_font_description_free (font_desc);

  /* FIXME: missing selected months lines */

  /* drawing grid skel */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "lines");

  gtk_style_context_get_color (context, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_set_line_width (cr, 0.2);

  /* vertical lines */
  for (i = 0; i < 5; i++)
    {
      pos_x = cell_width * (i + 1);
      cairo_move_to (cr, pos_x + 0.4, 0);
      cairo_rel_line_to (cr, 0, alloc.height);
    }

  for (i = 0; i < 2; i++)
    {
      pos_y = (alloc.height / 2) * i;
      cairo_move_to (cr, 0, pos_y + 0.4);
      cairo_rel_line_to (cr, alloc.width, 0);
    }
  cairo_stroke (cr);
  gtk_style_context_restore (context);

  if (GTK_WIDGET_CLASS (gcal_year_view_parent_class)->draw != NULL)
    GTK_WIDGET_CLASS (gcal_year_view_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gcal_year_view_button_press (GtkWidget      *widget,
                             GdkEventButton *event)
{
  GcalYearViewPrivate *priv;

  gdouble x, y;
  gint width, height;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));

  x = event->x;
  y = event->y;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  priv->clicked_cell = 6 * ( (gint ) ( y  / (height / 2) )) + ((gint) ( x / (width / 6) ));
  priv->start_mark_cell = priv->clicked_cell;

  g_debug ("button pressed: cell %d", priv->start_mark_cell);
  return TRUE;
}

static gboolean
gcal_year_view_motion_notify_event (GtkWidget      *widget,
                                    GdkEventMotion *event)
{
  GcalYearViewPrivate *priv;

  gint width, height;
  gint y;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));

  if (priv->clicked_cell == -1)
    return FALSE;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  y = event->y;

  if (y < 0)
    return FALSE;

  priv->end_mark_cell = 6 * ( (gint ) ( y  / (height / 2) )) + ((gint) ( event->x / (width / 6) ));

  gtk_widget_queue_draw (widget);

  return TRUE;
}

static gboolean
gcal_year_view_button_release (GtkWidget      *widget,
                               GdkEventButton *event)
{
  GcalYearViewPrivate *priv;

  gdouble x, y;
  gint width, height;

  icaltimetype *start_date;
  icaltimetype *end_date;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));

  if (priv->clicked_cell == -1)
    return FALSE;

  x = event->x;
  y = event->y;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  priv->end_mark_cell = 6 * ( (gint ) ( y  / (height / 2) )) + ((gint) ( x / (width / 6) ));

  x = (width / 6) * (( priv->end_mark_cell % 6) + 0.5);
  y = (height / 2) * (( priv->end_mark_cell / 6) + 0.5);

  start_date = gcal_dup_icaltime (priv->date);
  start_date->day = 1;
  start_date->month = priv->start_mark_cell + 1;
  start_date->is_date = 1;

  end_date = gcal_dup_icaltime (priv->date);
  end_date->day = icaltime_days_in_month (priv->end_mark_cell + 1,
                                          end_date->year);
  end_date->month = priv->end_mark_cell + 1;
  end_date->is_date = 1;

  if (priv->start_mark_cell > priv->end_mark_cell)
    {
      start_date->month = priv->end_mark_cell + 1;
      end_date->day = icaltime_days_in_month (priv->start_mark_cell + 1,
                                              end_date->year);
      end_date->month = priv->start_mark_cell + 1;
    }

  g_signal_emit_by_name (GCAL_VIEW (widget),
                         "create-event",
                         start_date, end_date,
                         x, y);
  g_free (start_date);
  g_free (end_date);

  g_debug ("button released: cell %d", priv->end_mark_cell);

  priv->clicked_cell = -1;
  return TRUE;
}

static void
gcal_year_view_direction_changed (GtkWidget        *widget,
                                  GtkTextDirection  previous_direction)
{
  GcalYearViewPrivate *priv;
  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (widget));

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    priv->k = 0;
  else if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    priv->k = 1;
}

/* GcalView Interface API */
/**
 * gcal_year_view_get_initial_date:
 *
 * Since: 0.1
 * Return value: the January first of the current year.
 * Returns: (transfer full): Release with g_free()
 **/
static icaltimetype*
gcal_year_view_get_initial_date (GcalView *view)
{
  GcalYearViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_YEAR_VIEW (view), NULL);
  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (view));

  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = 1;
  new_date->month = 1;
  new_date->is_date = 0;
  new_date->hour = 0;
  new_date->minute = 0;
  new_date->second = 0;

  return new_date;
}

/**
 * gcal_year_view_get_final_date:
 *
 * Since: 0.1
 * Return value: the last day of the month
 * Returns: (transfer full): Release with g_free()
 **/
static icaltimetype*
gcal_year_view_get_final_date (GcalView *view)
{
  GcalYearViewPrivate *priv;
  icaltimetype *new_date;

  g_return_val_if_fail (GCAL_IS_YEAR_VIEW (view), NULL);
  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (view));

  new_date = gcal_dup_icaltime (priv->date);
  new_date->day = 31;
  new_date->month =  12;
  new_date->is_date = 0;
  new_date->hour = 23;
  new_date->minute = 59;
  new_date->second = 0;

  return new_date;
}

static void
gcal_year_view_clear_marks (GcalView *view)
{
  GcalYearViewPrivate *priv;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (view));

  priv->start_mark_cell = -1;
  priv->end_mark_cell = -1;
  gtk_widget_queue_draw (GTK_WIDGET (view));
}

static gchar*
gcal_year_view_get_left_header (GcalView *view)
{
  GcalYearViewPrivate *priv;

  priv = gcal_year_view_get_instance_private (GCAL_YEAR_VIEW (view));

  return g_strdup_printf ("%d", priv->date->year);
}

static gchar*
gcal_year_view_get_right_header (GcalView *view)
{
  return g_strdup ("");
}

static GtkWidget*
gcal_year_view_get_by_uuid (GcalView    *view,
                            const gchar *uuid)
{
  return NULL;
}

/* Public API */
/**
 * gcal_year_view_new:
 * @manager: App singleton #GcalManager instance
 *
 * Create the year view
 *
 * Returns: (transfer full):
 **/
GtkWidget*
gcal_year_view_new (GcalManager *manager)
{
  return g_object_new (GCAL_TYPE_YEAR_VIEW, "manager", manager, NULL);
}
