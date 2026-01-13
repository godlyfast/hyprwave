#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "layout.h"
#include "paths.h"
#include "notification.h"
#include "art.h"

#define HYPRWAVE_VERSION "0.5.3"

typedef struct {
    GtkWidget *window;
    GtkWidget *window_revealer;
    GtkWidget *revealer;
    GtkWidget *play_button;      // Play button (for animation class)
    GtkWidget *play_icon;
    GtkWidget *expand_icon;
    GtkWidget *album_cover;
    GtkWidget *source_label;
    GtkWidget *format_label;     // Bitrate/format display
    GtkWidget *track_title;
    GtkWidget *artist_label;
    GtkWidget *time_remaining;
    GtkWidget *progress_bar;     // Now a GtkScale for seek support
    GtkWidget *player_label;     // Player selector display
    gboolean is_playing;
    gboolean is_expanded;
    gboolean is_visible;
    gboolean is_seeking;         // True while user is dragging seek slider
    gboolean can_seek;           // True if current player supports seeking
    GDBusProxy *mpris_proxy;
    gchar *current_player;       // D-Bus name of current player
    guint update_timer;
    LayoutConfig *layout;
    NotificationState *notification;
    gchar *last_track_id;
    gchar *last_title;           // Fallback for track change detection
    gchar *current_track_id;     // For seek operations
    gint64 current_length;       // Track length in microseconds
    // Player switching
    gchar **players;             // Array of MPRIS player D-Bus names
    gint player_count;
    gint current_player_index;
    gchar *player_display_name;  // Human-readable name from Identity
    // Notification debounce
    guint notification_timer;
    gchar *pending_title;
    gchar *pending_artist;
    gchar *pending_art_url;
    // Signal handler ID for blocking during programmatic updates
    gulong seek_handler_id;
    // Suppress notification during player switch
    gboolean suppress_notification;
} AppState;

static void update_position(AppState *state);
static void update_metadata(AppState *state);
static void update_playback_status(AppState *state);
static gint64 get_variant_as_int64(GVariant *value);
static void on_expand_clicked(GtkButton *button, gpointer user_data);
static void on_properties_changed(GDBusProxy *proxy, GVariant *changed_properties,
                                  GStrv invalidated_properties, gpointer user_data);
static void load_available_players(AppState *state);
static void switch_to_player(AppState *state, const gchar *bus_name);

// Global state for signal handlers
static AppState *global_state = NULL;

// ========================================
// PLAYER SWITCHING FUNCTIONS
// ========================================

// Check if a D-Bus name should be excluded from player list
static gboolean is_excluded_player(const gchar *name) {
    // Always exclude playerctld (proxy that duplicates other players)
    if (g_strstr_len(name, -1, "playerctld") != NULL)
        return TRUE;

    // Allow known Electron music apps (check Identity for these)
    // They register as chromium but have full MPRIS support
    // We'll check their Identity property separately

    // Exclude browsers (limited MPRIS support)
    if (g_strstr_len(name, -1, "firefox") != NULL ||
        g_strstr_len(name, -1, "brave") != NULL)
        return TRUE;

    // For chromium instances, we need to check Identity to distinguish
    // browser tabs from Electron apps like tidal-hifi
    // This is handled in is_allowed_chromium_player()
    return FALSE;
}

// Check if a chromium-based player is actually a music app (not browser tab)
static gboolean is_allowed_chromium_player(const gchar *bus_name) {
    if (g_strstr_len(bus_name, -1, "chromium") == NULL)
        return TRUE;  // Not chromium, allow

    // Query Identity to check if it's a known music app
    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL,
        bus_name, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2",
        NULL, NULL
    );

    if (!proxy) return FALSE;

    GVariant *identity = g_dbus_proxy_get_cached_property(proxy, "Identity");
    gboolean allowed = FALSE;

    if (identity) {
        const gchar *id = g_variant_get_string(identity, NULL);
        // Allow known music apps that use Electron/Chromium
        if (g_strstr_len(id, -1, "TIDAL") != NULL ||
            g_strstr_len(id, -1, "tidal") != NULL ||
            g_strstr_len(id, -1, "Cider") != NULL ||      // Apple Music client
            g_strstr_len(id, -1, "YouTube Music") != NULL)
            allowed = TRUE;
        g_variant_unref(identity);
    }

    g_object_unref(proxy);
    return allowed;
}

// Load all available MPRIS players from D-Bus
static void load_available_players(AppState *state) {
    // Free previous list
    if (state->players) {
        g_strfreev(state->players);
        state->players = NULL;
    }
    state->player_count = 0;

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
    GPtrArray *player_arr = g_ptr_array_new();

    // Find all MPRIS players (excluding browsers and playerctld)
    while (g_variant_iter_loop(iter, "&s", &name)) {
        if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.") &&
            !is_excluded_player(name) &&
            is_allowed_chromium_player(name)) {
            g_ptr_array_add(player_arr, g_strdup(name));
        }
    }

    g_variant_iter_free(iter);
    g_variant_unref(result);
    g_object_unref(dbus_proxy);

    g_ptr_array_add(player_arr, NULL);
    state->players = (gchar **)g_ptr_array_free(player_arr, FALSE);

    // Count players
    state->player_count = 0;
    for (gchar **p = state->players; *p; p++) state->player_count++;

    // Find current player index
    state->current_player_index = -1;
    if (state->current_player && state->players) {
        for (int i = 0; state->players[i]; i++) {
            if (g_strcmp0(state->players[i], state->current_player) == 0) {
                state->current_player_index = i;
                break;
            }
        }
    }

    // Update player label
    if (state->player_label) {
        if (state->player_display_name) {
            gtk_label_set_text(GTK_LABEL(state->player_label), state->player_display_name);
        } else if (state->player_count > 0) {
            gtk_label_set_text(GTK_LABEL(state->player_label), "Click to switch");
        } else {
            gtk_label_set_text(GTK_LABEL(state->player_label), "No players");
        }
    }
}

