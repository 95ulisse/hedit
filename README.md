# HEdit

HEdit is a simple terminal-based hex editor inspired by VIM.

## Build from source

**Note:** HEdit has a dependency on `libtickit`, which is currently available only on Bazaar,
so you might need to install a Git helper to clone from Bazaar, like `git-remote-bzr`.

```
$ git clone --recursive https://github.com/95ulisse/hedit.git
$ cd hedit
$ make
# make install
```

The build output is in the `out` folder.

## Roadmap

- [x] Undo/Redo
- [ ] Syntax highlighting
- [ ] Scriptability
- [ ] Structure view