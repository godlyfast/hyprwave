#include "notification.h"
#include "art.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#define SLIDE_DISTANCE 400  // Increased from 350 to ensure full off-screen

static gboolean auto_hide_notification(gpointer user_data) {
    NotificationState *state = (NotificationState *)user_data;
    notification_hide(state);
    return G_SOURCE_REMOVE;
}

static gboolean animate_slide_in(gpointer user_data) {
    NotificationState *state = (NotificationState *)user_data;
    
    if (state->current_offset <= 0) {
        state->current_offset = 0;
        gtk_layer_set_margin(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_RIGHT, 10);
        state->animation_timer = 0;
        return G_SOURCE_REMOVE;
    }
    
    state->current_offset -= 10;  // Slower slide speed for smoother animation
    if (state->current_offset < 0) state->current_offset = 0;
    
    gtk_layer_set_margin(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_RIGHT, 10 - state->current_offset);
    
    return G_SOURCE_CONTINUE;
}

static gboolean start_notification_animation_after_load(gpointer user_data) {
    NotificationState *state = (NotificationState *)user_data;

    // Double-check we're at off-screen position
    state->current_offset = SLIDE_DISTANCE;
    gtk_layer_set_margin(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_RIGHT, 10 - SLIDE_DISTANCE);
    
    // Start slide-in animation
    state->is_showing = TRUE;
    state->animation_timer = g_timeout_add(16, animate_slide_in, state);
    
    // Set timer to auto-hide after 4 seconds
    state->hide_timer = g_timeout_add_seconds(4, auto_hide_notification, state);
    
    return G_SOURCE_REMOVE;
}

static gboolean animate_slide_out(gpointer user_data) {
    NotificationState *state = (NotificationState *)user_data;
    
    if (state->current_offset >= SLIDE_DISTANCE) {
        state->current_offset = SLIDE_DISTANCE;
        gtk_layer_set_margin(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_RIGHT, 10 - state->current_offset);
        // Keep window visible but off-screen
        state->animation_timer = 0;
        return G_SOURCE_REMOVE;
    }
    
    state->current_offset += 10;  // Slower slide speed to match slide-in
    if (state->current_offset > SLIDE_DISTANCE) state->current_offset = SLIDE_DISTANCE;
    
    gtk_layer_set_margin(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_RIGHT, 10 - state->current_offset);
    
    return G_SOURCE_CONTINUE;
}