// Save preferred player to config file
static void save_preferred_player(const gchar *bus_name) {
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "hyprwave", NULL);
    gchar *pref_file = g_build_filename(config_dir, "preferred_player", NULL);
    GError *error = NULL;

    g_mkdir_with_parents(config_dir, 0755);
    if (!g_file_set_contents(pref_file, bus_name, -1, &error)) {
        g_warning("Failed to save preferred player: %s", error->message);
        g_error_free(error);
    }

    g_free(pref_file);
    g_free(config_dir);
}

// Load preferred player from config file
static gchar* load_preferred_player(void) {
    gchar *pref_file = g_build_filename(g_get_user_config_dir(), "hyprwave", "preferred_player", NULL);
    gchar *contents = NULL;

    if (g_file_get_contents(pref_file, &contents, NULL, NULL)) {
        g_strstrip(contents);
    }

    g_free(pref_file);
    return contents;
}

// Switch to a specific MPRIS player (by D-Bus name)
static void switch_to_player(AppState *state, const gchar *bus_name) {
    if (!bus_name) return;

    // Disconnect from current player
    if (state->mpris_proxy) {
        g_object_unref(state->mpris_proxy);
        state->mpris_proxy = NULL;
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
        g_free(state->current_player);
        state->current_player = NULL;
        return;
    }

    g_signal_connect(state->mpris_proxy, "g-properties-changed",
                     G_CALLBACK(on_properties_changed), state);

    // Get display name from Identity
    GDBusProxy *player_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        bus_name,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2",
        NULL,
        NULL
    );

    // Set fallback display name from bus_name (e.g., "org.mpris.MediaPlayer2.spotify" -> "spotify")
    g_free(state->player_display_name);
    const gchar *fallback_name = strrchr(bus_name, '.');
    state->player_display_name = g_strdup(fallback_name ? fallback_name + 1 : "Unknown");

    // Try to get better display name from Identity property
    if (player_proxy) {
        GVariant *identity = g_dbus_proxy_get_cached_property(player_proxy, "Identity");
        if (identity) {
            const gchar *id_str = g_variant_get_string(identity, NULL);
            g_free(state->player_display_name);
            state->player_display_name = g_strdup(id_str);
            g_variant_unref(identity);
        }
        g_object_unref(player_proxy);
    }

    // Check if player supports seeking (explicit fetch)
    state->can_seek = FALSE;
    GError *seek_err = NULL;
    GVariant *seek_res = g_dbus_proxy_call_sync(
        state->mpris_proxy,
        "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "CanSeek"),
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &seek_err
    );
    if (seek_res) {
        GVariant *val = NULL;
        g_variant_get(seek_res, "(v)", &val);
        if (val) {
            state->can_seek = g_variant_get_boolean(val);
            g_variant_unref(val);
        }
        g_variant_unref(seek_res);
    } else if (seek_err) {
        g_error_free(seek_err);
    }

    // Update display and save preference
    if (state->player_label) {
        gtk_label_set_text(GTK_LABEL(state->player_label), state->player_display_name);
    }
    save_preferred_player(bus_name);

    g_print("Switched to player: %s (%s)\n", state->player_display_name, bus_name);

    // Suppress notification during player switch
    state->suppress_notification = TRUE;
    update_metadata(state);
    update_playback_status(state);
    state->suppress_notification = FALSE;
}

static void cycle_player(AppState *state, gboolean forward) {
    // Refresh player list from D-Bus
    load_available_players(state);

    if (!state->players || state->player_count == 0) {
        g_print("No MPRIS players available\n");
        return;
    }

    gint new_index;
    if (state->current_player_index < 0) {
        new_index = 0;
    } else if (forward) {
        new_index = (state->current_player_index + 1) % state->player_count;
    } else {
        new_index = (state->current_player_index - 1 + state->player_count) % state->player_count;
    }

    switch_to_player(state, state->players[new_index]);
}

static void on_player_clicked(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    cycle_player(state, TRUE);
}

// ========================================
// SEEK FUNCTIONALITY
// ========================================

