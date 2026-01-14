# 🌊 HyprWave - v0.6 (latest release)

A sleek, modern music control bar for Wayland compositors (Hyprland, Niri, Sway, etc.) with MPRIS integration.


Updates till now:  - Multi-Anchor support, Notifications, Music Controls, CSS Styling (control bar, expanded section and notifications), and launching it as an application, huge UI bug fixing, ability to seek to different song part via click or drag and click, volume controls added in the latest update.

Built and primarily tested on Niri, for all wayland compositors that support GTK4 and GTK4-layer-shell.

Also, Massive update- hyprwave is now on AUR. 
Simply install it with-

```yay -S hyprwave```


It will not give you the bleeding new updates, but the latest releases.

#### 📸 Screenshots

### Vertical Layout

<img width="105" height="305" alt="image" src="https://github.com/user-attachments/assets/7675da1e-d6ae-417d-8201-17e1eb670ed0" />

<img width="313" height="377" alt="image" src="https://github.com/user-attachments/assets/432a85ad-a013-400c-9652-2481ff9a93ad" />

### Horizontal Layout

<img width="388" height="84" alt="image" src="https://github.com/user-attachments/assets/67a477a9-3cdc-4029-941c-d99cff080b16" />

<img width="449" height="275" alt="image" src="https://github.com/user-attachments/assets/0b1bede5-102b-484b-ab23-d7a6f87fe8ac" />


## Right

