# HEdit [![Build Status](https://travis-ci.org/95ulisse/hedit.svg?branch=master)](https://travis-ci.org/95ulisse/hedit)

HEdit is a simple terminal-based hex editor inspired by VIM and scriptable with JS.

## Build from source

HEdit has two main dependencies:

-  `libtickit`, which is currently available only on Bazaar, so you may need to install it.

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

Once you have satisfied all the dependencies, you can compile with `cmake`.

```
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
# make install
```

**Note**: `v8` takes **A LOT** of time to compile, so if you want to disable JavaScript integration,
pass `-DWITH_V8=OFF` to `cmake`. This will disable all the features that depend on the JS integration,
like syntax highlighting.

The build output is in the `build` folder.

### Running tests

To run the tests, go to the `buid` directory and invoke the `check` target:

```
cd build
make check
```

### Common build options

There are some common options to pass to `cmake` to customize the build:

- **Debug build**: `-DCMAKE_BUILD_TYPE=Debug`
- **Disable V8**: `-DWITH_V8=OFF`

## Documentation

There's some documentation available for the JavaScript APIs at https://95ulisse.github.io/hedit.

To build the docs locally, you need [Node](https://nodejs.org) installed, then issue:

```
$ cd build
$ make docs
```

The docs will be in `docs/out`.

Template by [NHN Entertainment Corp](https://github.com/nhnent/tui.jsdoc-template).

## Roadmap

- [x] Undo/Redo
- [x] Scriptability
- [x] Write JS docs
- [x] Syntax highlighting
- [ ] Improve performance of syntax highlighting
- [ ] Structure view