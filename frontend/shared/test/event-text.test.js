import { test } from 'node:test';
import assert from 'node:assert/strict';
import { renderEventText } from '../core/event-text.js';

test('event text: names, muted previous values and blue new values', () => {
    assert.deepEqual(renderEventText('**EXAMPLE** · ::destination DEHAM:: → [[ANTWERP]]'), {
        text: 'EXAMPLE · destination DEHAM → ANTWERP',
        html: '<span class="tk-name">EXAMPLE</span> · <span class="tk-label">destination DEHAM</span> → <span class="tk-to">ANTWERP</span>',
    });
});

test('event text: escaped received delimiters stay literal inside styles', () => {
    const result = renderEventText(String.raw`**A\*\*B** · [[\[\[PORT\]\] \:\: \\]]`);
    assert.equal(result.text, 'A**B · [[PORT]] :: \\');
    assert.equal(result.html, '<span class="tk-name">A**B</span> · <span class="tk-to">[[PORT]] :: \\</span>');
});

test('event text: HTML is inert in both styled and plain text', () => {
    const result = renderEventText('**<img src=x onerror="bad()">** & <script>bad()</script>');
    assert.ok(!result.html.includes('<img'));
    assert.ok(!result.html.includes('<script'));
    assert.ok(result.html.includes('&lt;img'));
    assert.ok(result.html.includes('&amp;'));
    assert.equal(result.text, '<img src=x onerror="bad()"> & <script>bad()</script>');
});

test('event text: unmatched markers and trailing backslash stay readable', () => {
    assert.deepEqual(renderEventText('**unfinished [[value \\'), { text: '**unfinished [[value \\', html: '**unfinished [[value \\' });
    assert.deepEqual(renderEventText(''), { text: '', html: '' });
});
