import {test} from 'node:test';
import assert from 'node:assert/strict';
import {needsMapObjects} from '../object-visibility.js';
import {setupDom} from './dom.js';
import {create} from '../mapobjects.js';

test('place hover leaves the API prefix to the host transport', async () => {
    setupDom();
    const requests = [];
    const objects = create({
        options: () => ({display:'off', places:true}),
        shipLabel: () => '', shipLink: () => '', isHovered: () => true,
        fetchJSON: async query => {
            requests.push(new URL('api/' + query, 'http://localhost/viewer/').pathname);
            return {type:'Feature', properties:{}, geometry:{type:'Polygon',
                coordinates:[[[4,52],[5,52],[5,53],[4,52]]]}};
        }
    });
    try {
        objects.applyDelta({place_version:'v1', objects:[{id:'place-0', kind:10,
            runtime_id:0, uuid:'example', label:'Port', place_type:'port', code:'NLRTM',
            lat:52, lon:4, has_geometry:true}]}, true);
        objects.redraw();
        const marker = objects.vector.getFeatureById('mo-place-0');
        objects.tooltip(marker);
        await new Promise(resolve => setTimeout(resolve, 0));
        assert.deepEqual(requests, ['/viewer/api/place.json']);
        assert(objects.vector.getFeatures().some(f => f.getGeometry().getType() === 'Polygon'));
        objects.tooltip(marker);
        assert.equal(requests.length, 1, 'successful hover reuses cached geometry');
    } finally {
        objects.clear();
    }
});

test('places and ports independently keep the object feed active',()=>{
    assert(needsMapObjects({show_ports:false,show_places:true},false));
    assert(needsMapObjects({show_ports:true,show_places:false},false));
    assert(!needsMapObjects({show_ports:false,show_places:false},false));
    assert(needsMapObjects({show_ports:false,show_places:false},true));
});

test('berths stay accessible without cluttering the map with markers', async () => {
    setupDom();
    const requests = [];
    let focus;
    const objects = create({
        options: () => ({display:'all', places:true, focus}),
        shipLabel: () => '', shipLink: () => '', openVessel() {},
        fetchJSON: async query => { requests.push(query); return {ships:[], total:0}; },
    });
    const terminal = {id:'place-0',kind:10,runtime_id:0,uuid:'terminal',label:'Euromax',place_type:'terminal',lat:52,lon:4,has_geometry:true};
    const berth = {...terminal,id:'place-1',runtime_id:1,uuid:'berth',label:'Quay 1',place_type:'berth',lon:4.1};
    objects.applyDelta({place_version:'v1',objects:[terminal,berth]},true);
    objects.redraw();
    assert(objects.vector.getFeatureById('mo-place-0'));
    assert.equal(objects.vector.getFeatureById('mo-place-1'),null);
    // The ship's visit supplies identity even when its marker was never loaded.
    objects.clear();
    for (const place of [terminal,berth]) {
        objects.openPlace({...place,place_version:'v1'});
        await new Promise(resolve => setTimeout(resolve, 0));
        assert.equal(document.querySelector('.side-table-label').textContent, place.place_type === 'terminal' ? 'Terminal' : 'Berth');
        assert.equal(document.querySelector('#port-ships-title').textContent, place.label);
        assert.equal(document.querySelector('[data-place-tab=expected]'), null);
        assert(requests.at(-1).includes('id=' + place.runtime_id + '&version=v1'));
    }
    focus = berth;
    objects.redraw();
    assert(objects.vector.getFeatureById('mo-place-1'), 'explicitly opening a berth can still show its position');
    objects.clear();
});
test('place object deltas retain unchanged geometry and remove deleted places',()=>{
    setupDom();
    const collections=[];
    const objects=create({options:()=>({places:true,hidden:()=>false}), onPlacesChanged:c=>collections.push(c), shipLabel:()=>'',shipLink:()=>''});
    const row={id:'place-2',kind:10,runtime_id:2,uuid:'example',label:'Port',place_type:'port',lat:52,lon:4,has_geometry:true};
    objects.applyDelta({objects:[row],removed:[],place_version:"generation-one"},true);
    assert.equal(collections.length,1);assert.equal(collections[0].features[0].runtime_id,2);
    assert.equal(collections[0].place_version,"generation-one");
    objects.applyDelta({objects:[],removed:[]},false);assert.equal(collections.length,1);
    objects.applyDelta({objects:[{...row,label:'Renamed'}],removed:[]},false);
    assert.equal(collections.at(-1).features[0].properties.name,'Renamed');
    objects.applyDelta({objects:[],removed:[row.id]},false);
    assert.equal(collections.at(-1).features.length,0);
    objects.applyDelta({objects:[row]},false);objects.applyDelta({objects:[]},true);
    assert.equal(collections.at(-1).features.length,0);
});

test('overlapping station and port retain separate clickable pill slots',()=>{
    setupDom();
    const opened=[];
    const objects=create({options:()=>({display:'all',hidden:()=>false}),shipLabel:()=>'',shipLink:()=>'',openStation:id=>opened.push(id)});
    objects.applyDelta({time:100,objects:[
        {id:'pNLRTM',kind:9,lat:52,lon:4,label:'Rotterdam',fields:{code:'NLRTM',country:'NL'}},
        {id:'s12',kind:8,lat:52,lon:4,label:'Receiver',t:100,flags:0}
    ]},true);
    objects.redraw();
    const station=objects.vector.getFeatureById('mo-s12'),port=objects.vector.getFeatureById('mo-pNLRTM');
    assert(station && port);assert.equal(station.pill_count,2);assert.equal(port.pill_count,2);
    assert.notEqual(station.pill_index,port.pill_index);
    assert(objects.click(station));assert.deepEqual(opened,[12]);
});


test('sections appear from zoom 13 when reusing overview tiles', () => {
    setupDom();
    let focus;
    const objects = create({options:() => ({display:'all',places:true,focus}), shipLabel:() => '',shipLink:() => ''});
    const section = {id:'section',kind:10,runtime_id:1,uuid:'section',label:'Hansadok',place_type:'section',lat:51.26,lon:4.34,z:7};
    const port = {...section,id:'port',runtime_id:0,uuid:'port',label:'Antwerpen',place_type:'port',lon:4.4};
    const later = {...section,id:'later',runtime_id:2,uuid:'later',label:'Later section',lon:4.5,z:15};
    objects.applyTile('9/1/1',{place_version:'v1',objects:[port,section,later]});
    for (const zoom of [7,12,12.9,13,15,12]) {
        objects.setViewZoom(zoom); objects.redraw();
        assert(objects.vector.getFeatureById('mo-port'));
        assert.equal(!!objects.vector.getFeatureById('mo-section'), zoom >= 13);
        assert.equal(!!objects.vector.getFeatureById('mo-later'), zoom >= 15);
    }
    focus = section;
    objects.redraw();
    assert(objects.vector.getFeatureById('mo-section'), 'a section explicitly opened by the user remains visible');
    objects.clear();
});
