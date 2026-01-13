# Go!Listen

<img width="512" alt="image" src="https://github.com/user-attachments/assets/6e518957-d739-41cb-8075-e902230aaa35" /><br>

This is a multimedia overlay plugin for the PlayStation Portable (PSP) made with the purpose of completing the Go!Explore application by adding infotainment capabilities. 

It can play standard MP3 files from `ms0:/MUSIC` as music background and display their metadata and cover art as popups in-game.

_This is currently a beta and as such lacks user controls. Soon™ I might update it to automatically lower the music volume while the navigator is talking._

While this was made specifically for Go!Explore it can be used in some games as a replacement to the old music.prx as this comes with hardware acceleration (the decoder runs on the ME core).

## Installation

### Automatic

You can use [StoreStation](https://github.com/StoreStation/StoreStation) to automatically download, install and enable the plugin!

### Manual

If you prefer manual installation go over to the releases tab and download the `.prx` file. Move it to the `SEPLUGINS/` and add the following line to your PLUGINS.txt (may differ if you are not using ARK-4):

```
UCES00881, ms0:/SEPLUGINS/GO_LISTEN.prx, on
```

## Building

This project can be built using the provided Makefile. You will need a PSP development environment set up.

Don't want to set up an environment? Import this project in [Google IDX](https://idx.google.com) and download the [toolchain](https://github.com/pspdev/pspdev/releases/latest/download/pspdev-ubuntu-latest-x86_64.tar.gz) to `~/pspdev`, the `.idx` scripts will build everything for you.

## License

This project is licensed under the MIT License. See the `LICENSE.md` file for details.

## Special Thanks

* [pspsdk](https://github.com/pspdev/pspsdk)
* [picojpeg](https://github.com/richgel999/picojpeg)
* [llsc-atomic](https://github.com/mcidclan/psp-undocumented-sorcery/blob/main/llsc-atomic/main.h)
* [missyhud](https://github.com/pebeto/missyhud.prx)
