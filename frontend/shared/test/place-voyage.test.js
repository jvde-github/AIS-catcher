import {test} from 'node:test';
import assert from 'node:assert/strict';
import {getChangeListHTML} from '../core/spark.js';
import {CHANGE} from '../core/text.js';

test('place visits remain separate timeline events and names are escaped', () => {
    const html = getChangeListHTML([
        {f:CHANGE.PLACE_ENTER,t:100,to:'Port <one>'},
        {f:CHANGE.PLACE_EXIT,t:120,to:'Port <one>'},
        {f:CHANGE.PLACE_ENTER,t:140,to:'Berth & two'}
    ], [CHANGE.PLACE_ENTER,CHANGE.PLACE_EXIT]);
    assert.equal((html.match(/tl-item/g)||[]).length,3);
    assert(html.includes('Entered place'));
    assert(html.includes('Left place'));
    assert(html.includes('Port &lt;one&gt;'));
    assert(html.includes('Berth &amp; two'));
    assert(!html.includes('tl-was'));
    assert(!html.includes('first seen'));
});

import {currentPlaceIds, visitListHTML} from '../visits.js';

test('packed visits decode full uint32 IDs and filter completed duplicates', () => {
    assert.deepEqual(currentPlaceIds([0, 1, 1, 2, 3, 8589934589]), [0, 1, 4294967294]);
    assert.deepEqual(currentPlaceIds([-1, NaN, 1.5, 2]), []);
});

test('visit list keeps repeat visits, escapes names and shows unknown times honestly', () => {
    const html = visitListHTML([
        {name:'Port <one>', entered:100, exited:220, inside:false},
        {name:'Port <one>', entered:300, exited:null, inside:true},
        {name:'Berth & two', entered:null, exited:null, inside:true, pending:true}
    ], 3960);
    assert.equal((html.match(/class="tl-item place-visit"/g) || []).length, 3);
    assert(html.includes('Port &lt;one&gt;'));
    assert(html.includes('Berth &amp; two'));
    assert(html.includes('2 min'));
    assert(html.includes('1 h 1 min'));
    assert(html.includes('Confirming arrival'));
    assert(html.includes('visit-dot-inside'));
    assert(html.includes('visit-dot-completed'));
    assert(html.includes('visit-dot-pending'));
    assert(html.includes('<span>Entry</span><span>Confirming…</span>'));
    assert(html.includes('<span>Duration</span><span>—</span>'));
    assert(visitListHTML([]).includes('No recorded visits'));
    assert(visitListHTML([{name:'Quay', entered:100, exited:112, inside:false}], 120).includes('12 sec'));
});


test('visit durations use server time and gaps keep an unknown exit', () => {
    const visits = [
        {name:'Port', entered:100, exited:null, inside:false},
        {name:'Port', entered:null, exited:null, inside:true}
    ];
    const html = visitListHTML(visits, 10000);
    assert(html.includes('Observation ended; exit unknown'));
    assert.equal((html.match(/<span>Duration<\/span><span>Unknown<\/span>/g) || []).length, 2);
    assert(visitListHTML([{name:'Port',entered:100,inside:true}], 160).includes('1 min'));
    assert(visitListHTML([{name:'Port',entered:100,inside:true}]).includes('<span>Duration</span><span>Unknown</span>'));
    assert(visitListHTML([{name:'Port',entered:100,inside:false,pending:true}], 160).includes('<span>Duration</span><span>—</span>'));
});

 test('visits sort by entry time, retain unknown entries and exclude duplicate crossing events', async () => {
    const {changeList} = await import('../shipcard.js');
    const html = visitListHTML([
        {name:'Unknown baseline',inside:true},
        {name:'Earlier active',entered:100,inside:true},
        {name:'Latest complete',entered:200,exited:300,inside:false}
    ], 400);
    assert(html.indexOf('Latest complete') < html.indexOf('Earlier active'));
    assert(html.indexOf('Earlier active') < html.indexOf('Unknown baseline'));
    assert.equal(changeList([{f:CHANGE.PLACE_ENTER,t:100,to:'Port'}]), '');
});
