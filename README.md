# HyprWave Hi-Fi Edition

[![AUR version](https://img.shields.io/aur/version/hyprwave-hifi?label=AUR%20hyprwave-hifi)](https://aur.archlinux.org/packages/hyprwave-hifi)
[![AUR version](https://img.shields.io/aur/version/hyprwave-hifi-git?label=AUR%20hyprwave-hifi-git)](https://aur.archlinux.org/packages/hyprwave-hifi-git)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A sleek, modern music control bar for Wayland compositors, **focused on hi-res playback services** like Roon, Tidal, Qobuz, and HQPlayer.

> **Fork Notice:** This is an enhanced fork of [shantanubaddar/hyprwave](https://github.com/shantanubaddar/hyprwave). The original project focuses on being a simple, sleek music controller. This fork extends it with features tailored for audiophile setups and multi-zone streaming.

Built with GTK4 and gtk4-layer-shell for Hyprland, Niri, Sway, and other Wayland compositors.

## Features

- **Volume Control** - Double-click album cover to reveal volume slider (auto-hides after 3s)
- **Interactive Seek Bar** - Click or drag to seek within tracks (MPRIS CanSeek support)
- **Multi-Player Switching** - Click to cycle through MPRIS players (Roon zones, Tidal, Qobuz, etc.)
- **Player Preference Persistence** - Remembers your preferred player across sessions
- **Now Playing Notifications** - Elegant slide-in notifications for track changes
- **Play Button Animation** - Subtle glow animation when playing
- **Dark Theme** - Config-based theme selection (light/dark)
- **Album Art Display** - Fetches artwork from file:// and http:// URLs
- **Live Progress Tracking** - Real-time progress bar with countdown
- **Configurable Layout** - Position on any screen edge
- **Minimal Resource Usage** - ~80-95MB RAM, <0.3% CPU

## Screenshots

### Dark Theme
| Collapsed | Expanded |
|-----------|----------|
| ![Dark Collapsed](screenshots/dark-collapsed.png) | ![Dark Expanded](screenshots/dark-expanded.png) |

### Light Theme
| Collapsed | Expanded |
|-----------|----------|
| ![Light Collapsed](screenshots/light-collapsed.png) | ![Light Expanded](screenshots/light-expanded.png) |

### Notifications
| Dark | Light |
|------|-------|
| ![Notification Dark](screenshots/notification-dark.png) | ![Notification Light](screenshots/notification-light.png) |

## Supported Players

**Primary Focus (Hi-Res):**
- Roon (via [roon-mpris-multizone](https://github.com/godlyfast/roon-mpris) bridge)
- TIDAL (via tidal-hifi)
- Qobuz (via desktop client or web with MPRIS)
- HQPlayer (with MPRIS integration)
- Cider (Apple Music client)

**Also Works With:**
- Spotify (Desktop app)
- VLC Media Player
- Any MPRIS2-compatible player

**Filtered by Default:**
- Web browsers (limited MPRIS metadata)

## Installation

### Arch Linux (AUR)

```bash
# Stable release
yay -S hyprwave-hifi

# Latest git
yay -S hyprwave-hifi-git
```

### Dependencies (other distros)

```bash
# Ubuntu / Debian
sudo apt install libgtk-4-dev gtk4-layer-shell

# Fedora
sudo dnf install gtk4-devel gtk4-layer-shell-devel
```

### Building from Source

```bash
git clone https://github.com/godlyfast/hyprwave-hifi.git
cd hyprwave-hifi
make
sudo make install PREFIX=/usr
```

System install locations:
- Binary: `/usr/bin/hyprwave`
- Resources: `/usr/share/hyprwave/`
- Toggle script: `/usr/bin/hyprwave-toggle`

User config at `~/.config/hyprwave/config.conf`

## Roon Setup

Roon requires an MPRIS bridge. The multi-zone bridge exposes all Roon zones as separate MPRIS players:

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
1. Run `roon-mpris` (add to autostart)
2. Open Roon -> Settings -> Extensions
3. Enable "Roon MPRIS Multi-Zone Bridge"
4. HyprWave auto-detects all zones

**Player Switching:** Click the player label in the expanded view to cycle through players. Your preference is saved automatically.

## Configuration

Edit `~/.config/hyprwave/config.conf`:

```conf
[General]
# Edge: right, left, top, bottom
edge = right
margin = 10

# Theme: light or dark
theme = dark

[Keybinds]
toggle_visibility = Super+Shift+M
toggle_expand = Super+M

[Notifications]
enabled = true
now_playing = true
```

### Custom CSS

Create `~/.config/hyprwave/user.css` to customize appearance. User CSS loads with highest priority.

Available classes: `.control-container`, `.expanded-section`, `.notification-container`, `.control-button`, `.album-cover`, `.track-title`, `.artist-label`, `.player-label`, `.track-progress`

## Keybinds

### Hyprland

Add to `~/.config/hypr/hyprland.conf`:
```conf
bind = SUPER_SHIFT, M, exec, hyprwave-toggle visibility
bind = SUPER, M, exec, hyprwave-toggle expand
exec-once = hyprwave
```

### Niri

Add to `~/.config/niri/config.kdl`:
```kdl
binds {
    Mod+Shift+M { spawn "hyprwave-toggle" "visibility"; }
    Mod+M { spawn "hyprwave-toggle" "expand"; }
}
spawn-at-startup "hyprwave"
```

### Sway

Add to `~/.config/sway/config`:
```conf
bindsym $mod+Shift+M exec hyprwave-toggle visibility
bindsym $mod+M exec hyprwave-toggle expand
```

## Troubleshooting

### Black box around HyprWave (Hyprland)

Add to `hyprland.conf`:
```conf
layerrule = noblur, hyprwave
layerrule = noblur, hyprwave-notification
```

### Album art not loading

Some players (especially Chromium-based) use ephemeral temp files for album art. This fork includes pre-loading to handle these cases.

## Roadmap

### Current Features
- [x] Volume control (double-click album cover)
- [x] Interactive seek bar with click/drag support
- [x] Multi-player switching with preference persistence
- [x] Dark theme with config-based selection
- [x] Now Playing notifications
- [x] Play button animation (breathing glow)
- [x] Roon multi-zone support

### Planned
- [ ] Qobuz-specific metadata display (bit depth, sample rate)
- [ ] HQPlayer integration improvements
- [ ] Audio quality indicator in UI
- [ ] Multi-zone grouping controls

## Technical Details

- **Language:** C
- **GUI:** GTK4 + gtk4-layer-shell
- **IPC:** D-Bus (MPRIS2)
- **Memory:** ~80-95MB
- **CPU:** <0.3% idle

## Contributing

Contributions welcome! This fork focuses on hi-res playback features.

- [Issues](https://github.com/godlyfast/hyprwave-hifi/issues)
- [Pull Requests](https://github.com/godlyfast/hyprwave-hifi/pulls)

## Acknowledgments

- Original project: [shantanubaddar/hyprwave](https://github.com/shantanubaddar/hyprwave)
- [GTK4](https://gtk.org/)
- [gtk4-layer-shell](https://github.com/wmww/gtk-layer-shell)
- [MPRIS specification](https://www.freedesktop.org/)

## License

Open source. Free to use, modify, and distribute.
