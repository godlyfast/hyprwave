# 🌊 HyprWave - v0.4 (latest release)

A sleek, modern music control bar for Wayland compositors (Hyprland, Niri, Sway, etc.) with MPRIS integration.


Updates till now:  - Multi-Anchor support, Notifications, Music Controls, CSS Styling (control bar, expanded section and notifications), and launching it as an application in latest update.

Built and primarily tested on Niri, for all wayland compositors that support GTK4 and GTK4-layer-shell.

#### 📸 Screenshots


### Right
<img width="1331" height="768" alt="image" src="https://github.com/user-attachments/assets/f5e7681d-942e-46f0-84f4-e789dca326cd" />
<img width="1331" height="768" alt="image" src="https://github.com/user-attachments/assets/e32bda28-491a-419c-890b-3fad5975cc98" />

### Top
<img width="1331" height="768" alt="image" src="https://github.com/user-attachments/assets/b1206d4f-a454-4666-bc41-8a34f0619a74" />
<img width="1331" height="768" alt="image" src="https://github.com/user-attachments/assets/3d46dd03-a777-482f-957b-6fcb8891ccea" />


### Bottom
<img width="1331" height="768" alt="image" src="https://github.com/user-attachments/assets/14236ba6-2e4f-4e18-ba58-67c256a43c0d" />
<img width="1331" height="768" alt="image" src="https://github.com/user-attachments/assets/4c4d1b79-4e13-48cb-bf61-3d3f17ac5f7d" />


### Left
<img width="1331" height="768" alt="image" src="https://github.com/user-attachments/assets/697b6ccb-ccad-422e-9d5c-ff75c2cc9645" />
<img width="1331" height="768" alt="image" src="https://github.com/user-attachments/assets/b6f5a8a6-d251-48c7-a7a3-1c19fa119539" />

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
- **Player Switching** - Click to cycle through available MPRIS players (Spotify, VLC, Roon zones, etc.)
- **Configurable Layout** - Position on any screen edge (left, right, top, bottom)
- **Minimal Resource Usage** - ~80-95MB RAM, <0.3% CPU

### Collapsed State
The control bar sits on your chosen screen edge with essential controls.

### Expanded State
Shows album cover, track title, artist, progress bar, and time remaining.

### Now Playing Notifications
Smooth slide-in notifications appear in the top-right corner when tracks change, showing album art, song title, and artist.

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
- Roon (via MPRIS bridge - see setup below)
- Any MPRIS2-compatible player (Rhythmbox, Audacious, MPD with mpDris2, etc.)

⚠️ **Limited Support:**
- Web browsers - Basic controls only, limited metadata

### Roon Setup

Roon requires an MPRIS bridge to work with HyprWave. We recommend the multi-zone bridge which exposes all Roon zones as separate MPRIS players:

```bash
# Arch Linux (AUR)
yay -S roon-mpris-multizone-git
```

Or install from source:
```bash
git clone https://github.com/godlyfast/roon-mpris.git
cd roon-mpris
npm install
```

Then:
1. Run `roon-mpris` (add to autostart for persistence)
2. Open Roon → Settings → Extensions
3. Enable "Roon MPRIS Multi-Zone Bridge"
4. HyprWave will auto-detect all your Roon zones

**Player Switching:** Click the player label in the expanded view to cycle through all available MPRIS players (including Roon zones). Your preference is saved automatically.

**Note:** Volume control requires the Roon app directly.

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

### v0.4.0 (Current) ✨ NEW
- [x] Now Playing notifications with smooth slide animations
- [x] Configurable notification settings
- [x] Album art in notifications
- [x] Multiple player switching (cycle through Spotify, VLC, Roon zones, etc.)

### v0.5.0 (Planned)
- [ ] Volume control integration
- [ ] Customizable notification duration

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

