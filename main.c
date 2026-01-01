#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *revealer;
    GtkWidget *play_icon;
    GtkWidget *expand_icon;
    GtkWidget *album_cover;
    GtkWidget *source_label;
    GtkWidget *track_title;
    GtkWidget *artist_label;
    GtkWidget *time_remaining;
    GtkWidget *progress_bar;
    gboolean is_playing;
    gboolean is_expanded;
    GDBusProxy *mpris_proxy;
    gchar *current_player;
    guint update_timer;
} AppState;

static void update_position(AppState *state);
static void update_metadata(AppState *state);

static gboolean update_position_tick(gpointer user_data) {
    AppState *state = (AppState *)user_data;
    update_position(state);
    return G_SOURCE_CONTINUE;
}

static void update_position(AppState *state) {
    if (!state->mpris_proxy) return;
    
    GError *error = NULL;
    GVariant *position_var = g_dbus_proxy_call_sync(
        state->mpris_proxy,
        "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "Position"),
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &error
    );
    
    if (error) {
        g_error_free(error);
        return;
    }
    
    GVariant *value;
    g_variant_get(position_var, "(v)", &value);
    gint64 position = g_variant_get_int64(value);
    g_variant_unref(value);
    g_variant_unref(position_var);
    
    // Get metadata for track length
    GVariant *metadata_var = g_dbus_proxy_get_cached_property(state->mpris_proxy, "Metadata");
    if (metadata_var) {
        GVariantDict dict;
        g_variant_dict_init(&dict, metadata_var);
        
        GVariant *length_var = g_variant_dict_lookup_value(&dict, "mpris:length", NULL);
        if (length_var) {
            gint64 length = g_variant_get_int64(length_var);
            gint64 remaining = (length - position) / 1000000; // Convert to seconds
            
            int mins = remaining / 60;
            int secs = remaining % 60;
            char time_str[32];
            snprintf(time_str, sizeof(time_str), "-%d:%02d", mins, secs);
            gtk_label_set_text(GTK_LABEL(state->time_remaining), time_str);
            
            // Update progress bar
            if (length > 0) {
                double fraction = (double)position / (double)length;
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->progress_bar), fraction);
            }
            
            g_variant_unref(length_var);
        }
        g_variant_dict_clear(&dict);
        g_variant_unref(metadata_var);
    }
}

