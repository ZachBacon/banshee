# Banshee

A modern, lightweight media player built in C using GTK3, GStreamer 1.0, and SQLite - inspired by the original Banshee Media Player.

![License](https://img.shields.io/badge/license-GPL3-blue.svg)

## Features

- **Modern GTK4 Interface** - Clean and intuitive user interface
- **GStreamer 1.0 Playback** - Support for various audio and video formats
- **SQLite Database** - Fast and efficient media library management
- **Playlist Management** - Create, edit, and organize playlists
- **Track Metadata** - Store and display artist, album, genre information
- **Play Statistics** - Track play counts and recently played tracks
- **Sidebar Navigation** - Easy access to music, videos, playlists, and radio
- **Seek Controls** - Precise playback position control
- **Volume Control** - Built-in volume adjustment

## Architecture

The application is organized into modular components:

- **player.c/h** - GStreamer-based media playback engine
- **database.c/h** - SQLite database layer for media library
- **ui.c/h** - GTK3 user interface components
- **playlist.c/h** - Playlist management with shuffle and repeat
- **main.c** - Application initialization and main loop

## Dependencies

### Required Libraries

- **GTK4** (>= 4.20) - GUI toolkit
- **GStreamer 1.0** (>= 1.14) - Multimedia framework
- **GLib 2.0** (>= 2.56) - Core application library
- **SQLite 3** (>= 3.22) - Database engine

### Ubuntu/Debian Installation

```bash
sudo apt-get update
sudo apt-get install build-essential pkg-config
sudo apt-get install libgtk-4-dev libgstreamer1.0-dev libglib2.0-dev libsqlite3-dev
sudo apt-get install gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly
```

### Fedora/RHEL Installation

```bash
sudo dnf install gcc make pkg-config
sudo dnf install gtk4-devel gstreamer1-devel glib2-devel sqlite-devel
sudo dnf install gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free gstreamer1-plugins-ugly-free
```

### Arch Linux Installation

```bash
sudo pacman -S base-devel pkg-config
sudo pacman -S gtk4 gstreamer glib2 sqlite
sudo pacman -S gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly
```

## Building

### Using Make (Recommended for Quick Build)

```bash
# Build the application
make

# Run the application
make run

# Build with debug symbols
make debug

# Install system-wide (optional)
sudo make install

# Clean build artifacts
make clean
```

### Using Meson (Recommended for Distribution)

```bash
# Configure the build
meson setup builddir

# Compile
meson compile -C builddir

# Install (optional)
sudo meson install -C builddir

# Run
./builddir/banshee
```

## Usage

### Running the Application

After building, you can run the application directly:

```bash
./build/banshee
```

Or if installed system-wide:

```bash
banshee
```

### Database Location

The media library database is stored at:
```
~/.local/share/banshee/library.db
```

### Adding Music

1. Click **Media → Import Media...** in the menu
2. Select your music files or folders
3. The tracks will be added to the library and displayed in the main view

### Playing Music

- Double-click a track in the library to play it
- Use the toolbar buttons to control playback:
  - **Play** - Start playback
  - **Pause** - Pause playback
  - **Stop** - Stop and reset playback
  - **Previous/Next** - Navigate through tracks
- Adjust volume using the volume slider in the bottom-right
- Seek through tracks using the position slider

### Playlists

- Select "Playlists" from the sidebar
- Create new playlists through the Media menu
- Drag and drop tracks to add them to playlists

## Development

### Code Style

- Follow GNU C11 standard
- Use descriptive variable and function names
- Keep functions focused and modular
- Add comments for complex logic

### Supported Formats

The supported formats depend on installed GStreamer plugins:
- **Audio**: MP3, FLAC, OGG, WAV, AAC, M4A
- **Video**: MP4, MKV, AVI, WebM
- **Streaming**: HTTP, HTTPS

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is open source and available under the MIT License.

## Acknowledgments

- Inspired by the original Banshee Media Player
- Built with GTK, GStreamer, and SQLite
- Uses standard Linux development tools

## Roadmap

Future enhancements planned:
- [x] Album art display
- [ ] Audio equalizer
- [ ] Internet radio support
- [ ] Podcast management
- [ ] Smart playlists
- [ ] Media file import wizard
- [ ] Keyboard shortcuts
- [ ] System tray integration
- [ ] MPRIS D-Bus support
- [ ] Metadata editing

## Contact

For bugs, feature requests, or questions, please open an issue on the project repository.