![right](https://github.com/user-attachments/assets/0ecf38e1-535c-4858-91d8-190e9c9fd55a)



## Top

![top](https://github.com/user-attachments/assets/eb1e279f-e47d-4f37-82ef-833810014fc9)



## Bottom

![bottom](https://github.com/user-attachments/assets/9dab5619-efc3-4523-a135-038c3c6b2551)


## Left


![left](https://github.com/user-attachments/assets/e02741cc-738d-4e3e-a097-89b98934842d)


# HyprWave 🎵

A sleek, modern music control overlay for Wayland compositors (Hyprland, Niri, Sway). Built with GTK4 and gtk4-layer-shell.

## ✨ Features

- **Elegant Design** - Glassmorphic UI with smooth animations
- **MPRIS Integration** - Works with Spotify, VLC, and any MPRIS-compatible player
- **Album Art Display** - Fetches and displays album artwork
- **Live Progress Tracking** - Real-time progress bar with countdown timer
- **Full Playback Controls** - Play/Pause, Next, Previous buttons
- **Expandable Panel** - Toggle to reveal detailed track information
- **Keybind Support** - Hide/show and expand with keyboard shortcuts
- **Now Playing Notifications** - Elegant slide-in notifications for track changes (v0.4.0)
- **Configurable Layout** - Position on any screen edge (left, right, top, bottom)
- **Minimal Resource Usage** - ~80-95MB RAM, <0.3% CPU

### Collapsed State
The control bar sits on your chosen screen edge with essential controls.

### Expanded State
Shows album cover, track title, artist, progress bar, and time remaining.

### Now Playing Notifications
Smooth slide-in notifications appear in the top-right corner when tracks change, showing album art, song title, and artist.

### Volume Control

Double click the album cover to reveal the volume bar, and double click it back to hide it again. Volume bar auto-hides itself after 3 seconds, or after you collapse the expanded state!

## 🚀 Installation

### Dependencies
```bash
# Arch Linux / Manjaro
sudo pacman -S gtk4 gtk4-layer-shell

# Ubuntu / Debian
sudo apt install libgtk-4-dev gtk4-layer-shell

# Fedora
sudo dnf install gtk4-devel gtk4-layer-shell-devel
```

### Building from Source
```bash
# Clone the repository
git clone https://github.com/shantanubaddar/hyprwave.git
cd hyprwave

# Build and install
make
make install
```

The installer will:
- Copy binary to `~/.local/bin/hyprwave`
- Install resources to `~/.local/share/hyprwave/`
- Install `hyprwave-toggle` script for keybinds
- Create config at `~/.config/hyprwave/config.conf`

## 🎵 Usage

### Quick Start

1. **Install and run HyprWave**: `hyprwave`
2. **Start a music player** (Spotify, VLC, etc.)
3. **Control your music** with the on-screen controls
4. **Use keybinds** for quick access (see configuration below)

### Supported Music Players

✅ **Fully Supported:**
- Spotify (Desktop app)
- VLC Media Player
- Any MPRIS2-compatible player (Rhythmbox, Audacious, MPD with mpDris2, etc.)

⚠️ **Limited Support:**
- Web browsers - Basic controls only, limited metadata

## ⚙️ Configuration

### Config File

Edit `~/.config/hyprwave/config.conf`:

```conf
[General]
# Edge to anchor HyprWave to
# Options: right, left, top, bottom
edge = right

# Margin from the screen edge (in pixels)
margin = 10

[Keybinds]
# Keybind labels (for display in setup message)
toggle_visibility = Super+Shift+M
toggle_expand = Super+M

[Notifications]
# Enable/disable notifications
enabled = true

# Show notification when song changes
now_playing = true
```

**Layout Options:**
- **`edge = right`** - Vertical layout on right edge (default)
- **`edge = left`** - Vertical layout on left edge
- **`edge = top`** - Horizontal layout on top edge
- **`edge = bottom`** - Horizontal layout on bottom edge

**Notification Options:**
- **`enabled = true`** - Master switch for all notifications
- **`now_playing = true`** - Show "Now Playing" notifications when tracks change


How notifications will appear on your setup-



https://github.com/user-attachments/assets/7328c91b-c9fa-43ac-a8fd-8c63c9b676d3



### Keybinds

HyprWave supports keybinds for toggling visibility and expanding details. **Add these to your compositor config:**

#### Hyprland

Add to `~/.config/hypr/hyprland.conf`:

```conf
# HyprWave keybinds
bind = SUPER_SHIFT, M, exec, hyprwave-toggle visibility
bind = SUPER, M, exec, hyprwave-toggle expand
```

Then reload: `hyprctl reload`

#### Niri

Add to `~/.config/niri/config.kdl`:

```kdl
binds {
    Mod+Shift+M { spawn "hyprwave-toggle" "visibility"; }
    Mod+M { spawn "hyprwave-toggle" "expand"; }
}
```

Then reload: `niri msg action reload-config`

#### Sway

Add to `~/.config/sway/config`:

```conf
# HyprWave keybinds
bindsym $mod+Shift+M exec hyprwave-toggle visibility
bindsym $mod+M exec hyprwave-toggle expand
```

Then reload: `swaymsg reload`

#### What the keybinds do:

- **Toggle Visibility** (`Super+Shift+M`) - Smoothly hides/shows entire HyprWave with slide animation
- **Toggle Expand** (`Super+M`) - Shows/hides album details
  - If HyprWave is hidden, this will show it AND expand in one smooth motion
 
  ## Here's how they will look-

  

https://github.com/user-attachments/assets/5bd27ec4-6b51-46fb-bf6e-fcb3cb3252b1



### Auto-start

#### Hyprland
Add to `~/.config/hypr/hyprland.conf`:
```conf
exec-once = hyprwave
```

#### Niri
Add to `~/.config/niri/config.kdl`:
```kdl
spawn-at-startup "hyprwave"
```


## 🔧 Troubleshooting

### Black box around HyprWave (Hyprland)

If you see a black box around HyprWave, disable blur for the overlay:

Add to `hyprland.conf`:
```conf
layerrule = noblur, hyprwave
layerrule = noblur, hyprwave-notification
```
If that doesn't work, it most probably is a broken gtk4 or gtk4-layer-shell package- just remove them, reinstall them, and try it again.
### Notifications not appearing

1. Check that notifications are enabled in `~/.config/hyprwave/config.conf`
2. Verify both `enabled = true` and `now_playing = true` under `[Notifications]`
3. Restart HyprWave after config changes

### Keybinds not working

1. Verify `hyprwave-toggle` is installed: `which hyprwave-toggle`
2. Test manually: `hyprwave-toggle visibility` (with HyprWave running)
3. Check your compositor config for syntax errors
4. Reload your compositor config after adding keybinds

### Album art not loading

HyprWave requires the music player to provide album art URLs via MPRIS. Desktop apps work better than web browsers for this.

## 🛠️ Technical Details

- **Language:** C
- **GUI Framework:** GTK4
- **Layer Shell:** gtk4-layer-shell (Wayland overlay)
- **IPC:** D-Bus (MPRIS2 protocol)
- **Memory:** ~80-95MB
- **CPU:** <0.3% idle, <1% during updates

### File Paths

Resources are searched in this order:
1. Local directory: `./icons/`, `./style.css` (for development)
2. User install: `~/.local/share/hyprwave/`
3. System install: `/usr/share/hyprwave/`

Config: `~/.config/hyprwave/config.conf`

## 🗺️ Roadmap

### v0.6.0 (Current and NEW!)
- [ ] Volume control integration
- [ ] Bug and Animation fixes

### v1.0.0 (Goals)
- [ ] Theming system with pre-built themes
- [ ] Custom dimensions and colors via config
- [ ] Plugin system for extensibility

## 🤝 Contributing

Contributions welcome! Feel free to:
- Report bugs via [GitHub Issues](https://github.com/shantanubaddar/hyprwave/issues)
- Submit feature requests
- Create pull requests
- Share your custom themes/icons

## 📝 License

Open source. Free to use, modify, and distribute.

## 🙏 Acknowledgments

- Built with [GTK4](https://gtk.org/)
- Uses [gtk4-layer-shell](https://github.com/wmww/gtk-layer-shell)
- Inspired by [waybar](https://github.com/Alexays/Waybar)
- MPRIS specification by [freedesktop.org](https://www.freedesktop.org/)hyprwave/issues)
- **Discussions:** [GitHub Discussions](https://github.com/shantanubaddar/hyprwave/discussions)
