// The engine builder page: builds the form from EngineBuilder.SECTIONS and
// saves, checks and reopens engines. Written to work with screen readers:
// every field has a label, its help text and range are tied to it, problems
// are listed in one place with links to the fields, and results are announced
// in a status region.
(function () {
    'use strict';

    const B = window.EngineBuilder;
    const fields = B.SECTIONS.flatMap(section => section.fields);

    const form = document.getElementById('engine-form');
    const statusRegion = document.getElementById('status');
    const errorBox = document.getElementById('errors');
    const scriptView = document.getElementById('script');

    function announce(text) {
        // Clear first so the same message is announced again
        statusRegion.textContent = '';
        window.setTimeout(() => { statusRegion.textContent = text; }, 50);
    }

    function element(tag, attributes, text) {
        const e = document.createElement(tag);
        for (const [name, value] of Object.entries(attributes || {})) e.setAttribute(name, value);
        if (text !== undefined) e.textContent = text;
        return e;
    }

    function labelText(field) {
        return field.unit ? `${field.label} (${field.unit})` : field.label;
    }

    function buildForm() {
        for (const section of B.SECTIONS) {
            const fieldset = element('fieldset', { id: `section-${section.id}` });
            const legend = element('legend');
            legend.appendChild(element('h2', {}, section.title));
            fieldset.appendChild(legend);

            for (const field of section.fields) {
                const wrapper = element('div', { class: 'field' });
                const helpId = `${field.id}-help`;
                const errorId = `${field.id}-error`;

                wrapper.appendChild(element('label', { for: field.id }, labelText(field)));

                let input;
                if (field.type === 'select') {
                    input = element('select', { id: field.id, name: field.id });
                    for (const [value, text] of Object.entries(field.options())) {
                        input.appendChild(element('option', { value }, text));
                    }
                }
                else if (field.type === 'text') {
                    input = element('input', { id: field.id, name: field.id, type: 'text', autocomplete: 'off' });
                }
                else {
                    input = element('input', {
                        id: field.id, name: field.id, type: 'number', inputmode: 'decimal',
                        min: field.min, max: field.max, step: field.step
                    });
                }

                input.setAttribute('aria-describedby', helpId);
                wrapper.appendChild(input);

                let help = field.help || '';
                if (field.type !== 'select' && field.type !== 'text') {
                    help = `${help} Allowed: ${field.min} to ${field.max}.`.trim();
                }
                wrapper.appendChild(element('p', { id: helpId, class: 'help' }, help));
                wrapper.appendChild(element('p', { id: errorId, class: 'error', hidden: '' }));

                fieldset.appendChild(wrapper);
            }

            form.appendChild(fieldset);
        }
    }

    function readValues() {
        const values = {};
        for (const field of fields) {
            const input = document.getElementById(field.id);
            if (field.type === 'select' || field.type === 'text') {
                values[field.id] = input.value;
            }
            else {
                values[field.id] = (input.value.trim() === '') ? '' : Number(input.value);
            }
        }

        return values;
    }

    function writeValues(values) {
        for (const field of fields) {
            if (values[field.id] === undefined) continue;
            document.getElementById(field.id).value = String(values[field.id]);
        }

        clearErrors();
    }

    function clearErrors() {
        errorBox.hidden = true;
        errorBox.replaceChildren();
        for (const field of fields) {
            const input = document.getElementById(field.id);
            input.removeAttribute('aria-invalid');
            input.setAttribute('aria-describedby', `${field.id}-help`);
            const error = document.getElementById(`${field.id}-error`);
            error.hidden = true;
            error.textContent = '';
        }
    }

    function showErrors(errors) {
        clearErrors();

        const heading = element('h2', { id: 'errors-heading' },
            errors.length === 1 ? 'There is 1 problem to fix' : `There are ${errors.length} problems to fix`);
        const list = element('ul');
        const seen = new Set();
        for (const problem of errors) {
            const item = element('li');
            const link = element('a', { href: `#${problem.field}` }, problem.message);
            link.addEventListener('click', event => {
                event.preventDefault();
                document.getElementById(problem.field).focus();
            });
            item.appendChild(link);
            list.appendChild(item);

            if (seen.has(problem.field)) continue;
            seen.add(problem.field);

            const input = document.getElementById(problem.field);
            input.setAttribute('aria-invalid', 'true');
            input.setAttribute('aria-describedby', `${problem.field}-error ${problem.field}-help`);
            const error = document.getElementById(`${problem.field}-error`);
            error.textContent = problem.message;
            error.hidden = false;
        }

        errorBox.appendChild(heading);
        errorBox.appendChild(list);
        errorBox.hidden = false;
        errorBox.focus();
    }

    // Returns the script, or null after showing the problems
    function check() {
        const values = readValues();
        const errors = B.validate(values);
        if (errors.length > 0) {
            showErrors(errors);
            return null;
        }

        clearErrors();
        const script = B.generate(values);
        scriptView.value = script;
        return { values, script };
    }

    function save() {
        const result = check();
        if (result === null) return;

        const name = B.fileName(result.values);
        const blob = new Blob([result.script], { type: 'text/plain' });
        const link = element('a', { href: URL.createObjectURL(blob), download: name });
        document.body.appendChild(link);
        link.click();
        link.remove();
        window.setTimeout(() => URL.revokeObjectURL(link.href), 10000);

        announce(`Saved ${name}, usually in your Downloads folder. ${B.summary(result.values)}. ` +
            `In the simulator, press O. With speech on, it is in the engine list as "${result.values.name}, in Downloads"; ` +
            'without speech, open the file in the Open dialog.');
    }

    function open(file) {
        file.text().then(text => {
            const values = B.readSavedValues(text);
            if (values === null) {
                announce(`${file.name} was not made with this builder, so it can't be opened here. ` +
                    'It can be changed in a text editor instead; the accessibility guide explains how.');
                return;
            }

            writeValues(values);
            scriptView.value = text;
            announce(`Opened ${file.name}: ${B.summary(values)}.`);
        });
    }

    function usePreset() {
        const preset = B.PRESETS[document.getElementById('preset').value];
        writeValues(preset.values);
        scriptView.value = '';
        announce(`Filled in the ${preset.label} preset. Change any value, then save.`);
    }

    function initialize() {
        const presetSelect = document.getElementById('preset');
        for (const [id, preset] of Object.entries(B.PRESETS)) {
            presetSelect.appendChild(element('option', { value: id }, preset.label));
        }
        presetSelect.value = B.DEFAULT_PRESET;

        buildForm();
        writeValues(B.PRESETS[B.DEFAULT_PRESET].values);

        document.getElementById('use-preset').addEventListener('click', usePreset);
        document.getElementById('check').addEventListener('click', () => {
            const result = check();
            if (result !== null) {
                announce(`No problems found. ${B.summary(result.values)}. The engine script is under "Show the engine script".`);
            }
        });
        document.getElementById('open').addEventListener('change', event => {
            if (event.target.files.length > 0) open(event.target.files[0]);
            event.target.value = '';
        });
        form.addEventListener('submit', event => {
            event.preventDefault();
            save();
        });
    }

    initialize();
})();
