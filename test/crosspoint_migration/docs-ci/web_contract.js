// Executes the actual shipped scripts with a tiny DOM/transport seam, not a UI clone.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const path = require('path');
const root = path.resolve(__dirname, '../../..');
const elements = new Map();
function el(id) {
  if (!elements.has(id)) elements.set(id, {value:'', checked:false, dataset:{}, style:{}, disabled:false,
    addEventListener(){}, replaceChildren(){}, appendChild(){}, innerHTML:''});
  return elements.get(id);
}
const calls = [];
const ctx = vm.createContext({console, Set, Number, FormData: class {},
  document:{getElementById:el, createElement:()=>el('temporary')},
  fetch:async(url, options)=>{
    calls.push({url, options});
    return {ok:true, json:async()=>[], text:async()=>''};
  }, showWebToast(){}, showWebConfirm:async()=>true});
vm.runInContext(fs.readFileSync(path.join(root,'web/pages/settings.js'),'utf8'), ctx);
(async()=>{
  // Wait for sequential load; loading never posts or changes settings.
  for(let i=0;i<12;i++) await Promise.resolve();
  assert(!calls.some(c=>c.options?.method==='POST'));
  const html=vm.runInContext(`renderControl({key:'action',type:'enum',value:null,options:['A','B']})`,ctx);
  assert(html.includes('value="" disabled selected'), 'unknown saved enum must not silently become first option');
  vm.runInContext(`allSettings=[{key:'sleepScreen',type:'enum',value:6,options:['A','B','C','D','E','F','G']}]`,ctx);
  el('setting-sleepScreen').value='6';
  assert.strictEqual(vm.runInContext('isQuickResumeSleepScreenSelected()',ctx),false, 'wire index is not raw QUICK_RESUME enum');
  const opds=vm.runInContext(`renderOpdsServer({name:'Test',headers:[{name:'Authorization',hasValue:true}]},0)`,ctx);
  assert(opds.includes('opds-header0-name-0'));
  assert(opds.includes('opds-filename-0') && opds.includes('disabled'), 'unsupported filename API must be explicit');
  el('setting-action').value='';
  assert.strictEqual(vm.runInContext(`getValue({key:'action',type:'enum'})`,ctx),undefined);
  el('setting-wide').value='500';
  assert.strictEqual(vm.runInContext(`getValue({key:'wide',type:'value'})`,ctx),500);
  vm.runInContext(fs.readFileSync(path.join(root,'web/pages/fonts.js'),'utf8'),ctx);
  assert.strictEqual(vm.runInContext(`familyFromFilename('Bookerly-Bold.otf')`,ctx),'Bookerly');
  assert.strictEqual(vm.runInContext(`fontExtRe().test('family.ttc')`,ctx),false,'SD TTC support is not web upload support');
  console.log('PASS actual settings/fonts scripts: enum-index/null, wide value, OPDS headers/capability, read-only load, font upload contract');
})().catch(e=>{console.error(e);process.exitCode=1;});