static void update_metadata(AppState *state) {
    if (!state->mpris_proxy) return;
    
    GVariant *metadata = g_dbus_proxy_get_cached_property(state->mpris_proxy, "Metadata");
    if (!metadata) {
        g_print("No metadata available\n");
        return;
    }
    
    // Iterate through the metadata dictionary
    GVariantIter iter;
    GVariant *value;
    gchar *key;
    
    const gchar *title = NULL;
    const gchar *artist = NULL;
    const gchar *album = NULL;
    const gchar *art_url = NULL;
    
    g_variant_iter_init(&iter, metadata);
    while (g_variant_iter_loop(&iter, "{sv}", &key, &value)) {
        g_print("Metadata key: %s, type: %s\n", key, g_variant_get_type_string(value));
        
        if (g_strcmp0(key, "xesam:title") == 0) {
            title = g_variant_get_string(value, NULL);
            g_print("  Title value: %s\n", title);
        }
        else if (g_strcmp0(key, "xesam:album") == 0) {
            album = g_variant_get_string(value, NULL);
            g_print("  Album value: %s\n", album);
        }
        else if (g_strcmp0(key, "xesam:artist") == 0) {
            // Artist is usually an array of strings
            if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING_ARRAY)) {
                gsize length;
                const gchar **artists = g_variant_get_strv(value, &length);
                g_print("  Artist array length: %zu\n", length);
                if (length > 0) {
                    artist = artists[0];
                    g_print("  Artist value: %s\n", artist);
                }
                // Don't free artists here - g_variant_iter_loop handles it
            }
        }
        else if (g_strcmp0(key, "mpris:artUrl") == 0) {
            art_url = g_variant_get_string(value, NULL);
            g_print("  Art URL value: %s\n", art_url);
        }
    }
    
    // Apply the values
    if (title && strlen(title) > 0) {
        gtk_label_set_text(GTK_LABEL(state->track_title), title);
        g_print("Set title: %s\n", title);
    } else {
        gtk_label_set_text(GTK_LABEL(state->track_title), "No Track Playing");
    }
    
    if (artist && strlen(artist) > 0) {
        gtk_label_set_text(GTK_LABEL(state->artist_label), artist);
        g_print("Set artist: %s\n", artist);
    } else {
        gtk_label_set_text(GTK_LABEL(state->artist_label), "Unknown Artist");
    }
    
    if (artist && strlen(artist) > 0) {
        gtk_label_set_text(GTK_LABEL(state->artist_label), artist);
        g_print("Set artist from metadata: %s\n", artist);
    } else if (gtk_label_get_text(GTK_LABEL(state->artist_label)) == NULL || 
               g_strcmp0(gtk_label_get_text(GTK_LABEL(state->artist_label)), "Unknown Artist") == 0) {
        // Only set "Unknown Artist" if we haven't already parsed it from title
        const gchar *current = gtk_label_get_text(GTK_LABEL(state->artist_label));
        if (g_strcmp0(current, "Unknown Artist") == 0) {
            gtk_label_set_text(GTK_LABEL(state->artist_label), "Unknown Artist");
        }
    }
    
    // Handle album art
    if (art_url && strlen(art_url) > 0) {
        g_print("Art URL: %s\n", art_url);
        
        if (g_str_has_prefix(art_url, "file://")) {
            gchar *file_path = g_filename_from_uri(art_url, NULL, NULL);
            
            if (file_path && g_file_test(file_path, G_FILE_TEST_EXISTS)) {
                g_print("Loading album art from: %s\n", file_path);
                
                GError *error = NULL;
                GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(file_path, 120, 120, FALSE, &error);
                
                if (pixbuf) {
                    GdkTexture *texture = gdk_texture_new_for_pixbuf(pixbuf);
                    GtkWidget *image = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
                    gtk_widget_set_size_request(image, 120, 120);
                    
                    // Clear old content
                    GtkWidget *child = gtk_widget_get_first_child(state->album_cover);
                    while (child) {
                        GtkWidget *next = gtk_widget_get_next_sibling(child);
                        gtk_widget_unparent(child);
                        child = next;
                    }
                    
                    gtk_box_append(GTK_BOX(state->album_cover), image);
                    g_object_unref(texture);
                    g_object_unref(pixbuf);
                    
                    g_print("Album art loaded successfully\n");
                } else if (error) {
                    g_printerr("Failed to load album art: %s\n", error->message);
                    g_error_free(error);
                }
            }
            
            g_free(file_path);
        } else if (g_str_has_prefix(art_url, "http://") || g_str_has_prefix(art_url, "https://")) {
            // Download album art from HTTP URL
            g_print("Downloading album art from HTTP: %s\n", art_url);
            
            GFile *file = g_file_new_for_uri(art_url);
            GError *error = NULL;
            GInputStream *stream = G_INPUT_STREAM(g_file_read(file, NULL, &error));
            
            if (stream && !error) {
                GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream_at_scale(stream, 120, 120, FALSE, NULL, &error);
                
                if (pixbuf && !error) {
                    GdkTexture *texture = gdk_texture_new_for_pixbuf(pixbuf);
                    GtkWidget *image = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
                    gtk_widget_set_size_request(image, 120, 120);
                    
                    // Clear old content
                    GtkWidget *child = gtk_widget_get_first_child(state->album_cover);
                    while (child) {
                        GtkWidget *next = gtk_widget_get_next_sibling(child);
                        gtk_widget_unparent(child);
                        child = next;
                    }
                    
                    gtk_box_append(GTK_BOX(state->album_cover), image);
                    g_object_unref(texture);
                    g_object_unref(pixbuf);
                    
                    g_print("HTTP album art loaded successfully\n");
                } else if (error) {
                    g_printerr("Failed to load HTTP album art: %s\n", error->message);
                    g_error_free(error);
                }
                
                g_object_unref(stream);
            } else if (error) {
                g_printerr("Failed to open HTTP stream: %s\n", error->message);
                g_error_free(error);
            }
            
            g_object_unref(file);
        }
    } else {
        g_print("No album art URL in metadata\n");
    }
    
    // Set source (player name)
    if (state->current_player) {
        // Try to get Identity property first
        GError *error = NULL;
        GDBusProxy *player_proxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SESSION,
            G_DBUS_PROXY_FLAGS_NONE,
            NULL,
            state->current_player,
            "/org/mpris/MediaPlayer2",
            "org.mpris.MediaPlayer2",
            NULL,
            &error
        );
        
        if (player_proxy && !error) {
            GVariant *identity = g_dbus_proxy_get_cached_property(player_proxy, "Identity");
            if (identity) {
                const gchar *player_name = g_variant_get_string(identity, NULL);
                gtk_label_set_text(GTK_LABEL(state->source_label), player_name);
                g_print("Source: %s\n", player_name);
                g_variant_unref(identity);
            }
            g_object_unref(player_proxy);
        } else if (error) {
            g_error_free(error);
        }
    }
    
    g_variant_unref(metadata);
    update_position(state);
}

