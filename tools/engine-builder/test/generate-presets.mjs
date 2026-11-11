// Checks the engine builder's generator.
//
//   node tools/engine-builder/test/generate-presets.mjs [--out <dir>]
//       [--probe <boost-probe>] [--export <engine-sim-export>] [--random <n>]
//
// Always: every preset passes validation, survives saving and reopening, and
// gets an evenly spaced firing order with matching crank pins.
// With --probe: each generated engine starts and makes positive torque across
// its rev range. With --export: its impulse responses load and it idles.
// --random adds that many random variations of the presets to the run.

import { execFile } from 'node:child_process';
import { mkdirSync, writeFileSync } from 'node:fs';
import { cpus, tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { promisify } from 'node:util';

import builder from '../generator.js';

const { LAYOUTS, PRESETS, SECTIONS, crankLayout, generate, readSavedValues, validate } = builder;

const run = promisify(execFile);

const args = process.argv.slice(2);
const option = name => {
    const i = args.indexOf(name);
    return i >= 0 ? args[i + 1] : undefined;
};

const outDir = resolve(option('--out') || join(tmpdir(), 'engine-builder-test'));
const probe = option('--probe');
const exporter = option('--export');
const randomCount = Number(option('--random') || 0);

let failures = 0;
function fail(message) {
    ++failures;
    console.log(`FAIL ${message}`);
}

function warn(message) {
    console.log(`WARN ${message}`);
}

// Crank layouts
for (const [id, layout] of Object.entries(LAYOUTS)) {
    const cylinders = crankLayout(id);
    const firing = cylinders.map(c => c.firing).sort((a, b) => a - b);
    const interval = 720 / cylinders.length;
    firing.forEach((f, i) => {
        if (Math.abs(f - i * interval) > 1e-9) fail(`${id}: uneven firing at ${f}`);
    });

    for (const c of cylinders) {
        const expected = ((c.firing + layout.banks[c.bank]) % 360 + 360) % 360;
        if (Math.abs(c.journal - expected) > 1e-9) fail(`${id}: cylinder ${c.number} journal ${c.journal}`);
    }

    if (new Set(layout.firing).size !== layout.bank.length) fail(`${id}: firing order doesn't list every cylinder once`);
}

// Random variations stay inside the form's ranges
function seededRandom(seed) {
    let s = seed >>> 0;
    return () => {
        s = (s * 1664525 + 1013904223) >>> 0;
        return s / 4294967296;
    };
}

const engines = Object.entries(PRESETS).map(([id, preset]) => ({ id, values: preset.values }));
const random = seededRandom(1);
const presetList = Object.values(PRESETS);
for (let i = 0; i < randomCount; ++i) {
    const values = { ...presetList[i % presetList.length].values };
    const vary = (name, spread) => {
        const field = SECTIONS.flatMap(s => s.fields).find(f => f.id === name);
        const v = values[name] * (1 + (random() * 2 - 1) * spread);
        values[name] = Math.min(field.max, Math.max(field.min, Math.round(v * 10) / 10));
    };
    vary('bore', 0.1);
    vary('stroke', 0.1);
    vary('intakeDuration', 0.08);
    vary('exhaustDuration', 0.08);
    vary('intakeLift', 0.15);
    vary('runnerLength', 0.4);
    vary('primaryLength', 0.4);
    vary('flywheel', 0.4);
    values.rodLength = Math.max(values.rodLength, Math.round(1.7 * values.stroke));
    values.name = `Random ${i + 1}`;
    engines.push({ id: `random_${i + 1}`, values, random: true });
}

mkdirSync(outDir, { recursive: true });

for (const engine of engines) {
    const errors = validate(engine.values);
    if (errors.length > 0) {
        fail(`${engine.id}: ${errors.map(e => e.message).join(' ')}`);
        continue;
    }

    engine.script = generate(engine.values);
    const reopened = readSavedValues(engine.script);
    if (JSON.stringify(reopened) !== JSON.stringify(engine.values)) fail(`${engine.id}: values don't survive reopening`);

    engine.file = join(outDir, `${engine.id}.mr`);
    writeFileSync(engine.file, engine.script);

    // boost-probe needs a script that sets the engine
    engine.main = join(outDir, `${engine.id}_main.mr`);
    writeFileSync(engine.main,
        `import "engine_sim.mr"\nimport "${engine.file.replace(/\\/g, '/')}"\n\nmain()\n`);
}

async function check(engine) {
    const redline = engine.values.redline;
    const rpms = [0.3, 0.55, 0.8, 0.95].map(f => Math.round(redline * f / 100) * 100);
    const notes = [];

    if (probe) {
        try {
            const { stdout } = await run(probe, [engine.main, ...rpms.map(String)], { timeout: 600000 });
            const started = /started: (\d+) rpm/.exec(stdout);
            const torque = rpms.map(rpm => {
                const row = stdout.split(/\r?\n/).map(l => l.trim().split(/\s+/))
                    .filter(c => Number(c[0]) === rpm && Number(c[1]) === 6.0);
                return row.length > 0 ? Number(row[row.length - 1][6]) : NaN;
            });

            if (!started) fail(`${engine.id}: didn't start`);
            torque.forEach((t, i) => {
                if (!(t > 0)) fail(`${engine.id}: ${t} lb-ft at ${rpms[i]} rpm`);
            });

            notes.push(`started ${started ? started[1] : '-'} rpm`);
            notes.push(rpms.map((rpm, i) => `${rpm}: ${Math.round(torque[i])} lb-ft`).join(', '));
        }
        catch (e) {
            fail(`${engine.id}: boost-probe failed: ${(e.stderr || e.message).toString().split('\n')[0]}`);
        }
    }

    if (exporter) {
        let stdout = '';
        try {
            ({ stdout } = await run(exporter, [
                '--script', engine.file, '--out', join(outDir, 'export'),
                '--clips', 'idle', '--formats', 'wav', '--jobs', '1'
            ], { timeout: 600000 }));
        }
        catch (e) {
            // A clip that fails (e.g. the engine stalls) makes the exit code 1
            stdout = (e.stdout || '').toString();
            if (!/\bidle\b/.test(stdout)) {
                fail(`${engine.id}: engine-sim-export failed: ${(stdout || e.message).split('\n').filter(l => /error/i.test(l)).join(' ')}`);
            }
        }

        const idle = /idle\s+[\d.]+ s audio, (\d+) rpm/.exec(stdout);
        const rpm = idle ? Number(idle[1]) : NaN;
        if (!(rpm >= 400 && rpm <= 2000)) {
            // Random variations may need their idle opening adjusted, as a
            // user would; the presets must idle as they are
            (engine.random ? warn : fail)(`${engine.id}: ${idle ? `idles at ${rpm} rpm` : 'stalls at idle'}`);
        }
        notes.push(idle ? `idle ${rpm} rpm` : 'stalls at idle');
    }

    console.log(`${engine.id.padEnd(12)} ${notes.join(' | ')}`);
}

const pending = engines.filter(e => e.script);
const workers = Array.from({ length: Math.max(1, Math.min(cpus().length / 2, 8)) }, async () => {
    while (pending.length > 0) await check(pending.shift());
});
await Promise.all(workers);

console.log(failures === 0
    ? `OK: ${engines.length} engines in ${outDir}`
    : `${failures} failure(s); scripts are in ${outDir}`);
process.exit(failures === 0 ? 0 : 1);
