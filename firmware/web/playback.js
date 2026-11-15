// Shared playback appearance. Drafts stay independent of the status poll.
const playbackFields={
 lights:[],
 screen:[
  ['Elements',[
   ['show_status','Status icon','toggle'],['show_title','Title','toggle'],
   ['show_artist','Artist','toggle'],['show_progress','Progress bar','toggle'],
   ['time_display','Time display',[['elapsed_total','Elapsed / Total'],['remaining','Remaining'],['off','Off']]],
   ['long_text','Long text',[['ellipsis','Ellipsis'],['scroll','Scrolling']]]]],
  ['Playback state',[
   ['paused_screen','When paused',[['playback','Keep playback screen'],['clock','Clock'],['blank','Blank']]],
   ['stopped_screen','When stopped (nothing playing)',[['playback','Keep playback screen'],['clock','Clock'],['blank','Blank']]],
   ['disconnected_screen','When disconnected',[['connecting','Connecting icon'],['clock','Clock'],['blank','Blank']]]]]]
};
const playbackStates=['playing','paused','stopped'];
const playbackStateNames={playing:'Playing',paused:'Paused',stopped:'Stopped'};
let playbackLightState='playing';
for(const state of playbackStates){
 const prefix=state+'_';
 const ring=[
  ['ring_source','Lighting plan',[['scene','Use Scene'],['custom','Custom']]],
  ['ring_display','Display',state==='stopped'?[['lighting','Lighting'],['off','Off']]:[['progress','Playback progress'],['volume','Volume'],['lighting','Lighting'],['off','Off']]],
  ['ring_style','Progress fill',[['clockwise','Clockwise'],['bottom_to_top','Bottom to top']]],
  ['volume_ring_style','Volume feedback fill',[['clockwise','Clockwise'],['bottom_to_top','Bottom to top']]],
  ['ring_color_mode','Filled color',[['scene','Use Scene color'],['static','Static color'],['rainbow','Rainbow']]],
  ['ring_color','Filled color value','color'],['ring_brightness','Brightness','brightness'],
  ['unfilled_color_mode','Unfilled color',[['filled','Same as filled'],['static','Static color']]],
  ['unfilled_color','Unfilled color value','color'],['unfilled_brightness','Unfilled brightness','range']
 ];
 const keys=[['keys_source','Lighting plan',[['scene','Use Scene'],['custom','Custom']]],
  ['keys_mode','Lighting',[['static','Static color'],['rainbow','Rainbow'],['off','Off']]],
  ['keys_color','Color','color'],['keys_brightness','Brightness','brightness']];
 playbackFields.lights.push(['Ring',ring.map(([key,...rest])=>[prefix+key,...rest]),state,'ring'],['Buttons',keys.map(([key,...rest])=>[prefix+key,...rest]),state,'keys']);
}
function selectPlaybackState(state){
 if(!playbackStates.includes(state))return;Dropdowns.close();playbackLightState=state;
 for(const name of playbackStates){const tab=$('playback-state-tab-'+name),panel=$('playback-state-'+name);if(!tab||!panel)continue;tab.setAttribute('aria-selected',String(name===state));tab.tabIndex=name===state?0:-1;panel.hidden=name!==state;}
}
function playbackStateTabs(container){
 const tabs=document.createElement('div');tabs.className='playback-state-tabs';tabs.setAttribute('role','tablist');tabs.setAttribute('aria-label','Playback lighting states');container.append(tabs);
 for(const [i,state] of playbackStates.entries()){
  const tab=document.createElement('button');tab.type='button';tab.className='playback-state-tab';tab.id='playback-state-tab-'+state;tab.textContent=playbackStateNames[state];tab.setAttribute('role','tab');tab.setAttribute('aria-controls','playback-state-'+state);tabs.append(tab);
  tab.onclick=()=>selectPlaybackState(state);
  tab.onkeydown=e=>{let next=i;if(e.key==='ArrowRight')next=(i+1)%3;else if(e.key==='ArrowLeft')next=(i+2)%3;else if(e.key==='Home')next=0;else if(e.key==='End')next=2;else return;e.preventDefault();selectPlaybackState(playbackStates[next]);$('playback-state-tab-'+playbackStates[next]).focus()};
  const panel=document.createElement('div');panel.id='playback-state-'+state;panel.setAttribute('role','tabpanel');panel.setAttribute('aria-labelledby',tab.id);container.append(panel);
 }
}
const playbackDraft={},playbackDirty={lights:false,screen:false},playbackBusy={lights:false,screen:false};
let playbackInitialized=false;
function playbackNotice(panel,text,bad=false){message(text,bad,'playback-'+panel+'-message')}
function playbackChanged(panel){playbackDirty[panel]=true;playbackNotice(panel,'Unsaved changes');playbackVisibility(panel)}
function playbackVisibility(panel){
 const d=playbackDraft[panel];if(!d)return;
 if(panel!=='lights')return;
 for(const state of playbackStates){
  const p=state+'_',ring=d[p+'ring_source']==='custom',keys=d[p+'keys_source']==='custom',display=d[p+'ring_display'];
  const filled=ring&&display!=='off',progress=filled&&['progress','volume'].includes(display);
  const visibility={ring_display:ring,ring_style:progress&&display==='progress',volume_ring_style:ring,
   ring_color_mode:filled,ring_color:filled&&d[p+'ring_color_mode']==='static',ring_brightness:filled,
   unfilled_color_mode:progress,unfilled_color:progress&&d[p+'unfilled_color_mode']==='static',unfilled_brightness:progress,
   keys_mode:keys,keys_color:keys&&d[p+'keys_mode']==='static',keys_brightness:keys&&d[p+'keys_mode']!=='off'};
  for(const [key,visible] of Object.entries(visibility))$('playback-'+p+key+'-field').hidden=!visible;
 }
}
function renderPlayback(panel,settings){
 playbackDraft[panel]={...settings};playbackDirty[panel]=false;
 const container=$('playback-'+panel+'-fields');container.replaceChildren();
 if(panel==='lights')playbackStateTabs(container);
 for(const [heading,fields,state] of playbackFields[panel]){
  const parent=state?$('playback-state-'+state):container;
  const title=document.createElement('h3');title.textContent=heading;parent.append(title);
  const grid=document.createElement('div');grid.className='playback-fields';parent.append(grid);
  for(const [key,label,type] of fields){
   const box=document.createElement('div');box.className='playback-field';box.id='playback-'+key+'-field';if(key.endsWith('_source'))box.classList.add('playback-source');grid.append(box);
   const id='playback-'+key,caption=document.createElement('label');caption.htmlFor=id;caption.textContent=label;box.append(caption);
   let control;
   if(Array.isArray(type)){
    control=document.createElement('select');control.replaceChildren(...type.map(([value,name])=>option(name,value)));control.value=settings[key];
    control.onchange=()=>{playbackDraft[panel][key]=control.value;playbackChanged(panel)};
   }else if(type==='toggle'){
    control=document.createElement('input');control.type='checkbox';control.checked=!!settings[key];control.setAttribute('role','switch');box.classList.add('playback-toggle');
    control.onchange=()=>{playbackDraft[panel][key]=control.checked;playbackChanged(panel)};
   }else if(type==='color'){
    control=document.createElement('input');control.type='color';control.value=settings[key];
    control.oninput=()=>{playbackDraft[panel][key]=control.value;playbackChanged(panel)};
   }else{
    if(type==='brightness'){
     const mode=document.createElement('select');mode.id=id;mode.setAttribute('aria-label',label+' mode');mode.replaceChildren(option('Use Scene','scene'),option('Override','override'));mode.value=settings[key]<0?'scene':'override';box.append(mode);caption.htmlFor=mode.id;
     const row=document.createElement('div');row.className='playback-range';row.hidden=settings[key]<0;box.append(row);
     control=document.createElement('input');control.type='range';control.min=0;control.max=100;control.value=settings[key]<0?70:settings[key];control.id=id+'-value';control.setAttribute('aria-label',label+' override');
     const output=document.createElement('output');output.htmlFor=control.id;output.textContent=control.value+'%';row.append(control,output);
     mode.onchange=()=>{row.hidden=mode.value==='scene';playbackDraft[panel][key]=row.hidden?-1:Number(control.value);playbackChanged(panel)};
     control.oninput=()=>{output.textContent=control.value+'%';playbackDraft[panel][key]=Number(control.value);playbackChanged(panel)};
     continue;
    }
    const row=document.createElement('div');row.className='playback-range';box.append(row);
    control=document.createElement('input');control.type='range';control.min=0;control.max=100;control.value=settings[key];control.id=id;
    const output=document.createElement('output');output.htmlFor=id;output.textContent=control.value+'%';row.append(control,output);
    control.oninput=()=>{output.textContent=control.value+'%';playbackDraft[panel][key]=Number(control.value);playbackChanged(panel)};
    continue;
   }
   control.id=id;box.append(control);
  }
 }
 container.disabled=false;styleControls(container);playbackVisibility(panel);if(panel==='lights')selectPlaybackState(playbackLightState);
}
async function loadPlayback(panel=null){
 if(!playbackInitialized){
  playbackInitialized=true;
  for(const name of ['lights','screen']){
   $('playback-'+name+'-form').onsubmit=e=>{e.preventDefault();savePlayback(name)};
   $('playback-'+name+'-reload').onclick=()=>{if(!playbackDirty[name]||confirm('Discard unsaved playback '+name+' settings?'))loadPlayback(name)};
  }
 }
 const panels=panel?[panel]:['lights','screen'];
 try{const settings=await request('/api/playback');for(const name of panels){if(!playbackBusy[name]){renderPlayback(name,settings);playbackNotice(name,settings.stored?'':'Settings are not saved',!settings.stored)}}}
 catch(e){for(const name of panels)playbackNotice(name,e.message,true)}
}
async function savePlayback(panel){
 if(playbackBusy[panel]||!playbackDraft[panel])return;
 playbackBusy[panel]=true;const fieldset=$('playback-'+panel+'-fields'),save=$('playback-'+panel+'-save'),reload=$('playback-'+panel+'-reload');fieldset.disabled=save.disabled=reload.disabled=true;
 const data={};for(const [,fields] of playbackFields[panel])for(const [key] of fields){const value=playbackDraft[panel][key];data[key]=typeof value==='boolean'?(value?1:0):value}
 try{const settings=await request('/api/playback',data);renderPlayback(panel,settings);playbackNotice(panel,'Saved')}
 catch(e){playbackNotice(panel,e.message,true)}
 finally{playbackBusy[panel]=false;fieldset.disabled=save.disabled=reload.disabled=false}
}
