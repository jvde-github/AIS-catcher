import test from 'node:test';
import assert from 'node:assert/strict';
import {createConfigUpdater} from '../server-config.js';
const cfg = (v, plugin='p') => ({config_version:v,plugin_version:plugin,station:v});
test('configuration is fetched only on change and plugin changes notify once',async()=>{
    const current=cfg('a');let calls=0,notices=0,applied=0;
    const updater=createConfigUpdater({current,fetchConfig:async()=>{calls++;return cfg('b','q');},
        apply:()=>applied++,pluginsChanged:()=>notices++});
    await updater.update('a');assert.equal(calls,0);
    await updater.update('b');await updater.update('b');
    assert.equal(calls,1);assert.equal(applied,1);assert.equal(notices,1);assert.equal(current.station,'b');
});
test('failed or mismatched replies retain the last working version and retry later',async()=>{
    const current=cfg('a');let count=0;
    const updater=createConfigUpdater({current,fetchConfig:async()=>{
        count++;if(count===1)throw Error('offline');return cfg(count===2?'old':'b');
    },apply:()=>{},pluginsChanged:()=>{}});
    await updater.update('b');assert.equal(current.config_version,'a');
    await updater.update('b');assert.equal(current.config_version,'a');
    await updater.update('b');assert.equal(current.config_version,'b');
});
test('an obsolete in-flight response cannot replace newer desired settings',async()=>{
    let finish;const current=cfg('a');const applied=[];
    const updater=createConfigUpdater({current,fetchConfig:()=>new Promise(r=>finish=r),
        apply:next=>applied.push(next.config_version),pluginsChanged:()=>{}});
    const first=updater.update('b');updater.update('c');finish(cfg('b'));
    await new Promise(r=>setImmediate(r));finish(cfg('c'));await first;
    assert.deepEqual(applied,['c']);assert.equal(current.config_version,'c');
});

test('bootstrap detects plugins changed between the code and JSON requests',async()=>{
    let notices=0;
    const current=cfg('b','new');
    const updater=createConfigUpdater({current,loadedPluginVersion:'old',
        fetchConfig:()=>{throw Error('must not fetch');},apply:()=>{},pluginsChanged:()=>notices++});
    await updater.update('b');await updater.update('b');
    assert.equal(notices,1);
});

test('server overlays are above base maps, below ships, and unchanged layers are retained',async()=>{
    const {createServerMaps}=await import('../../src/features/server-maps.js');
    const base={}, ships={}, array=[base,ships], bases={Default:base}, overlays={};
    const map={getLayers:()=>({getArray:()=>array,insertAt:(i,l)=>array.splice(i,0,l)}),
        removeLayer:l=>array.splice(array.indexOf(l),1)};
    const ol={layer:{Tile:class{constructor(options){Object.assign(this,options);}}},
        source:{XYZ:class{constructor(options){Object.assign(this,options);}}}};
    const apply=createServerMaps({ol,basemaps:()=>bases,overlays:()=>overlays,getMap:()=>map,refresh:()=>{}});
    const source={id:'a',name:'Overlay',overlay:true,url:'tiles/a/{z}/{x}/{y}',minZoom:0,maxZoom:18};
    apply([source]);
    const layer=overlays.Overlay;
    assert.deepEqual(array,[base,layer,ships]);
    apply([source]);assert.equal(overlays.Overlay,layer);assert.equal(array.length,3);
});