// Perform the actual MPRIS seek call
static void perform_seek(AppState *state, gdouble fraction) {
    if (!state->mpris_proxy || state->current_length <= 0)
        return;

    gint64 target_position = (gint64)(fraction * state->current_length);

    if (state->current_track_id && strlen(state->current_track_id) > 0) {
        // Use SetPosition with track ID if available
        g_dbus_proxy_call(state->mpris_proxy, "SetPosition",
            g_variant_new("(ox)", state->current_track_id, target_position),
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    } else {
        // Fallback: Use relative Seek method
        // First get current position to calculate offset
        GError *error = NULL;
        GVariant *pos_result = g_dbus_proxy_call_sync(
            state->mpris_proxy,
            "org.freedesktop.DBus.Properties.Get",
            g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "Position"),
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error
        );

        if (pos_result && !error) {
            GVariant *pos_wrapped;
            g_variant_get(pos_result, "(v)", &pos_wrapped);
            gint64 current_position = get_variant_as_int64(pos_wrapped);
            g_variant_unref(pos_wrapped);
            g_variant_unref(pos_result);

            // Calculate offset and use Seek method
            gint64 offset = target_position - current_position;
            g_dbus_proxy_call(state->mpris_proxy, "Seek",
                g_variant_new("(x)", offset),
                G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
        }
        if (error) g_error_free(error);
    }
}

// Debounce timer for seek - waits for user to stop interacting before seeking
static guint seek_debounce_timer = 0;

// Called after seek delay to allow position updates again
static gboolean seek_cooldown_complete(gpointer user_data) {
    AppState *state = (AppState *)user_data;
    state->is_seeking = FALSE;
    return G_SOURCE_REMOVE;
}

// Called after debounce delay to perform the actual seek
static gboolean perform_seek_debounced(gpointer user_data) {
    AppState *state = (AppState *)user_data;
    seek_debounce_timer = 0;

    gdouble fraction = gtk_range_get_value(GTK_RANGE(state->progress_bar));
    perform_seek(state, fraction);

    // Keep is_seeking TRUE for a bit longer to let the player update its position
    // This prevents the slider from jumping back to the old position
    g_timeout_add(500, seek_cooldown_complete, state);

    return G_SOURCE_REMOVE;
}

// Called when slider value changes (during drag or click)
static void on_seek_value_changed(GtkRange *range, gpointer user_data) {
    AppState *state = (AppState *)user_data;

    // If seeking not supported, ignore
    if (!state->can_seek) return;

    // Mark that user is interacting with the slider
    state->is_seeking = TRUE;

    // Update time display preview
    gdouble fraction = gtk_range_get_value(range);
    if (state->current_length > 0) {
        gint64 len_seconds = state->current_length / 1000000;
        gint64 pos_seconds = (gint64)(fraction * len_seconds);
        gint64 rem_seconds = len_seconds - pos_seconds;
        if (rem_seconds < 0) rem_seconds = 0;
        int mins = rem_seconds / 60;
        int secs = rem_seconds % 60;
        char time_str[32];
        snprintf(time_str, sizeof(time_str), "-%d:%02d", mins, secs);
        gtk_label_set_text(GTK_LABEL(state->time_remaining), time_str);
    }

    // Cancel previous debounce timer
    if (seek_debounce_timer > 0) {
        g_source_remove(seek_debounce_timer);
    }

    // Schedule seek after 150ms of no changes
    seek_debounce_timer = g_timeout_add(150, perform_seek_debounced, state);
}

// ========================================
// SIGNAL HANDLERS FOR KEYBINDS
// ========================================

static void on_window_hide_complete(GObject *revealer, GParamSpec *pspec, gpointer user_data) {
    AppState *state = (AppState *)user_data;

    if (!gtk_revealer_get_child_revealed(GTK_REVEALER(state->window_revealer))) {
        // Actually hide the window to make it completely invisible
        gtk_widget_set_visible(state->window, FALSE);
    }
}

static void handle_sigusr1(int sig) {
    if (!global_state) return;
    
    global_state->is_visible = !global_state->is_visible;
    
    if (!global_state->is_visible) {
        // HIDE: Slide out animation
        // First collapse expanded section if open
        if (global_state->is_expanded) {
            global_state->is_expanded = FALSE;
            gtk_revealer_set_reveal_child(GTK_REVEALER(global_state->revealer), FALSE);
        }
        
        // Slide out the entire window
        gtk_revealer_set_reveal_child(GTK_REVEALER(global_state->window_revealer), FALSE);
        
    } else {
        // SHOW: Slide in animation
        // Make window visible first, then start reveal animation
        gtk_widget_set_visible(global_state->window, TRUE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(global_state->window_revealer), TRUE);
    }
}

static void handle_sigusr2(int sig) {
    if (!global_state) return;
    if (!global_state->is_visible) return;

    // Toggle expand by simulating button click
    on_expand_clicked(NULL, global_state);
}

// ========================================
// HELPER FUNCTIONS
// ========================================

static gint64 get_variant_as_int64(GVariant *value) {
    if (value == NULL) return 0;
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT64)) {
        return g_variant_get_int64(value);
    } 
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64)) {
        return (gint64)g_variant_get_uint64(value);
    }
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
        return (gint64)g_variant_get_int32(value);
    }
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
        return (gint64)g_variant_get_uint32(value);
    }
    else if (g_variant_is_of_type(value, G_VARIANT_TYPE_DOUBLE)) {
        return (gint64)g_variant_get_double(value);
    }
    return 0;
}

static gboolean update_position_tick(gpointer user_data) {
    AppState *state = (AppState *)user_data;
    update_position(state);
    return G_SOURCE_CONTINUE;
}

