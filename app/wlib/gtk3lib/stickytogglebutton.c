#include <gtk/gtk.h>

#include "stickytogglebutton.h"

typedef struct _StickyToggleButton StickyToggleButton;

struct _StickyToggleButton {
    GtkToggleButton parent;
    
    gboolean sticky_state;
    gboolean sticky_mode;
    guint long_press_timeout_id;
    guint long_click;
    GtkCssProvider *css_provider;
};

enum {
    PROP_0,
    PROP_STICKY,
    PROP_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE(StickyToggleButton, sticky_toggle_button, GTK_TYPE_TOGGLE_BUTTON)

// CSS for sticky border
static const char *sticky_css = 
    "button.sticky-toggle-button.sticky {\n"
    "    border: 1px solid #00ff00;\n"
    "}\n";

static void sticky_toggle_button_set_property(GObject *object, guint prop_id, 
                                            const GValue *value, GParamSpec *pspec);
static void sticky_toggle_button_get_property(GObject *object, guint prop_id, 
                                            GValue *value, GParamSpec *pspec);
static void sticky_toggle_button_dispose(GObject *object);
static gboolean sticky_toggle_button_button_press_event(GtkWidget *widget, GdkEventButton *event);
static gboolean sticky_toggle_button_button_release_event(GtkWidget *widget, GdkEventButton *event);
static gboolean long_press_timeout_callback(gpointer user_data);
static void update_sticky_style(StickyToggleButton *self);

static void sticky_toggle_button_class_init(StickyToggleButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    
    object_class->set_property = sticky_toggle_button_set_property;
    object_class->get_property = sticky_toggle_button_get_property;
    object_class->dispose = sticky_toggle_button_dispose;
    
    widget_class->button_press_event = sticky_toggle_button_button_press_event;
    widget_class->button_release_event = sticky_toggle_button_button_release_event;
    
    properties[PROP_STICKY] = g_param_spec_boolean("sticky",
                                                   "Sticky",
                                                   "Whether the button is in sticky mode",
                                                   FALSE,
                                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_MODE] = g_param_spec_boolean("mode",
        "Mode",
        "Whether sticky mode is temporarily activated by user or permanently configured by the application",
        FALSE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    
    g_object_class_install_properties(object_class, N_PROPERTIES, properties);
}

static void sticky_toggle_button_init(StickyToggleButton *self) {
    self->sticky_state = FALSE;
    self->sticky_mode = FALSE;
    self->long_press_timeout_id = 0;
    self->long_click = FALSE;
    self->css_provider = gtk_css_provider_new();
    
    // Load CSS
    gtk_css_provider_load_from_data(self->css_provider, sticky_css, -1, NULL);
    
    // Add CSS class
    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(self));
    gtk_style_context_add_class(context, "sticky-toggle-button");
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(self->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void sticky_toggle_button_set_property(GObject *object, guint prop_id,
                                            const GValue *value, GParamSpec *pspec) {
    StickyToggleButton *self = STICKY_TOGGLE_BUTTON(object);
    
    switch (prop_id) {
        case PROP_STICKY:
            sticky_toggle_button_set_sticky(self, g_value_get_boolean(value));
            break;
        case PROP_MODE:
            sticky_toggle_button_set_mode(self, g_value_get_boolean(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void sticky_toggle_button_get_property(GObject *object, guint prop_id,
                                            GValue *value, GParamSpec *pspec) {
    StickyToggleButton *self = STICKY_TOGGLE_BUTTON(object);
    
    switch (prop_id) {
        case PROP_STICKY:
            g_value_set_boolean(value, self->sticky_state);
            break;
        case PROP_MODE:
            g_value_set_boolean(value, self->sticky_mode);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void sticky_toggle_button_dispose(GObject *object) {
    StickyToggleButton *self = STICKY_TOGGLE_BUTTON(object);
    
     if (self->long_press_timeout_id) {
        g_source_remove(self->long_press_timeout_id);
        self->long_press_timeout_id = 0;
    }
    
    if (self->css_provider) {
        g_object_unref(self->css_provider);
        self->css_provider = NULL;
    }
    
    G_OBJECT_CLASS(sticky_toggle_button_parent_class)->dispose(object);
}

static gboolean sticky_toggle_button_button_press_event(GtkWidget *widget, GdkEventButton *event) {
    StickyToggleButton *self = STICKY_TOGGLE_BUTTON(widget);
    
    if (event->button == GDK_BUTTON_PRIMARY && !self->sticky_mode) {
        // Start long press timer (500ms)
        self->long_press_timeout_id = g_timeout_add(500, long_press_timeout_callback, self);
    }
    
    return GTK_WIDGET_CLASS(sticky_toggle_button_parent_class)->button_press_event(widget, event);
}

static gboolean sticky_toggle_button_button_release_event(GtkWidget *widget, GdkEventButton *event) {
    StickyToggleButton *self = STICKY_TOGGLE_BUTTON(widget);
    
    if (event->button == GDK_BUTTON_PRIMARY) {
        // Cancel long press timer
        if (self->long_press_timeout_id) {
            g_source_remove(self->long_press_timeout_id);
            self->long_press_timeout_id = 0;
        }
        
        if (self->sticky_mode) {
            // Toggle sticky on click (if not long press)
            if (!self->long_click) {
                sticky_toggle_button_set_sticky(self, !self->sticky_state);
            }
            self->long_click = FALSE;
        } else {
            self->sticky_state = FALSE;
        }
        update_sticky_style(self);
    }
    
    return GTK_WIDGET_CLASS(sticky_toggle_button_parent_class)->button_release_event(widget, event);
}

static gboolean long_press_timeout_callback(gpointer user_data) {
    StickyToggleButton *self = STICKY_TOGGLE_BUTTON(user_data);
    
    // Set sticky on long press
    sticky_toggle_button_set_sticky(self, TRUE);
    
    self->long_press_timeout_id = 0;
    self->long_click = TRUE;
    return G_SOURCE_REMOVE;
}

static void update_sticky_style(StickyToggleButton *self) {
    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(self));
    
    if (self->sticky_state) {
        gtk_style_context_add_class(context, "sticky");
    } else {
        gtk_style_context_remove_class(context, "sticky");
    }
}

// Public API functions
GtkWidget *
sticky_toggle_button_new(void) {
    return g_object_new(STICKY_TOGGLE_BUTTON_TYPE, NULL);
}

GtkWidget* 
sticky_toggle_button_new_with_mode(STICKY_TOGGLE_BUTTON_MODE mode)
{
    StickyToggleButton *newSticky = g_object_new(STICKY_TOGGLE_BUTTON_TYPE, NULL);

    sticky_toggle_button_set_mode(newSticky, mode);

    return(GTK_WIDGET(newSticky));
}

void sticky_toggle_button_set_sticky(StickyToggleButton *self, gboolean sticky_state) {
    g_return_if_fail(IS_STICKY_TOGGLE_BUTTON(self));
    
    if (self->sticky_state != sticky_state) {
        GValue activeState = G_VALUE_INIT;

        self->sticky_state = sticky_state;
        update_sticky_style(self);
        g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_STICKY]);

        // make active state of toggle button equal to sticky state
        g_value_init(&activeState, G_TYPE_BOOLEAN);
        g_value_set_boolean(&activeState, self->sticky_state);
        g_object_set_property(G_OBJECT(self), "active", &activeState);
        g_value_unset(&activeState);
    }
}

gboolean sticky_toggle_button_get_sticky(StickyToggleButton *self) {
    g_return_val_if_fail(IS_STICKY_TOGGLE_BUTTON(self), FALSE);
    return self->sticky_state;
}

void sticky_toggle_button_set_mode(StickyToggleButton* self, STICKY_TOGGLE_BUTTON_MODE mode) {
    g_return_if_fail(IS_STICKY_TOGGLE_BUTTON(self));
    g_return_if_fail(mode == STICKY_TOGGLE_BUTTON_FIXED || mode == STICKY_TOGGLE_BUTTON_TEMP);

    self->sticky_mode = mode;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_MODE]);
}

gboolean sticky_toggle_button_get_mode(StickyToggleButton* self) {
    g_return_val_if_fail(IS_STICKY_TOGGLE_BUTTON(self), FALSE);

    return self->sticky_mode;
}

