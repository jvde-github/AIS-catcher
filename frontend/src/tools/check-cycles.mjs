// node check-cycles.mjs DIR...: fails when the static imports among a folder's modules form a cycle

import fs from 'node:fs';
import path from 'node:path';

let failed = false;
for (const dir of process.argv.slice(2).map((d) => path.resolve(d))) {
    const files = [];
    (function walk(d) {
        for (const e of fs.readdirSync(d, { withFileTypes: true })) {
            const p = path.join(d, e.name);
            if (e.isDirectory()) { if (e.name !== 'node_modules' && e.name !== 'dist') walk(p); }
            else if (p.endsWith('.js')) files.push(p);
        }
    })(dir);
    const imports = new Map(files.map((f) => [f, [...fs.readFileSync(f, 'utf8').matchAll(/^\s*(?:import|export)\s+(?:[^'"]*?\sfrom\s+)?['"](\.{1,2}\/[^'"]+)['"]/gm)]
        .map((m) => path.resolve(path.dirname(f), m[1])).filter((t) => t !== f && files.includes(t))]));

    // Tarjan's strongly connected components: any of more than one module is a cycle
    let next = 0;
    const index = new Map(), low = new Map(), stack = [], onStack = new Set();
    const visit = (v) => {
        index.set(v, next); low.set(v, next++); stack.push(v); onStack.add(v);
        for (const w of imports.get(v)) {
            if (!index.has(w)) { visit(w); low.set(v, Math.min(low.get(v), low.get(w))); }
            else if (onStack.has(w)) low.set(v, Math.min(low.get(v), index.get(w)));
        }
        if (low.get(v) !== index.get(v)) return;
        const cycle = [];
        let w;
        do { w = stack.pop(); onStack.delete(w); cycle.push(path.relative(process.cwd(), w)); } while (w !== v);
        if (cycle.length > 1) {
            failed = true;
            console.error('import cycle: ' + cycle.sort().join(', '));
        }
    };
    for (const f of files) if (!index.has(f)) visit(f);
}
process.exit(failed ? 1 : 0);