static void on_properties_changed(GDBusProxy *proxy, GVariant *changed_properties,
                                   GStrv invalidated_properties, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    update_metadata(state);
    
    // Check playback status
    GVariant *status_var = g_dbus_proxy_get_cached_property(proxy, "PlaybackStatus");
    if (status_var) {
        const gchar *status = g_variant_get_string(status_var, NULL);
        state->is_playing = g_strcmp0(status, "Playing") == 0;
        
        if (state->is_playing) {
            gtk_image_set_from_file(GTK_IMAGE(state->play_icon), "icons/pause.svg");
        } else {
            gtk_image_set_from_file(GTK_IMAGE(state->play_icon), "icons/play.svg");
        }
        g_variant_unref(status_var);
    }
}

static void connect_to_player(AppState *state, const gchar *bus_name) {
    if (state->mpris_proxy) {
        g_object_unref(state->mpris_proxy);
    }
    
    g_free(state->current_player);
    state->current_player = g_strdup(bus_name);
    
    GError *error = NULL;
    state->mpris_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        bus_name,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        NULL,
        &error
    );
    
    if (error) {
        g_printerr("Failed to connect to player: %s\n", error->message);
        g_error_free(error);
        return;
    }
    
    g_signal_connect(state->mpris_proxy, "g-properties-changed",
                     G_CALLBACK(on_properties_changed), state);
    
    update_metadata(state);
    g_print("Connected to player: %s\n", bus_name);
}

static void find_active_player(AppState *state) {
    GError *error = NULL;
    GDBusProxy *dbus_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        NULL,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return;
    }
    
    GVariant *result = g_dbus_proxy_call_sync(
        dbus_proxy,
        "ListNames",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &error
    );
    
    if (error) {
        g_error_free(error);
        g_object_unref(dbus_proxy);
        return;
    }
    
    GVariantIter *iter;
    g_variant_get(result, "(as)", &iter);
    
    const gchar *name;
    while (g_variant_iter_loop(iter, "&s", &name)) {
        if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.")) {
            connect_to_player(state, name);
            break;
        }
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    g_object_unref(dbus_proxy);
}

static void on_play_clicked(GtkButton *button, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    
    if (!state->mpris_proxy) {
        g_print("No media player connected\n");
        return;
    }
    
    g_dbus_proxy_call(
        state->mpris_proxy,
        "PlayPause",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, NULL, NULL
    );
}

static void on_prev_clicked(GtkButton *button, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    
    if (!state->mpris_proxy) return;
    
    g_dbus_proxy_call(
        state->mpris_proxy,
        "Previous",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, NULL, NULL
    );
}

static void on_next_clicked(GtkButton *button, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    
    if (!state->mpris_proxy) return;
    
    g_dbus_proxy_call(
        state->mpris_proxy,
        "Next",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, NULL, NULL
    );
}

static void on_revealer_transition_done(GObject *revealer, GParamSpec *pspec, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    
    if (!state->is_expanded) {
        gtk_window_set_default_size(GTK_WINDOW(state->window), 1, 1);
        gtk_widget_queue_resize(state->window);
    }
}