NotificationState* notification_init(GtkApplication *app) {
    NotificationState *state = g_new0(NotificationState, 1);
    
    // Create window
    GtkWidget *window = gtk_application_window_new(app);
    state->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "HyprWave Notification");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    // Layer Shell Setup - Top Right Corner
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_namespace(GTK_WINDOW(window), "hyprwave-notification");
    
    // Anchor to top-right
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 10);
    // Start way off-screen
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 10 - SLIDE_DISTANCE);
    
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_widget_add_css_class(window, "notification-window");
    
    // Create main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(main_box, "notification-container");
    gtk_widget_set_overflow(main_box, GTK_OVERFLOW_HIDDEN);
    
    // Header label
    GtkWidget *header = gtk_label_new("Now Playing");
    gtk_widget_add_css_class(header, "notification-header");
    gtk_box_append(GTK_BOX(main_box), header);
    
    // Content box (horizontal layout like expanded section)
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(content_box, "notification-content");
    
    // Album cover
    GtkWidget *album_cover = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->album_cover = album_cover;
    gtk_widget_add_css_class(album_cover, "notification-album");
    gtk_widget_set_size_request(album_cover, 70, 70);
    gtk_widget_set_overflow(album_cover, GTK_OVERFLOW_HIDDEN);
    
    // Info panel
    GtkWidget *info_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign(info_panel, GTK_ALIGN_CENTER);
    
    // Song title
    GtkWidget *song_label = gtk_label_new("");
    state->song_label = song_label;
    gtk_widget_add_css_class(song_label, "notification-song");
    gtk_label_set_ellipsize(GTK_LABEL(song_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(song_label), 30);
    gtk_label_set_xalign(GTK_LABEL(song_label), 0.0);
    
    // Artist label
    GtkWidget *artist_label = gtk_label_new("");
    state->artist_label = artist_label;
    gtk_widget_add_css_class(artist_label, "notification-artist");
    gtk_label_set_ellipsize(GTK_LABEL(artist_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(artist_label), 30);
    gtk_label_set_xalign(GTK_LABEL(artist_label), 0.0);
    
    gtk_box_append(GTK_BOX(info_panel), song_label);
    gtk_box_append(GTK_BOX(info_panel), artist_label);
    
    gtk_box_append(GTK_BOX(content_box), album_cover);
    gtk_box_append(GTK_BOX(content_box), info_panel);
    
    gtk_box_append(GTK_BOX(main_box), content_box);
    
    gtk_window_set_child(GTK_WINDOW(window), main_box);
    
    // ALWAYS keep window visible, just off-screen
    gtk_window_present(GTK_WINDOW(window));

    state->is_showing = FALSE;
    state->hide_timer = 0;
    state->animation_timer = 0;
    state->current_offset = SLIDE_DISTANCE;
    
    return state;
}

void notification_show(NotificationState *state,
                       const gchar *title,
                       const gchar *artist,
                       const gchar *art_url,
                       const gchar *notification_type) {
    if (!state) return;

    // Cancel existing timers first
    if (state->hide_timer > 0) {
        g_source_remove(state->hide_timer);
        state->hide_timer = 0;
    }
    if (state->animation_timer > 0) {
        g_source_remove(state->animation_timer);
        state->animation_timer = 0;
    }

    // Always update text content immediately
    const gchar *display_title = (title && strlen(title) > 0) ? title : "Unknown Track";
    const gchar *display_artist = (artist && strlen(artist) > 0) ? artist : "Unknown Artist";

    gtk_label_set_text(GTK_LABEL(state->song_label), display_title);

    gchar *artist_text = g_strdup_printf("By %s", display_artist);
    gtk_label_set_text(GTK_LABEL(state->artist_label), artist_text);
    g_free(artist_text);

    // Only load album art if container is empty (art may be pre-loaded)
    // This handles ephemeral temp files from Chromium that get deleted quickly
    GtkWidget *existing_art = gtk_widget_get_first_child(state->album_cover);
    if (!existing_art) {
        load_album_art_to_container(art_url, state->album_cover, 70);
    }

    // Check if notification is currently showing or partially visible
    if (state->is_showing || state->current_offset < SLIDE_DISTANCE) {
        // Notification is visible - content already updated above
        state->is_showing = TRUE;
        // Reset hide timer
        state->hide_timer = g_timeout_add_seconds(4, auto_hide_notification, state);
    } else {
        // Notification is fully hidden (off-screen), do slide-in animation
        // Ensure we're at off-screen position
        state->current_offset = SLIDE_DISTANCE;
        gtk_layer_set_margin(GTK_WINDOW(state->window), GTK_LAYER_SHELL_EDGE_RIGHT, 10 - SLIDE_DISTANCE);

        // Wait a bit for content to be fully rendered before animating
        g_timeout_add(100, start_notification_animation_after_load, state);
    }
}

void notification_hide(NotificationState *state) {
    if (!state || !state->is_showing) return;
    
    state->is_showing = FALSE;
    
    // Cancel hide timer
    if (state->hide_timer > 0) {
        g_source_remove(state->hide_timer);
        state->hide_timer = 0;
    }
    
    // Cancel any existing animation
    if (state->animation_timer > 0) {
        g_source_remove(state->animation_timer);
    }
    
    // Start slide-out animation from current position
    state->animation_timer = g_timeout_add(16, animate_slide_out, state);  // ~60fps
}

void notification_cleanup(NotificationState *state) {
    if (!state) return;
    
    if (state->hide_timer > 0) {
        g_source_remove(state->hide_timer);
    }
    
    if (state->animation_timer > 0) {
        g_source_remove(state->animation_timer);
    }
    
    if (state->window) {
        gtk_window_destroy(GTK_WINDOW(state->window));
    }
    
    g_free(state);
}
