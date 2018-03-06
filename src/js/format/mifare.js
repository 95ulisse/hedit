// This is a common format for all the Mifare Classic family file formats: 1K, 2K, 4K.

import Format from 'hedit/format';



// Block constructors

const dataBlock = n =>
    new Format()
        .array('Block #' + n, 16);

const madBlock = n =>
    new Format()
        .array('Block #' + n + ' (MAD)', 16, 'blue');

const manufacturerBlock = n =>
    new Format()
        .group('Block #' + n)
            .uint32('UID', 'red')
            .array('Manufacturer data', 12, 'pink')
        .endgroup();

const trailerBlock = n =>
    new Format()
        .group('Block #' + n)
            .array('Key A', 6, 'green')
            .array('Access bits', 3, 'purple')
            .array('General purpose', 1)
            .array('Key B', 6, 'orange')
        .endgroup();



// Sector constructors

const sector = (manufacturer, mad, n) =>
    new Format()
        .group('Sector #' + n)
            .child(manufacturer ? manufacturerBlock(n * 4) : (
                            mad ? madBlock(n * 4) :
                                  dataBlock(n * 4)))
            .child(mad ? madBlock(n * 4 + 1) : dataBlock(n * 4 + 1))
            .child(mad ? madBlock(n * 4 + 2) : dataBlock(n * 4 + 2))
            .child(trailerBlock(n * 4 + 3))
        .endgroup();

const largeSector = n => {
    const firstSector = (n - 32) * 16 + 128;
    const f = new Format().group('Sector #' + n);
    for (let i = 0; i <= 14; i++) {
        f.child(sector(false, false, firstSector + i));
    }
    f.child(trailerBlock(firstSector + 15));
    return f.endgroup();
};



// Root format

const f = new Format();
f.child(sector(true, true, 0));
for (let i = 1; i <= 15; i++) {
    f.child(sector(false, false, i));
}
f.child(sector(false, true, 16));
for (let i = 17; i <= 31; i++) {
    f.child(sector(false, false, i));
}
for (let i = 32; i <= 39; i++) {
    f.child(largeSector(i));
}

export default f;
