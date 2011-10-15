#include "nl.h"

#include <glib/gi18n.h>

#include "plugin.h"
#include "panel.h"
//#include "misc.h"

#define BORDER_SIZE 2
#define PANEL_HEIGHT_DEFAULT 26 // from panel.h

//#include "dbg.h"

typedef float NetLoadSample;

/* Private context for netload plugin. */
typedef struct {
    GdkGC * graphics_context;			/* Graphics context for drawing area */
    GdkColor foreground_color;			/* Foreground color for drawing area */
    GtkWidget * da;				/* Drawing area */
    GdkPixmap * pixmap;				/* Pixmap to be drawn on drawing area */

    guint timer;				/* Timer for periodic update */
    NetLoadSample * stats_nl;			/* Ring buffer of network utilization values */
    unsigned int ring_cursor;			/* Cursor for ring buffer */
    int pixmap_width;				/* Width of drawing area pixmap; also size of ring buffer; does not include border size */
    int pixmap_height;				/* Height of drawing area pixmap; does not include border size */
    NetLoadSample previous_stat;		/* Previous value of network load */
} NetLoadPlugin;

gboolean plugin_button_press_event(GtkWidget *widget, GdkEventButton *event, Plugin *plugin)
{
  return TRUE;
}

/* Redraw after timer callback or resize. */
static void redraw_pixmap(NetLoadPlugin * c)
{
    /* Erase pixmap. */
    gdk_draw_rectangle(c->pixmap, c->da->style->black_gc, TRUE, 0, 0, c->pixmap_width, c->pixmap_height);

    /* Recompute pixmap. */
    unsigned int i;
    unsigned int drawing_cursor = c->ring_cursor;
    for (i = 0; i < c->pixmap_width; i++)
    {
        /* Draw one bar of the netload usage graph. */
        if (c->stats_nl[drawing_cursor] != 0.0)
            gdk_draw_line(c->pixmap, c->graphics_context,
                i, c->pixmap_height,
                i, c->pixmap_height - c->stats_nl[drawing_cursor] * c->pixmap_height);

        /* Increment and wrap drawing cursor. */
        drawing_cursor += 1;
	if (drawing_cursor >= c->pixmap_width)
            drawing_cursor = 0;
    }

    /* Redraw pixmap. */
    gtk_widget_queue_draw(c->da);
}

/* Periodic timer callback. */
static gboolean netload_update(NetLoadPlugin * c)
{
  if ((c->stats_nl != NULL) && (c->pixmap != NULL)) {
      size_t num_ifaces = 0;
      NetStat *stat = get_stat(&num_ifaces);
      double summary = summary_bytes(stat, num_ifaces);
      double diff = summary - c->previous_stat;
      double total100p = (100 / 8) * 1024 * 1024; // 100Mbit/s
      c->previous_stat = summary;
      c->stats_nl[c->ring_cursor] = diff / total100p;
      c->ring_cursor += 1;
      if ( c->ring_cursor >= c->pixmap_width)
	c->ring_cursor = 0;
      
      redraw_pixmap(c);
      free_stat(stat, num_ifaces);
  }
  return TRUE;
}

