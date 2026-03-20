//import resolve from 'rollup-plugin-node-resolve';
import fs from 'fs';

const pkg = JSON.parse(fs.readFileSync('package.json', {encoding: 'utf8'}));
const banner = `/* uzip-module@${pkg.version}, license MIT */`;

export default [
  {
    input: 'src/UZIP.js',
    plugins: [
//      resolve({
//        modulesOnly: true,
//      }),
    ],
    output: [
      {
        format: 'umd',
        name: 'uzip',
        file: 'dist/uzip.js',
        indent: '  ',
        banner,
        exports: 'named',
      },
    ],
  },
];
