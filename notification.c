#include "notification.h"
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

// Helper function to clear album art
static void clear_album_art(NotificationState *state) {
    GtkWidget *child = gtk_widget_get_first_child(state->album_cover);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_widget_unparent(child);
        child = next;
    }
}

// Helper function to load and set album art
static void load_album_art(NotificationState *state, const gchar *art_url) {
    if (!art_url || strlen(art_url) == 0) return;

    GdkPixbuf *pixbuf = NULL;

    if (g_str_has_prefix(art_url, "file://")) {
        gchar *file_path = g_filename_from_uri(art_url, NULL, NULL);
        if (file_path && g_file_test(file_path, G_FILE_TEST_EXISTS)) {
            GError *error = NULL;
            pixbuf = gdk_pixbuf_new_from_file_at_scale(file_path, 70, 70, FALSE, &error);
            if (error) {
                g_error_free(error);
                pixbuf = NULL;
            }
        }
        g_free(file_path);
    } else if (g_str_has_prefix(art_url, "http://") || g_str_has_prefix(art_url, "https://")) {
        GFile *file = g_file_new_for_uri(art_url);
        GError *error = NULL;
        GInputStream *stream = G_INPUT_STREAM(g_file_read(file, NULL, &error));
        if (stream && !error) {
            pixbuf = gdk_pixbuf_new_from_stream_at_scale(stream, 70, 70, FALSE, NULL, &error);
            if (error) {
                g_error_free(error);
                pixbuf = NULL;
            }
            g_object_unref(stream);
        } else if (error) {
            g_error_free(error);
        }
        g_object_unref(file);
    }

    if (pixbuf) {
        GdkTexture *texture = gdk_texture_new_for_pixbuf(pixbuf);
        GtkWidget *image = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
        gtk_widget_set_size_request(image, 70, 70);

        // Clear old album art first
        clear_album_art(state);

        gtk_box_append(GTK_BOX(state->album_cover), image);
        g_object_unref(texture);
        g_object_unref(pixbuf);
    }
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

    // Always clear old album art first to prevent showing stale content
    clear_album_art(state);

    // Always update text content immediately
    const gchar *display_title = (title && strlen(title) > 0) ? title : "Unknown Track";
    const gchar *display_artist = (artist && strlen(artist) > 0) ? artist : "Unknown Artist";

    gtk_label_set_text(GTK_LABEL(state->song_label), display_title);

    gchar *artist_text = g_strdup_printf("By %s", display_artist);
    gtk_label_set_text(GTK_LABEL(state->artist_label), artist_text);
    g_free(artist_text);

    // Load new album art
    load_album_art(state, art_url);

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
