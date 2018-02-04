# HEdit

HEdit is a simple terminal-based hex editor inspired by VIM and scriptable with JS.

## Build from source

HEdit has two main dependencies:

-  `libtickit`, which is currently available only on Bazaar, and you might need to
   install a Git helper to clone from Bazaar, like `git-remote-bzr`.

- `V8`, which has its own build system. To build `V8` you need to install the `depot_tools`
  from http://dev.chromium.org/developers/how-tos/install-depot-tools, and you must also be sure
  to have Python **2** available and set as the default Python interpreter.

  If you have Python 3 as your default interpreter, a quick-and-dirty just-run-once solution
  to create a temporary environment in which Python 2 is the default might be:
  ```
  $ sudo unshare -m
  # mount --bind /usr/bin/python2 /usr/bin/python
  # su <some-unprivileged-user>
  $ python --version # This should be Python 2.*
  ```

Once you have satisfied all the dependencies, you can compile by issuing a simple `make`.

```
$ git clone --recursive https://github.com/95ulisse/hedit.git
$ cd hedit
$ make
# make install
```

The build output is in the `out` folder.

## Roadmap

- [x] Undo/Redo
- [x] Scriptability
- [ ] Write JS docs
- [ ] Syntax highlighting
- [ ] Structure view