static void update_position(AppState *state) {
    if (!state->mpris_proxy) return;

    // Don't update position while user is seeking
    if (state->is_seeking) return;

    GError *error = NULL;
    GVariant *position_container = g_dbus_proxy_call_sync(
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

    GVariant *position_val_wrapped;
    g_variant_get(position_container, "(v)", &position_val_wrapped);
    gint64 position = get_variant_as_int64(position_val_wrapped);
    g_variant_unref(position_val_wrapped);
    g_variant_unref(position_container);

    gint64 length = state->current_length;
    if (length <= 0) {
        // Fallback: try to get length from metadata if not cached
        GVariant *metadata_var = g_dbus_proxy_get_cached_property(state->mpris_proxy, "Metadata");
        if (metadata_var) {
            GVariantIter iter;
            gchar *key;
            GVariant *val;
            g_variant_iter_init(&iter, metadata_var);
            while (g_variant_iter_loop(&iter, "{sv}", &key, &val)) {
                if (g_strcmp0(key, "mpris:length") == 0) {
                    length = get_variant_as_int64(val);
                    state->current_length = length;
                    break;
                }
            }
            g_variant_unref(metadata_var);
        }
    }

    char time_str[32];
    double fraction = 0.0;
    gint64 pos_seconds = position / 1000000;

    if (length > 0) {
        gint64 len_seconds = length / 1000000;
        gint64 rem_seconds = len_seconds - pos_seconds;
        if (rem_seconds < 0) rem_seconds = 0;
        int mins = rem_seconds / 60;
        int secs = rem_seconds % 60;
        snprintf(time_str, sizeof(time_str), "-%d:%02d", mins, secs);
        fraction = (double)position / (double)length;
    } else {
        int mins = pos_seconds / 60;
        int secs = pos_seconds % 60;
        snprintf(time_str, sizeof(time_str), "%d:%02d", mins, secs);
        fraction = 0.0;
    }

    if (fraction > 1.0) fraction = 1.0;
    if (fraction < 0.0) fraction = 0.0;

    gtk_label_set_text(GTK_LABEL(state->time_remaining), time_str);

    // Block signal handler to prevent triggering seek during programmatic update
    if (state->seek_handler_id > 0) {
        g_signal_handler_block(state->progress_bar, state->seek_handler_id);
    }
    gtk_range_set_value(GTK_RANGE(state->progress_bar), fraction);
    if (state->seek_handler_id > 0) {
        g_signal_handler_unblock(state->progress_bar, state->seek_handler_id);
    }
}

// Track notification retry count
static gint notification_retry_count = 0;
#define MAX_NOTIFICATION_RETRIES 5

// Debounced notification callback - shows notification after delay
static gboolean show_pending_notification(gpointer user_data) {
    AppState *state = (AppState *)user_data;

    gboolean has_title = state->pending_title && strlen(state->pending_title) > 0;
    gboolean has_artist = state->pending_artist && strlen(state->pending_artist) > 0;

    // Wait for at least title and artist, retry if not ready yet
    if (!has_title || !has_artist) {
        notification_retry_count++;
        if (notification_retry_count < MAX_NOTIFICATION_RETRIES) {
            // Reschedule - keep timer ID updated
            state->notification_timer = g_timeout_add(200, show_pending_notification, state);
            return G_SOURCE_REMOVE;
        }
    } else {
        // We have complete data - show notification
        notification_show(state->notification,
                          state->pending_title,
                          state->pending_artist,
                          state->pending_art_url,
                          "Now Playing");
    }

    // Clear pending data and reset
    g_free(state->pending_title);
    g_free(state->pending_artist);
    g_free(state->pending_art_url);
    state->pending_title = NULL;
    state->pending_artist = NULL;
    state->pending_art_url = NULL;
    state->notification_timer = 0;
    notification_retry_count = 0;

    return G_SOURCE_REMOVE;
}

static void update_metadata(AppState *state) {
    if (!state->mpris_proxy) return;

    GVariant *metadata = g_dbus_proxy_get_cached_property(state->mpris_proxy, "Metadata");
    if (!metadata) return;

    GVariantIter iter;
    GVariant *value;
    gchar *key;

    // Use owned copies since g_variant_iter_loop frees data each iteration
    gchar *title = NULL;
    gchar *artist = NULL;
    gchar *art_url = NULL;
    gchar *track_id = NULL;
    gint64 length = 0;
    gint64 bitrate = 0;

    g_variant_iter_init(&iter, metadata);
    while (g_variant_iter_loop(&iter, "{sv}", &key, &value)) {
        if (g_strcmp0(key, "xesam:title") == 0) {
            g_free(title);
            title = g_strdup(g_variant_get_string(value, NULL));
        }
        else if (g_strcmp0(key, "xesam:artist") == 0) {
            if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING_ARRAY)) {
                gsize arr_length;
                const gchar **artists = g_variant_get_strv(value, &arr_length);
                if (arr_length > 0) {
                    g_free(artist);
                    artist = g_strdup(artists[0]);
                }
                g_free(artists);
            }
        }
        else if (g_strcmp0(key, "mpris:artUrl") == 0) {
            g_free(art_url);
            art_url = g_strdup(g_variant_get_string(value, NULL));
        }
        else if (g_strcmp0(key, "mpris:trackid") == 0) {
            g_free(track_id);
            track_id = g_strdup(g_variant_get_string(value, NULL));
        }
        else if (g_strcmp0(key, "mpris:length") == 0) {
            length = get_variant_as_int64(value);
        }
        else if (g_strcmp0(key, "xesam:audioBitrate") == 0) {
            bitrate = get_variant_as_int64(value);
        }
    }

    // Store track info for seek operations
    if (track_id) {
        g_free(state->current_track_id);
        state->current_track_id = g_strdup(track_id);
    }
    state->current_length = length;
    
    // Check if track changed for notification
    // Use track_id OR title change (fallback for buggy MPRIS bridges like roon-mpris)
    gboolean track_changed = FALSE;
    if (track_id && state->last_track_id) {
        track_changed = (g_strcmp0(track_id, state->last_track_id) != 0);
    } else if (track_id && !state->last_track_id) {
        track_changed = TRUE;
    }

    // Fallback: also check title change if track_id stayed same
    // (workaround for MPRIS bridges that don't update track_id properly)
    if (!track_changed && title && state->last_title) {
        track_changed = (g_strcmp0(title, state->last_title) != 0);
    }

    // Update last track ID and title
    if (track_id) {
        g_free(state->last_track_id);
        state->last_track_id = g_strdup(track_id);
    }
    if (title) {
        g_free(state->last_title);
        state->last_title = g_strdup(title);
    }
    // Handle notification with debounce
    if (state->layout->notifications_enabled &&
        state->layout->now_playing_enabled && state->notification) {

        if (track_changed && !state->suppress_notification) {
            // New track - cancel any pending notification and schedule new one
            if (state->notification_timer > 0) {
                g_source_remove(state->notification_timer);
                state->notification_timer = 0;
            }

            // Reset retry count for new track
            notification_retry_count = 0;

            // Store pending notification data (make copies)
            g_free(state->pending_title);
            g_free(state->pending_artist);
            g_free(state->pending_art_url);
            state->pending_title = g_strdup(title);
            state->pending_artist = g_strdup(artist);
            state->pending_art_url = g_strdup(art_url);

            // Pre-load notification album art NOW before temp file gets deleted
            // (Chromium uses ephemeral temp files that disappear quickly)
            if (state->notification->album_cover) {
                // Clear old art first, then try to load new art
                clear_album_art_container(state->notification->album_cover);
                load_album_art_to_container(art_url, state->notification->album_cover, 70);
            }

            // Schedule notification after 300ms delay to ensure metadata is complete
            state->notification_timer = g_timeout_add(300, show_pending_notification, state);

        } else if (state->notification_timer > 0) {
            // Same track but notification pending - update with latest data
            // This handles the case where metadata arrives in multiple signals
            g_free(state->pending_title);
            g_free(state->pending_artist);
            g_free(state->pending_art_url);
            state->pending_title = g_strdup(title);
            state->pending_artist = g_strdup(artist);
            state->pending_art_url = g_strdup(art_url);
        }
    }
    
    if (title && strlen(title) > 0) {
        gtk_label_set_text(GTK_LABEL(state->track_title), title);
    } else {
        gtk_label_set_text(GTK_LABEL(state->track_title), "No Track Playing");
    }
    
    if (artist && strlen(artist) > 0) {
        gtk_label_set_text(GTK_LABEL(state->artist_label), artist);
    } else {
        const gchar *current = gtk_label_get_text(GTK_LABEL(state->artist_label));
        if (g_strcmp0(current, "Unknown Artist") == 0 || strlen(current) == 0) {
            gtk_label_set_text(GTK_LABEL(state->artist_label), "Unknown Artist");
        }
    }
    
    // Handle album art using shared utility
    load_album_art_to_container(art_url, state->album_cover, 120);
    
    // Set source (player name)
    if (state->current_player) {
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
                g_variant_unref(identity);
            }
            g_object_unref(player_proxy);
        } else if (error) {
            g_error_free(error);
        }
    }

    // Update format/bitrate label
    if (state->format_label) {
        if (bitrate > 0) {
            // Bitrate is in bps, convert to kbps for display
            gint kbps = (gint)(bitrate / 1000);
            gchar *format_str = g_strdup_printf("%d kbps", kbps);
            gtk_label_set_text(GTK_LABEL(state->format_label), format_str);
            g_free(format_str);
            gtk_widget_set_visible(state->format_label, TRUE);
        } else {
            // No bitrate info available - hide the label
            gtk_widget_set_visible(state->format_label, FALSE);
        }
    }

    // Cleanup allocated strings
    g_free(title);
    g_free(artist);
    g_free(art_url);
    g_free(track_id);

    g_variant_unref(metadata);
    update_position(state);
}

