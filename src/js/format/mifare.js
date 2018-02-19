// This is a common format for all the Mifare Classic family file formats: 1K, 2K, 4K.

function range(from, to) {
    return Array.from({ length: to - from + 1 }, (_, i) => i + from);
}

function makeDataBlock(n) {
    return {
        description: 'Block #' + n,
        color: 'white',
        length: 16
    };
}

function makeAppDirectoryBlock(n) {
    return {
        description: 'Block #' + n + ' (Mifare application directory)',
        color: 'blue',
        length: 16
    };
}

function makeManufacturerBlock(n) {
    return {
        description: 'Block #' + n,
        children: [
            { description: 'UID', color: 'red', length: 4 },
            { description: 'Manufacturer data', color: 'pink', length: 12 }
        ],
    };
}

function makeTrailerBlock(n) {
    return {
        description: 'Block #' + n,
        children: [
            { description: 'Key A',           color: 'green',   length: 6 },
            { description: 'Access bits',     color: 'purple',  length: 3 },
            { description: 'General purpose', color: 'white',   length: 1 },
            { description: 'Key B',           color: 'orange',  length: 6 }
        ]
    };
}

function makeSector(hasManufacturerBlock, hasAppDirectoryBlock, n) {
    return {
        description: 'Sector #' + n,
        children: [
            hasManufacturerBlock ? makeManufacturerBlock(n * 4) : (hasAppDirectoryBlock ? makeAppDirectoryBlock(n * 4) :  makeDataBlock(n * 4)),
            hasAppDirectoryBlock ? makeAppDirectoryBlock(n * 4 + 1) : makeDataBlock(n * 4 + 1),
            hasAppDirectoryBlock ? makeAppDirectoryBlock(n * 4 + 2) : makeDataBlock(n * 4 + 2),
            makeTrailerBlock(n * 4 + 3)
        ]
    };
}

function makeLargeSector(n) {
    const firstSector = (n - 32) * 16 + 128;
    return {
        description: 'Sector #' + n,
        children: [
            ...range(0, 14).map(i => makeSector(false, false, firstSector + i)),
            makeTrailerBlock(firstSector + 15)
        ]
    };
}

export default [
    makeSector(true, true, 0),
    ...range(1, 15).map(makeSector.bind(null, false, false)),
    makeSector(false, true, 16),
    ...range(17, 31).map(makeSector.bind(null, false, false)),
    ...range(32, 39).map(makeLargeSector)
];
