const imagePlugin = require('esbuild-plugin-inline-image');
const postCssPlugin = require('esbuild-style-plugin');
const tailwind = require('tailwindcss');
const postCssColorFunctionalNotation = require('postcss-color-functional-notation');

/** @type { import('@synaptic-simulations/mach').MachConfig } */
module.exports = {
    packageName: 'A339X',
    packageDir: 'out/headwindsim-aircraft-a330-900',
    plugins: [
        imagePlugin({ limit: -1 }),
        postCssPlugin({
            extract: true,
            postcss: {
                plugins: [
                    tailwind('src/systems/instruments/src/EFB/tailwind.config.js'),
                    postCssColorFunctionalNotation(),
                ],
            }
        }),
    ],
    instruments: [
        msfsAvionicsInstrument('PFD'),
        msfsAvionicsInstrument('EWD'),
        msfsAvionicsInstrument('Clock'),

        reactInstrument('ND', ['/JS/A339X/A32NX_Util.js']),
        reactInstrument('SD'),
        reactInstrument('DCDU'),
        reactInstrument('RTPI'),
        reactInstrument('RMP'),
        reactInstrument('ISIS'),
        reactInstrument('BAT'),
        reactInstrument('ATC'),
        reactInstrument('EFB', ['/Pages/VCockpit/Instruments/Shared/Map/MapInstrument.html']),
    ],
};

function msfsAvionicsInstrument(name) {
    return {
        name,
        index: `src/systems/instruments/src/${name}/instrument.tsx`,
        simulatorPackage: {
            type: 'baseInstrument',
            templateId: `A339X_${name}`,
            mountElementId: `${name}_CONTENT`,
            fileName: name.toLowerCase(),
            imports: ['/JS/dataStorage.js','/JS/A339X/A32NX_Simvars.js'],
        },
    };
}

function reactInstrument(name, additionalImports) {
    return {
        name,
        index: `src/systems/instruments/src/${name}/index.tsx`,
        simulatorPackage: {
            type: 'react',
            isInteractive: false,
            fileName: name.toLowerCase(),
            imports: ['/JS/dataStorage.js','/JS/A339X/A32NX_Simvars.js', ...(additionalImports ?? [])],
        },
    };
}