// Update play button icon based on current playback status
static void update_playback_status(AppState *state) {
    if (!state->mpris_proxy) return;

    GVariant *status_var = g_dbus_proxy_get_cached_property(state->mpris_proxy, "PlaybackStatus");
    if (status_var) {
        const gchar *status = g_variant_get_string(status_var, NULL);
        state->is_playing = g_strcmp0(status, "Playing") == 0;

        if (state->is_playing) {
            gchar *icon_path = get_icon_path("pause.svg");
            gtk_image_set_from_file(GTK_IMAGE(state->play_icon), icon_path);
            free_path(icon_path);
            // Add playing animation
            if (state->play_button) {
                gtk_widget_add_css_class(state->play_button, "playing");
            }
        } else {
            gchar *icon_path = get_icon_path("play.svg");
            gtk_image_set_from_file(GTK_IMAGE(state->play_icon), icon_path);
            free_path(icon_path);
            // Remove playing animation
            if (state->play_button) {
                gtk_widget_remove_css_class(state->play_button, "playing");
            }
        }
        g_variant_unref(status_var);
    }
}

static gboolean refresh_playback_status(gpointer user_data) {
    AppState *state = (AppState *)user_data;
    if (!state->mpris_proxy) return G_SOURCE_REMOVE;

    // Force fetch PlaybackStatus via D-Bus call (bypass cache)
    GError *error = NULL;
    GVariant *result = g_dbus_proxy_call_sync(
        state->mpris_proxy,
        "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "PlaybackStatus"),
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &error
    );

    if (result) {
        GVariant *value = NULL;
        g_variant_get(result, "(v)", &value);
        if (value) {
            const gchar *status = g_variant_get_string(value, NULL);
            state->is_playing = g_strcmp0(status, "Playing") == 0;

            if (state->is_playing) {
                gchar *icon_path = get_icon_path("pause.svg");
                gtk_image_set_from_file(GTK_IMAGE(state->play_icon), icon_path);
                free_path(icon_path);
                if (state->play_button) {
                    gtk_widget_add_css_class(state->play_button, "playing");
                }
            } else {
                gchar *icon_path = get_icon_path("play.svg");
                gtk_image_set_from_file(GTK_IMAGE(state->play_icon), icon_path);
                free_path(icon_path);
                if (state->play_button) {
                    gtk_widget_remove_css_class(state->play_button, "playing");
                }
            }
            g_variant_unref(value);
        }
        g_variant_unref(result);
    } else if (error) {
        g_error_free(error);
    }

    return G_SOURCE_REMOVE;
}