/* Handler for configure_event on drawing area. */
static gboolean configure_event(GtkWidget * widget, GdkEventConfigure * event, NetLoadPlugin * c)
{
    /* Allocate pixmap and statistics buffer without border pixels. */
    int new_pixmap_width = widget->allocation.width - BORDER_SIZE * 2;
    int new_pixmap_height = widget->allocation.height - BORDER_SIZE * 2;
    if ((new_pixmap_width > 0) && (new_pixmap_height > 0))
    {
        /* If statistics buffer does not exist or it changed size, reallocate and preserve existing data. */
        if ((c->stats_nl == NULL) || (new_pixmap_width != c->pixmap_width))
        {
            NetLoadSample * new_stats_nl = g_new0(typeof(*c->stats_nl), new_pixmap_width);
            if (c->stats_nl != NULL)
            {
                if (new_pixmap_width > c->pixmap_width)
                {
                    /* New allocation is larger.
                     * Introduce new "oldest" samples of zero following the cursor. */
                    memcpy(&new_stats_nl[0],
                        &c->stats_nl[0], c->ring_cursor * sizeof(NetLoadSample));
                    memcpy(&new_stats_nl[new_pixmap_width - c->pixmap_width + c->ring_cursor],
                        &c->stats_nl[c->ring_cursor], (c->pixmap_width - c->ring_cursor) * sizeof(NetLoadSample));
                }
                else if (c->ring_cursor <= new_pixmap_width)
                {
                    /* New allocation is smaller, but still larger than the ring buffer cursor.
                     * Discard the oldest samples following the cursor. */
                    memcpy(&new_stats_nl[0],
                        &c->stats_nl[0], c->ring_cursor * sizeof(NetLoadSample));
                    memcpy(&new_stats_nl[c->ring_cursor],
                        &c->stats_nl[c->pixmap_width - new_pixmap_width + c->ring_cursor], (new_pixmap_width - c->ring_cursor) * sizeof(NetLoadSample));
                }
                else
                {
                    /* New allocation is smaller, and also smaller than the ring buffer cursor.
                     * Discard all oldest samples following the ring buffer cursor and additional samples at the beginning of the buffer. */
                    memcpy(&new_stats_nl[0],
                        &c->stats_nl[c->ring_cursor - new_pixmap_width], new_pixmap_width * sizeof(NetLoadSample));
                    c->ring_cursor = 0;
                }
                g_free(c->stats_nl);
            }
            c->stats_nl = new_stats_nl;
        }

        /* Allocate or reallocate pixmap. */
        c->pixmap_width = new_pixmap_width;
        c->pixmap_height = new_pixmap_height;
        if (c->pixmap)
            g_object_unref(c->pixmap);
        c->pixmap = gdk_pixmap_new(widget->window, c->pixmap_width, c->pixmap_height, -1);

        /* Redraw pixmap at the new size. */
        redraw_pixmap(c);
    }
    return TRUE;
}

/* Handler for expose_event on drawing area. */
static gboolean expose_event(GtkWidget * widget, GdkEventExpose * event, NetLoadPlugin * c)
{
    /* Draw the requested part of the pixmap onto the drawing area.
     * Translate it in both x and y by the border size. */
    if (c->pixmap != NULL)
    {
        gdk_draw_drawable (widget->window,
              c->da->style->black_gc,
              c->pixmap,
              event->area.x, event->area.y,
              event->area.x + BORDER_SIZE, event->area.y + BORDER_SIZE,
              event->area.width, event->area.height);
    }
    return FALSE;
}

/* Plugin constructor. */
static int netload_constructor(Plugin * p, char ** fp)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    NetLoadPlugin * c = g_new0(NetLoadPlugin, 1);
    p->priv = c;

    /* Allocate top level widget and set into Plugin widget pointer. */
    p->pwid = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(p->pwid), 1);
    GTK_WIDGET_SET_FLAGS(p->pwid, GTK_NO_WINDOW);

    /* Allocate drawing area as a child of top level widget.  Enable button press events. */
    c->da = gtk_drawing_area_new();
    gtk_widget_set_size_request(c->da, 40, PANEL_HEIGHT_DEFAULT);
    gtk_widget_add_events(c->da, GDK_BUTTON_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(p->pwid), c->da);

    /* Clone a graphics context and set "yellow" as its foreground color.
     * We will use this to draw the graph. */
    c->graphics_context = gdk_gc_new(p->panel->topgwin->window);
    gdk_color_parse("yellow",  &c->foreground_color);
    gdk_colormap_alloc_color(gdk_drawable_get_colormap(p->panel->topgwin->window), &c->foreground_color, FALSE, TRUE);
    gdk_gc_set_foreground(c->graphics_context, &c->foreground_color);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(c->da), "configure_event", G_CALLBACK(configure_event), (gpointer) c);
    g_signal_connect(G_OBJECT(c->da), "expose_event", G_CALLBACK(expose_event), (gpointer) c);
    g_signal_connect(c->da, "button-press-event", G_CALLBACK(plugin_button_press_event), p);

    /* Show the widget.  Connect a timer to refresh the statistics. */
    gtk_widget_show(c->da);
    c->timer = g_timeout_add(1500, (GSourceFunc) netload_update, (gpointer) c);
    return 1;
}

/* Plugin destructor. */
static void netload_destructor(Plugin * p)
{
    NetLoadPlugin * c = (NetLoadPlugin *) p->priv;

    /* Disconnect the timer. */
    g_source_remove(c->timer);

    /* Deallocate memory. */
    g_object_unref(c->graphics_context);
    g_object_unref(c->pixmap);
    g_free(c->stats_nl);
    g_free(c);
}

/* Plugin descriptor. */
PluginClass netload_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "netload",
    name : N_("Network Usage Monitor"),
    version: "1.0",
    description : N_("Display network usage"),

    constructor : netload_constructor,
    destructor  : netload_destructor,
    config : NULL,
    save : NULL
};
