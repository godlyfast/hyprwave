# 🌊 HyprWave

A sleek, modern music control bar for Wayland compositors (Hyprland, Niri, Sway, etc.) with MPRIS integration.
### Right
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/3692e345-0f3a-4a17-9196-8ff9ba22171f" />
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/a9b45a78-837a-4689-b5a3-0c0c25e1e924" />

### Top
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/9270b4ca-3042-4842-9172-622addf4d56c" />
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/28620bbb-bede-4602-97e9-70a39b83de43" />

### Bottom
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/620b65fe-7d35-4426-ae77-941174c77b35" />
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/4b103396-ee4e-4031-b40c-352119079af0" />

### Left
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/53c24894-f253-40a8-9983-49a43cc7285a" />
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/cc5a9ab6-a262-4991-bf8c-debb4c910164" />


### IMPORTANT
Place the config.conf file by creating a new folder in ~./config, called hyprwave, and place the file there. The main code will read the config.conf from this directory, and hence anchor the hyprwave accordingly. Edit it to your pleasing.


## ✨ Features

- **Elegant Design** - Glassmorphic UI with smooth animations
- **MPRIS Integration** - Works with Spotify, VLC, and any MPRIS-compatible player
- **Album Art Display** - Fetches and displays album artwork (local files and HTTP URLs)
- **Live Progress Tracking** - Real-time progress bar with countdown timer
- **Full Playback Controls** - Play/Pause, Next, Previous buttons
- **Expandable Panel** - Click to reveal detailed track information
- **Minimal Resource Usage** - ~80-95MB RAM, <0.3% CPU

## 📸 Screenshots

### Collapsed State
The control bar sits on the right edge of your screen with essential controls.

### Expanded State
Click the expand button to reveal:
- Album cover art (120x120)
- Source (player name)
- Track title
- Artist name
- Progress bar with visual indicator
- Time remaining

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

# Build
make

# Run
./hyprwave
```

### Installing
```bash
# Install to ~/.local/bin
make install

# Or manually copy
cp hyprwave ~/.local/bin/
cp -r icons ~/.local/share/hyprwave/
cp style.css ~/.local/share/hyprwave/
```

## 🎵 Usage

### Basic Usage

1. **Start a music player** - Launch Spotify, VLC, or any MPRIS-compatible player
2. **Run HyprWave** - Execute `./hyprwave` or `hyprwave` if installed
3. **Control your music** - The bar appears on the right edge with playback controls
4. **Expand for details** - Click the purple expand button (←) to see album info
5. **Collapse** - Click again to hide the details panel

### Supported Music Players

HyprWave works best with **native desktop applications**:

✅ **Fully Supported:**
- Spotify (Desktop app)
- VLC Media Player
- Rhythmbox
- Audacious
- Clementine
- Strawberry
- MPD with mpDris2
- Any MPRIS2-compatible player

⚠️ **Limited Support:**
- Web browsers (Chromium/Brave/Chrome) - Basic controls only, no metadata
- Firefox - Partial metadata support

**Recommendation:** Use desktop music apps for full functionality (album art, track info, etc.)

### Controls

- **▶/⏸ Play/Pause** (Blue button) - Toggle playback
- **⏮ Previous** (Gray button) - Previous track
- **⏭ Next** (Gray button) - Next track
- **← Expand** (Purple button) - Show/hide album details

## ⚙️ Configuration

### Icons

Place your custom SVG icons in the `icons/` directory:
- `play.svg` - Play button icon
- `pause.svg` - Pause button icon
- `next.svg` - Next track icon
- `previous.svg` - Previous track icon
- `arrow-left.svg` - Expand button (collapsed state)
- `arrow-right.svg` - Collapse button (expanded state)
- `arrow-up.svg` - Expand/Collapse button
- `arrow-down.svg`= Expand/Collapse button

### Styling

Customize colors, sizes, and appearance by editing `style.css`. The design uses:
- **Glassmorphic backgrounds** with transparency
- **Gradient buttons** for visual hierarchy
- **Smooth animations** on hover and click
- **Rounded corners** for modern aesthetics

### Auto-start with Hyprland

Add to your `~/.config/hypr/hyprland.conf`:
```conf
exec-once = /path/to/hyprwave
```

### Auto-start with Niri

Add to your `~/.config/niri/config.kdl`:
```kdl
spawn-at-startup "/path/to/hyprwave"
```

## 🛠️ Technical Details

### Architecture

- **Language:** C
- **GUI Framework:** GTK4
- **Layer Shell:** gtk4-layer-shell (for Wayland overlay windows)
- **IPC:** D-Bus (MPRIS2 protocol)
- **Memory Usage:** ~80-95MB
- **CPU Usage:** <0.3% idle, <1% during updates

### MPRIS Integration

HyprWave communicates with media players via the MPRIS D-Bus interface:
- Automatically detects active media players
- Listens for metadata changes in real-time
- Updates position/progress every second
- Fetches album art from local files or HTTP URLs

### IMPORTANT
Place the config.conf file by creating a new directory in ~./config, called hyprwave, and place the file there. The main code will read the config.conf from this directory, and hence anchor the hyprwave accordingly. Edit it to your pleasing.

## 🗺️ Roadmap

### v0.2.0 (Planned)
- [ ] **Keybind toggle** - Show/hide with configurable keybind (e.g., Super+Shift+M)
- [ ] **Slide-in animation** - Smooth reveal from screen edge
- [ ] **Configuration file** - TOML/JSON config for settings
- [ ] **Auto-hide** - Optional auto-hide when not in use

### v0.3.0 (Future)
- [ ] **Multiple players** - Switch between active media players
- [ ] **Theming system** - Pre-built color themes

### v1.0.0 (Goals)
- [ ] **Full customization** - Position, size, colors, layout via config
- [ ] **Plugin system** - Extend functionality
- [ ] **Notification integration** - Show track changes
- [ ] **Volume control** - Integrated volume slider

## 🤝 Contributing

Contributions are welcome! Feel free to:
- Report bugs via GitHub Issues
- Submit feature requests
- Create pull requests
- Improve documentation
- Share your custom themes/icons

## 🙏 Acknowledgments

- Built with [GTK4](https://gtk.org/)
- Uses [gtk4-layer-shell](https://github.com/wmww/gtk-layer-shell)
- Inspired by [waybar](https://github.com/Alexays/Waybar) and modern music players
- MPRIS specification by freedesktop.org

## 💬 Support

- **Issues:** [GitHub Issues](https://github.com/shantanubaddar/hyprwave/issues)
- **Discussions:** [GitHub Discussions](https://github.com/shantanubaddar/hyprwave/discussions)