static void on_properties_changed(GDBusProxy *proxy, GVariant *changed_properties,
                                  GStrv invalidated_properties, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    update_metadata(state);
    // Use explicit fetch instead of cached properties for reliable status
    refresh_playback_status(state);
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

    // Get display name from Identity for all players
    GDBusProxy *player_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        bus_name,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2",
        NULL,
        NULL
    );

    if (player_proxy) {
        GVariant *identity = g_dbus_proxy_get_cached_property(player_proxy, "Identity");
        if (identity) {
            const gchar *id_str = g_variant_get_string(identity, NULL);
            g_free(state->player_display_name);
            state->player_display_name = g_strdup(id_str);
            if (state->player_label) {
                gtk_label_set_text(GTK_LABEL(state->player_label), state->player_display_name);
            }
            g_variant_unref(identity);
        }
        g_object_unref(player_proxy);
    }

    // Check if player supports seeking (explicit fetch, don't rely on cache)
    state->can_seek = FALSE;
    GError *seek_error = NULL;
    GVariant *seek_result = g_dbus_proxy_call_sync(
        state->mpris_proxy,
        "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", "org.mpris.MediaPlayer2.Player", "CanSeek"),
        G_DBUS_CALL_FLAGS_NONE,
        -1, NULL, &seek_error
    );
    if (seek_result) {
        GVariant *seek_value = NULL;
        g_variant_get(seek_result, "(v)", &seek_value);
        if (seek_value) {
            state->can_seek = g_variant_get_boolean(seek_value);
            g_variant_unref(seek_value);
        }
        g_variant_unref(seek_result);
    } else if (seek_error) {
        g_error_free(seek_error);
    }
    g_print("CanSeek: %s\n", state->can_seek ? "true" : "false");

    // Suppress notification on initial connection
    state->suppress_notification = TRUE;
    update_metadata(state);
    update_playback_status(state);
    state->suppress_notification = FALSE;
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
    gchar *first_player = NULL;
    gchar *preferred_player = load_preferred_player();

    // Find MPRIS players
    while (g_variant_iter_loop(iter, "&s", &name)) {
        if (g_str_has_prefix(name, "org.mpris.MediaPlayer2.") &&
            !is_excluded_player(name) &&
            is_allowed_chromium_player(name)) {
            // Check for preferred player first
            if (preferred_player && g_strcmp0(name, preferred_player) == 0) {
                connect_to_player(state, name);
                g_variant_iter_free(iter);
                g_variant_unref(result);
                g_object_unref(dbus_proxy);
                g_free(preferred_player);
                g_free(first_player);
                return;
            }
            // Remember first valid player as fallback
            if (!first_player) {
                first_player = g_strdup(name);
            }
        }
    }

    g_free(preferred_player);

    // Use first available player if no preferred found
    if (first_player) {
        connect_to_player(state, first_player);
        g_free(first_player);
    }

    g_variant_iter_free(iter);
    g_variant_unref(result);
    g_object_unref(dbus_proxy);
}

