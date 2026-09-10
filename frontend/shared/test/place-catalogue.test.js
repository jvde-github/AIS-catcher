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
