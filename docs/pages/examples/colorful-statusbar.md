Using themes and events, we can have our statusbar change color depending on the current mode we are in.

```js
// Import the main hedit builtin module
import hedit from 'hedit';

// Change the color of the statusbar depending on the current mode.
// Colors can be both hex and standard ANSI numbers.
const map = {
    normal: 247,       // The default gray-ish color
    insert: '0066cc',  // A light blue
    replace: 'ff875f'  // A soft orange
};
hedit.on('modeSwitch', (mode) => {
    hedit.setTheme({
        statusbar: { bg: map[mode] || map.normal }
    });
});
```

Save the previous script to `~/.heditrc` and restart HEdit.