static void on_play_clicked(GtkButton *button, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    
    if (!state->mpris_proxy) {
        find_active_player(state);
        return;
    }
    
    g_dbus_proxy_call(state->mpris_proxy, "PlayPause", NULL,
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void on_prev_clicked(GtkButton *button, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    if (!state->mpris_proxy) return;
    
    g_dbus_proxy_call(state->mpris_proxy, "Previous", NULL,
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void on_next_clicked(GtkButton *button, gpointer user_data) {
    AppState *state = (AppState *)user_data;
    if (!state->mpris_proxy) return;
    
    g_dbus_proxy_call(state->mpris_proxy, "Next", NULL,
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
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
    
    // Update icon using layout helper
    const gchar *icon_name = layout_get_expand_icon(state->layout, state->is_expanded);
    gchar *icon_path = get_icon_path(icon_name);
    gtk_image_set_from_file(GTK_IMAGE(state->expand_icon), icon_path);
    free_path(icon_path);
    
    gtk_revealer_set_reveal_child(GTK_REVEALER(state->revealer), state->is_expanded);
}

static void load_css() {
    // 1. Load base styles (style.css)
    gchar *css_path = get_style_path();
    g_print("Loading base CSS from: %s\n", css_path);

    GtkCssProvider *provider = gtk_css_provider_new();
    GFile *base_file = g_file_new_for_path(css_path);
    GError *base_error = NULL;
    gchar *base_contents = NULL;
    gsize base_length = 0;

    if (g_file_load_contents(base_file, NULL, &base_contents, &base_length, NULL, &base_error)) {
        gtk_css_provider_load_from_string(provider, base_contents);
        gtk_style_context_add_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        g_print("Base CSS loaded successfully\n");
        g_free(base_contents);
    } else {
        g_warning("Failed to load base CSS from '%s': %s",
                  css_path, base_error ? base_error->message : "Unknown error");
        if (base_error) g_error_free(base_error);
    }
    g_object_unref(base_file);
    g_object_unref(provider);
    free_path(css_path);

    // 2. Load theme CSS if not using default "light" theme
    gchar *theme = get_config_theme();
    g_print("Theme from config: %s\n", theme);

    gchar *theme_path = get_theme_path(theme);
    if (theme_path) {
        GtkCssProvider *theme_provider = gtk_css_provider_new();
        GFile *theme_file = g_file_new_for_path(theme_path);
        GError *theme_error = NULL;
        gchar *theme_contents = NULL;
        gsize theme_length = 0;

        if (g_file_load_contents(theme_file, NULL, &theme_contents, &theme_length, NULL, &theme_error)) {
            gtk_css_provider_load_from_string(theme_provider, theme_contents);
            gtk_style_context_add_provider_for_display(
                gdk_display_get_default(),
                GTK_STYLE_PROVIDER(theme_provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1
            );
            g_print("Theme CSS loaded: %s\n", theme_path);
            g_free(theme_contents);
        } else {
            g_warning("Failed to load theme CSS from '%s': %s",
                      theme_path, theme_error ? theme_error->message : "Unknown error");
            if (theme_error) g_error_free(theme_error);
        }
        g_object_unref(theme_file);
        g_object_unref(theme_provider);
        free_path(theme_path);
    }
    g_free(theme);

    // 3. Load optional user CSS overrides with highest priority
    gchar *user_css = g_build_filename(g_get_user_config_dir(), "hyprwave", "user.css", NULL);
    if (g_file_test(user_css, G_FILE_TEST_EXISTS)) {
        GtkCssProvider *user_provider = gtk_css_provider_new();
        GFile *css_file = g_file_new_for_path(user_css);
        GError *css_error = NULL;
        gchar *css_contents = NULL;
        gsize css_length = 0;

        // Read and load CSS with error checking
        if (g_file_load_contents(css_file, NULL, &css_contents, &css_length, NULL, &css_error)) {
            gtk_css_provider_load_from_string(user_provider, css_contents);
            gtk_style_context_add_provider_for_display(
                gdk_display_get_default(),
                GTK_STYLE_PROVIDER(user_provider),
                GTK_STYLE_PROVIDER_PRIORITY_USER
            );
            g_print("User CSS loaded from: %s\n", user_css);
            g_free(css_contents);
        } else {
            g_warning("Failed to load user CSS from '%s': %s",
                      user_css, css_error ? css_error->message : "Unknown error");
            if (css_error) g_error_free(css_error);
        }
        g_object_unref(css_file);
        g_object_unref(user_provider);
    }
    g_free(user_css);
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppState *state = g_new0(AppState, 1);
    state->is_playing = FALSE;
    state->is_expanded = FALSE;
    state->is_visible = TRUE;
    state->suppress_notification = FALSE;
    state->mpris_proxy = NULL;
    state->current_player = NULL;
    state->last_track_id = NULL;
    state->last_title = NULL;
    state->layout = layout_load_config();
    
    // Initialize notification system
    state->notification = notification_init(app);
    if (!state->notification) {
        g_printerr("Failed to initialize notification system\n");
    }
    
    GtkWidget *window = gtk_application_window_new(app);
    state->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "HyprWave");
    
    // Layer Shell Setup
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_exclusive_zone(GTK_WINDOW(window), 0);
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_namespace(GTK_WINDOW(window), "hyprwave");
    
    // Setup anchors using layout module
    layout_setup_window_anchors(GTK_WINDOW(window), state->layout);
    
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    
    // Set window as transparent app-paintable
    gtk_widget_set_name(window, "hyprwave-window");
    
    // CRITICAL: Make the window background fully transparent
    gtk_widget_add_css_class(window, "hyprwave-window");
    
    // Create widget elements
    GtkWidget *album_cover = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->album_cover = album_cover;
    gtk_widget_add_css_class(album_cover, "album-cover");
    gtk_widget_set_size_request(album_cover, 120, 120);
    gtk_widget_set_halign(album_cover, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(album_cover, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(album_cover, FALSE);
    gtk_widget_set_vexpand(album_cover, FALSE);
    gtk_widget_set_overflow(album_cover, GTK_OVERFLOW_HIDDEN);
    
    GtkWidget *source_label = gtk_label_new("No Source");
    state->source_label = source_label;
    gtk_widget_add_css_class(source_label, "source-label");

    // Format/bitrate label (shown when available)
    GtkWidget *format_label = gtk_label_new("");
    state->format_label = format_label;
    gtk_widget_add_css_class(format_label, "format-label");
    gtk_widget_set_visible(format_label, FALSE);  // Hidden until bitrate is available

    // Player label (clickable to cycle players)
    GtkWidget *player_label = gtk_label_new("No Player");
    state->player_label = player_label;
    gtk_widget_add_css_class(player_label, "player-label");

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
    
    // Progress bar is now a GtkScale for seek support
    GtkWidget *progress_bar = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.001);
    state->progress_bar = progress_bar;
    gtk_widget_add_css_class(progress_bar, "track-progress");
    gtk_scale_set_draw_value(GTK_SCALE(progress_bar), FALSE);  // Hide numeric value
    gtk_range_set_value(GTK_RANGE(progress_bar), 0.0);
    gtk_widget_set_size_request(progress_bar, 140, 16);  // Taller for easier clicking

    // Connect seek signal - uses debounce to perform seek after user stops interacting
    // Store handler ID so we can block it during programmatic updates
    state->seek_handler_id = g_signal_connect(progress_bar, "value-changed", G_CALLBACK(on_seek_value_changed), state);

    GtkWidget *time_remaining = gtk_label_new("--:--");
    state->time_remaining = time_remaining;
    gtk_widget_add_css_class(time_remaining, "time-remaining");

    // Create expanded section using layout module
    // Add click gesture to player label for cycling players
    GtkGesture *player_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(player_click), GDK_BUTTON_PRIMARY);
    g_signal_connect(player_click, "pressed", G_CALLBACK(on_player_clicked), state);
    gtk_widget_add_controller(player_label, GTK_EVENT_CONTROLLER(player_click));

    ExpandedWidgets expanded_widgets = {
        .album_cover = album_cover,
        .source_label = source_label,
        .format_label = format_label,
        .player_label = player_label,
        .track_title = track_title,
        .artist_label = artist_label,
        .progress_bar = progress_bar,
        .time_remaining = time_remaining
    };

    GtkWidget *expanded_section = layout_create_expanded_section(state->layout, &expanded_widgets);
    
    // Create revealer
    GtkWidget *revealer = gtk_revealer_new();
    state->revealer = revealer;
    gtk_revealer_set_transition_type(GTK_REVEALER(revealer), layout_get_transition_type(state->layout));
    gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 300);
    gtk_revealer_set_child(GTK_REVEALER(revealer), expanded_section);
    gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
    g_signal_connect(revealer, "notify::child-revealed", G_CALLBACK(on_revealer_transition_done), state);
    
    // Create buttons
    // Previous button
    GtkWidget *prev_btn = gtk_button_new();
    gtk_widget_set_size_request(prev_btn, 44, 44);
    gchar *prev_icon_path = get_icon_path("previous.svg");
    GtkWidget *prev_icon = gtk_image_new_from_file(prev_icon_path);
    free_path(prev_icon_path);
    gtk_image_set_pixel_size(GTK_IMAGE(prev_icon), 20);
    gtk_button_set_child(GTK_BUTTON(prev_btn), prev_icon);
    gtk_widget_add_css_class(prev_btn, "control-button");
    gtk_widget_add_css_class(prev_btn, "prev-button");
    g_signal_connect(prev_btn, "clicked", G_CALLBACK(on_prev_clicked), state);
    
    // Play button
    GtkWidget *play_btn = gtk_button_new();
    state->play_button = play_btn;
    gtk_widget_set_size_request(play_btn, 44, 44);
    gchar *play_icon_path = get_icon_path("play.svg");
    GtkWidget *play_icon = gtk_image_new_from_file(play_icon_path);
    free_path(play_icon_path);
    state->play_icon = play_icon;
    gtk_image_set_pixel_size(GTK_IMAGE(play_icon), 20);
    gtk_button_set_child(GTK_BUTTON(play_btn), play_icon);
    gtk_widget_add_css_class(play_btn, "control-button");
    gtk_widget_add_css_class(play_btn, "play-button");
    g_signal_connect(play_btn, "clicked", G_CALLBACK(on_play_clicked), state);
    
    // Next button
    GtkWidget *next_btn = gtk_button_new();
    gtk_widget_set_size_request(next_btn, 44, 44);
    gchar *next_icon_path = get_icon_path("next.svg");
    GtkWidget *next_icon = gtk_image_new_from_file(next_icon_path);
    free_path(next_icon_path);
    gtk_image_set_pixel_size(GTK_IMAGE(next_icon), 20);
    gtk_button_set_child(GTK_BUTTON(next_btn), next_icon);
    gtk_widget_add_css_class(next_btn, "control-button");
    gtk_widget_add_css_class(next_btn, "next-button");
    g_signal_connect(next_btn, "clicked", G_CALLBACK(on_next_clicked), state);
    
    // Expand button
    GtkWidget *expand_btn = gtk_button_new();
    gtk_widget_set_size_request(expand_btn, 44, 44);
    const gchar *initial_icon_name = layout_get_expand_icon(state->layout, FALSE);
    gchar *expand_icon_path = get_icon_path(initial_icon_name);
    GtkWidget *expand_icon = gtk_image_new_from_file(expand_icon_path);
    free_path(expand_icon_path);
    state->expand_icon = expand_icon;
    gtk_image_set_pixel_size(GTK_IMAGE(expand_icon), 20);
    gtk_button_set_child(GTK_BUTTON(expand_btn), expand_icon);
    gtk_widget_add_css_class(expand_btn, "control-button");
    gtk_widget_add_css_class(expand_btn, "expand-button");
    g_signal_connect(expand_btn, "clicked", G_CALLBACK(on_expand_clicked), state);
    
    // Create control bar using layout module
    GtkWidget *control_bar = layout_create_control_bar(state->layout, &prev_btn, &play_btn, &next_btn, &expand_btn);
    
    // Create main container using layout module
    GtkWidget *main_container = layout_create_main_container(state->layout, control_bar, revealer);
    
    // Wrap main_container in a window-level revealer for hide/show animation
    GtkWidget *window_revealer = gtk_revealer_new();
    state->window_revealer = window_revealer;
    
    // Set transition based on layout edge
    GtkRevealerTransitionType window_transition;
    if (state->layout->edge == EDGE_RIGHT) {
        window_transition = GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT;
    } else if (state->layout->edge == EDGE_LEFT) {
        window_transition = GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT;
    } else if (state->layout->edge == EDGE_TOP) {
        window_transition = GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP;
    } else {
        window_transition = GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN;
    }
    
    gtk_revealer_set_transition_type(GTK_REVEALER(window_revealer), window_transition);
    gtk_revealer_set_transition_duration(GTK_REVEALER(window_revealer), 300);
    gtk_revealer_set_child(GTK_REVEALER(window_revealer), main_container);
    gtk_revealer_set_reveal_child(GTK_REVEALER(window_revealer), TRUE);
    
    g_signal_connect(window_revealer, "notify::child-revealed", 
                     G_CALLBACK(on_window_hide_complete), state);
    
    gtk_window_set_child(GTK_WINDOW(window), window_revealer);
    gtk_window_present(GTK_WINDOW(window));
    
    // Setup signal handlers for keybinds
    global_state = state;
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    
    // Print keybind setup instructions on first run
    static gboolean first_run_check = FALSE;
    gchar *config_check = g_build_filename(g_get_user_config_dir(), "hyprwave", ".setup_complete", NULL);
    
    if (!g_file_test(config_check, G_FILE_TEST_EXISTS)) {
        first_run_check = TRUE;
        // Create marker file
        g_file_set_contents(config_check, "1", -1, NULL);
    }
    g_free(config_check);
    
    if (first_run_check) {
        g_print("\n");
        g_print("═══════════════════════════════════════════════════════════\n");
        g_print("  🎵 HyprWave Keybind Setup\n");
        g_print("═══════════════════════════════════════════════════════════\n");
        g_print("\n");
        g_print("Add these keybinds to your compositor config:\n");
        g_print("\n");
        g_print("Hyprland (~/.config/hypr/hyprland.conf):\n");
        g_print("  bind = SUPER_SHIFT, M, exec, hyprwave-toggle visibility\n");
        g_print("  bind = SUPER, M, exec, hyprwave-toggle expand\n");
        g_print("\n");
        g_print("Niri (~/.config/niri/config.kdl):\n");
        g_print("  binds {\n");
        g_print("      Mod+Shift+M { spawn \"hyprwave-toggle\" \"visibility\"; }\n");
        g_print("      Mod+M { spawn \"hyprwave-toggle\" \"expand\"; }\n");
        g_print("  }\n");
        g_print("\n");
        g_print("Then reload your compositor config.\n");
        g_print("\n");
        g_print("Keybinds:\n");
        g_print("  • %s - Hide/show HyprWave\n", state->layout->toggle_visibility_bind);
        g_print("  • %s - Toggle album details\n", state->layout->toggle_expand_bind);
        g_print("\n");
        g_print("═══════════════════════════════════════════════════════════\n");
        g_print("\n");
    }
    
    // Find and connect to active media player
    find_active_player(state);

    // Load available players
    load_available_players(state);

    // Update position every second
    state->update_timer = g_timeout_add_seconds(1, update_position_tick, state);

    // Refresh playback status after main loop starts (D-Bus cache delay)
    g_timeout_add(500, refresh_playback_status, state);

    g_print("HyprWave Hi-Fi Edition v%s started!\n", HYPRWAVE_VERSION);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.hyprwave.app", G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(app, "startup", G_CALLBACK(load_css), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
