## HEdit JavaScript API

This is the home of the documentation for the JavaScript APIs available to script HEdit.

HEdit configuration files are normal JavaScript modules that interact with the editor by importing
some builtin module. All canfiguration files will begin by importing one of those modules:

```js
// For a list of all the available modules, look at the index of the documentation.
import hedit from 'hedit';
```

After that, do whatever you want, it's just JS! You have the power of a full programming language
to describe complex and interactive configurations, as well as APIs to modify the open files programmatically
and interact with the editor.

For an example of those APIs, check out one of the available examples on the left.

### Config file locations

HEdit during startup time tries to load configuration files from the following locations:

- `/etc/heditrc.js`
- `~/.heditrc`
