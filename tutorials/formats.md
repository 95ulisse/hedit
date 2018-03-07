HEdit allows you to define custom file formats to support syntax highlighting.

For other example file formats, see one of the files in
[src/js/format](https://github.com/95ulisse/hedit/tree/master/src/js/format).

* **Groups usage**
  ```
  new Format()
      .group('Group 1')
          .uint8('A byte')
          .group('Group 2')
              .uint8('Another byte')
          .endgroup()
          .uint8('A final byte')
      .endgroup();
  ```

  This produces spans with the following names:
  ```
  'Group 1 > A byte'
  'Group 1 > Group 2 > Another byte'
  'Group 1 > A final byte'
  ```

* **Nested formats**
  ```
  const c = new Format().uint('Nested byte');
  new Format()
      .child('Parent', c);
  ```

  This produces spans with the following names:
  ```
  'Parent > Nested byte'
  ```

* **Combinators**
  ```
  const c = new Format().uint8('Nested byte');
  new Format()
      .array('Array of bytes', 3, c);
  ```

  This produces spans with the following names:
  ```
  'Array of bytes > Nested byte'
  'Array of bytes > Nested byte'
  'Array of bytes > Nested byte'
  ```

* **Pascal-like string**
  ```
  new Format()
      .uint8('String length', 'orange', 'len')
      .array('String contents', 'len', 'blue');
  ```