static void on_expand_clicked(GtkButton *button, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    state->is_expanded = !state->is_expanded;
    
    if (state->is_expanded) {
        gtk_image_set_from_file(GTK_IMAGE(state->expand_icon), "icons/arrow-right.svg");
    } else {
        gtk_image_set_from_file(GTK_IMAGE(state->expand_icon), "icons/arrow-left.svg");
    }
    
    gtk_revealer_set_reveal_child(GTK_REVEALER(state->revealer), state->is_expanded);
    
    g_print("Expand clicked - expanded: %s\n", state->is_expanded ? "true" : "false");
}

static void load_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppState *state = g_new0(AppState, 1);
    state->is_playing = FALSE;
    state->is_expanded = FALSE;
    state->mpris_proxy = NULL;
    state->current_player = NULL;
    
    GtkWidget *window = gtk_application_window_new(app);
    state->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "HyprWave");
    
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_exclusive_zone(GTK_WINDOW(window), 0);
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_namespace(GTK_WINDOW(window), "hyprwave");
    
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
    
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 10);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    
    gtk_widget_add_css_class(window, "hyprwave-window");
    
    GtkWidget *main_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(main_container, "main-container");
    gtk_widget_set_hexpand(main_container, FALSE);
    gtk_widget_set_vexpand(main_container, FALSE);
    
    GtkWidget *control_bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(control_bar, "control-container");
    gtk_widget_set_halign(control_bar, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(control_bar, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(control_bar, FALSE);
    gtk_widget_set_vexpand(control_bar, FALSE);
    gtk_widget_set_size_request(control_bar, 70, 240);
    
    GtkWidget *expanded_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(expanded_section, "expanded-section");
    gtk_widget_set_halign(expanded_section, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(expanded_section, GTK_ALIGN_CENTER);
    
    GtkWidget *album_cover = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->album_cover = album_cover;
    gtk_widget_add_css_class(album_cover, "album-cover");
    gtk_widget_set_size_request(album_cover, 120, 120);
    
    GtkWidget *source_label = gtk_label_new("No Source");
    state->source_label = source_label;
    gtk_widget_add_css_class(source_label, "source-label");
    
    GtkWidget *track_title = gtk_label_new("No Track Playing");
    state->track_title = track_title;
    gtk_widget_add_css_class(track_title, "track-title");
    gtk_label_set_ellipsize(GTK_LABEL(track_title), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(track_title), 20);
    
    GtkWidget *artist_label = gtk_label_new("Unknown Artist");
    state->artist_label = artist_label;
    gtk_widget_add_css_class(artist_label, "artist-label");
    gtk_label_set_ellipsize(GTK_LABEL(artist_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(artist_label), 20);
    
    GtkWidget *progress_bar = gtk_progress_bar_new();
    state->progress_bar = progress_bar;
    gtk_widget_add_css_class(progress_bar, "track-progress");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    gtk_widget_set_size_request(progress_bar, 140, 4);
    
    GtkWidget *time_remaining = gtk_label_new("--:--");
    state->time_remaining = time_remaining;
    gtk_widget_add_css_class(time_remaining, "time-remaining");
    
    gtk_box_append(GTK_BOX(expanded_section), album_cover);
    gtk_box_append(GTK_BOX(expanded_section), source_label);
    gtk_box_append(GTK_BOX(expanded_section), track_title);
    gtk_box_append(GTK_BOX(expanded_section), artist_label);
    gtk_box_append(GTK_BOX(expanded_section), progress_bar);
    gtk_box_append(GTK_BOX(expanded_section), time_remaining);
    
    GtkWidget *revealer = gtk_revealer_new();
    state->revealer = revealer;
    gtk_revealer_set_transition_type(GTK_REVEALER(revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 300);
    gtk_revealer_set_child(GTK_REVEALER(revealer), expanded_section);
    gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
    g_signal_connect(revealer, "notify::child-revealed", G_CALLBACK(on_revealer_transition_done), state);
    
    GtkWidget *prev_btn = gtk_button_new();
    gtk_widget_set_size_request(prev_btn, 44, 44);
    gtk_widget_set_hexpand(prev_btn, FALSE);
    gtk_widget_set_vexpand(prev_btn, FALSE);
    GtkWidget *prev_icon = gtk_image_new_from_file("icons/previous.svg");
    gtk_image_set_pixel_size(GTK_IMAGE(prev_icon), 20);
    gtk_button_set_child(GTK_BUTTON(prev_btn), prev_icon);
    gtk_widget_set_tooltip_text(prev_btn, "Previous");
    gtk_widget_add_css_class(prev_btn, "control-button");
    gtk_widget_add_css_class(prev_btn, "prev-button");
    g_signal_connect(prev_btn, "clicked", G_CALLBACK(on_prev_clicked), state);
    
    GtkWidget *play_btn = gtk_button_new();
    gtk_widget_set_size_request(play_btn, 44, 44);
    gtk_widget_set_hexpand(play_btn, FALSE);
    gtk_widget_set_vexpand(play_btn, FALSE);
    GtkWidget *play_icon = gtk_image_new_from_file("icons/play.svg");
    state->play_icon = play_icon;
    gtk_image_set_pixel_size(GTK_IMAGE(play_icon), 20);
    gtk_button_set_child(GTK_BUTTON(play_btn), play_icon);
    gtk_widget_set_tooltip_text(play_btn, "Play/Pause");
    gtk_widget_add_css_class(play_btn, "control-button");
    gtk_widget_add_css_class(play_btn, "play-button");
    g_signal_connect(play_btn, "clicked", G_CALLBACK(on_play_clicked), state);
    
    GtkWidget *next_btn = gtk_button_new();
    gtk_widget_set_size_request(next_btn, 44, 44);
    gtk_widget_set_hexpand(next_btn, FALSE);
    gtk_widget_set_vexpand(next_btn, FALSE);
    GtkWidget *next_icon = gtk_image_new_from_file("icons/next.svg");
    gtk_image_set_pixel_size(GTK_IMAGE(next_icon), 20);
    gtk_button_set_child(GTK_BUTTON(next_btn), next_icon);
    gtk_widget_set_tooltip_text(next_btn, "Next");
    gtk_widget_add_css_class(next_btn, "control-button");
    gtk_widget_add_css_class(next_btn, "next-button");
    g_signal_connect(next_btn, "clicked", G_CALLBACK(on_next_clicked), state);
    
    GtkWidget *expand_btn = gtk_button_new();
    gtk_widget_set_size_request(expand_btn, 44, 44);
    gtk_widget_set_hexpand(expand_btn, FALSE);
    gtk_widget_set_vexpand(expand_btn, FALSE);
    GtkWidget *expand_icon = gtk_image_new_from_file("icons/arrow-left.svg");
    state->expand_icon = expand_icon;
    gtk_image_set_pixel_size(GTK_IMAGE(expand_icon), 20);
    gtk_button_set_child(GTK_BUTTON(expand_btn), expand_icon);
    gtk_widget_set_tooltip_text(expand_btn, "Expand");
    gtk_widget_add_css_class(expand_btn, "control-button");
    gtk_widget_add_css_class(expand_btn, "expand-button");
    g_signal_connect(expand_btn, "clicked", G_CALLBACK(on_expand_clicked), state);
    
    gtk_box_append(GTK_BOX(control_bar), prev_btn);
    gtk_box_append(GTK_BOX(control_bar), play_btn);
    gtk_box_append(GTK_BOX(control_bar), next_btn);
    gtk_box_append(GTK_BOX(control_bar), expand_btn);
    
    gtk_box_append(GTK_BOX(main_container), control_bar);
    gtk_box_append(GTK_BOX(main_container), revealer);
    
    gtk_window_set_child(GTK_WINDOW(window), main_container);
    gtk_window_present(GTK_WINDOW(window));
    
    // Find and connect to active media player
    find_active_player(state);
    
    // Update position every second
    state->update_timer = g_timeout_add_seconds(1, update_position_tick, state);
    
    g_print("HyprWave window created\n");
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.hyprwave.app", G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(app, "startup", G_CALLBACK(load_css), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
