// Browser-only dropdowns. Hidden selects retain form values and existing handlers.
window.Dropdowns=(()=>{
  let active=null,sequence=0;
  const valueProperty=Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype,'value');
  const labelText=label=>{const copy=label.cloneNode(true);copy.querySelectorAll('select,input,button,.dropdown-select').forEach(n=>n.remove());return copy.textContent.trim()};
  function close(focus=false){
    if(!active)return;
    const current=active;active=null;current.menu?.remove();current.menu=null;current.rows=[];current.groups=[];current.search=null;
    current.button.setAttribute('aria-expanded','false');
    if(focus&&current.button.isConnected)current.button.focus({preventScroll:true});
  }
  function position(){
    if(!active)return;
    const p=active;if(!p.select.isConnected){close();return}
    const r=p.button.getBoundingClientRect(),v=window.visualViewport;
    const left=v?.offsetLeft||0,top=v?.offsetTop||0,width=v?.width||innerWidth,height=v?.height||innerHeight;
    const below=top+height-r.bottom-12,above=r.top-top-12,down=below>=220||below>=above;
    const available=Math.max(60,down?below:above),menuWidth=Math.min(Math.max(r.width,220),width-16);
    p.menu.style.width=menuWidth+'px';p.menu.style.maxHeight=Math.min(340,available)+'px';
    p.menu.style.left=Math.max(left+8,Math.min(r.left,left+width-menuWidth-8))+'px';
    p.menu.style.top=(down?r.bottom+4:Math.max(top+8,r.top-4-p.menu.getBoundingClientRect().height))+'px';
  }
  function enhance(root=document){
    root.querySelectorAll('select').forEach(select=>{
      if(select._dropdown){select._dropdown.sync();return}
      const key=select.id||'action-'+[...document.querySelectorAll('.action')].indexOf(select.closest('.action'))+'-'+select.dataset.field;
      const id='dropdown-'+(++sequence),labels=[...select.labels];
      const name=select.getAttribute('aria-label')||labels.map(labelText).filter(Boolean).join(' ')||'Choose option';
      const wrapper=document.createElement('span');wrapper.className='dropdown-select';
      const button=document.createElement('button');button.type='button';button.id=id;button.className='dropdown-control';
      button.setAttribute('role','combobox');button.setAttribute('aria-haspopup','listbox');button.setAttribute('aria-expanded','false');button.setAttribute('aria-controls',id+'-list');
      const caption=document.createElement('span');caption.className='dropdown-value';
      const arrow=document.createElement('span');arrow.className='dropdown-arrow';arrow.setAttribute('aria-hidden','true');button.append(caption,arrow);
      select.before(wrapper);wrapper.append(select,button);select.dataset.dropdownNative='';select.dataset.dropdownKey=key;select.tabIndex=-1;select.setAttribute('aria-hidden','true');
      labels.forEach(label=>label.htmlFor=id);
      const p={select,wrapper,button,menu:null,signature:'',rows:[],search:null,sync};select._dropdown=p;
      select.focus=options=>button.focus(options);select.blur=()=>button.blur();
      Object.defineProperty(select,'value',{configurable:true,get(){return valueProperty.get.call(this)},set(value){valueProperty.set.call(this,value);sync()}});
      function sync(){
        const selected=select.selectedOptions[0],text=selected?.label||'Choose…';
        if(caption.textContent!==text)caption.textContent=text;
        button.title=text;button.setAttribute('aria-label',name+': '+text);button.disabled=select.matches(':disabled');
        const signature=JSON.stringify([...select.options].map(o=>[o.value,o.label,o.disabled,o.parentElement.label,o.parentElement.disabled]));
        if(active===p&&(button.disabled||signature!==p.signature))close();
        p.signature=signature;
        if(active===p)p.rows.forEach(row=>row.button.setAttribute('aria-selected',String(row.option.selected)));
      }
      function choose(option){
        if(select.matches(':disabled')||option.disabled||option.parentElement.disabled)return;
        const previous=select.value;select.value=option.value;close(true);
        if(previous!==select.value){
          select.dispatchEvent(new Event('input',{bubbles:true}));select.dispatchEvent(new Event('change',{bubbles:true}));
          // Command changes can rebuild the action editor and replace this control.
          const replacement=[...document.querySelectorAll('select')].find(s=>s.dataset.dropdownKey===key);
          replacement?._dropdown?.button.focus({preventScroll:true});
        }
      }
      function visible(){return p.rows.filter(row=>!row.button.hidden&&!row.button.disabled)}
      function focusRow(row){
        if(!row)return;
        row.button.focus({preventScroll:true});
        const list=row.button.parentElement,top=row.button.offsetTop-list.offsetTop,bottom=top+row.button.offsetHeight;
        if(top<list.scrollTop)list.scrollTop=top;
        else if(bottom>list.scrollTop+list.clientHeight)list.scrollTop=bottom-list.clientHeight;
      }
      function filter(){
        const query=p.search?.value.trim().toLocaleLowerCase()||'';
        p.rows.forEach(row=>{const text=(row.option.label+' '+(row.option.dataset.search||'')).toLocaleLowerCase();row.button.hidden=!!query&&!query.split(/\s+/).every(word=>text.includes(word))});
        p.groups.forEach(group=>group.heading.hidden=!group.rows.some(row=>!row.button.hidden));
        p.empty.hidden=p.rows.some(row=>!row.button.hidden);position();
      }
      function open(){
        sync();if(button.disabled||!select.options.length)return;
        close();window.closeModifierMenus?.();active=p;
        const menu=document.createElement('div');menu.className='dropdown-menu-custom';p.menu=menu;p.rows=[];p.groups=[];
        const list=document.createElement('div');list.className='dropdown-list';list.id=id+'-list';list.setAttribute('role','listbox');list.setAttribute('aria-label',name);
        p.search=null;
        if(select.options.length>10){
          const search=document.createElement('input');search.type='search';search.className='dropdown-search';search.placeholder='Search options…';search.setAttribute('aria-label','Search '+name);search.autocomplete='off';search.oninput=filter;p.search=search;menu.append(search);
        }
        const add=(option,group)=>{
          const item=document.createElement('button');item.type='button';item.className='dropdown-option';item.tabIndex=-1;item.setAttribute('role','option');item.setAttribute('aria-label',option.label);item.setAttribute('aria-selected',String(option.selected));item.disabled=option.disabled||!!option.parentElement.disabled;
          const text=document.createElement('span');text.textContent=option.label;item.append(text);item.onclick=()=>choose(option);
          const row={option,button:item};p.rows.push(row);group?.rows.push(row);list.append(item);
        };
        for(const child of select.children){
          if(child.tagName==='OPTGROUP'){
            const heading=document.createElement('div');heading.className='dropdown-group';heading.textContent=child.label;heading.setAttribute('role','presentation');list.append(heading);
            const group={heading,rows:[]};p.groups.push(group);[...child.children].forEach(o=>add(o,group));
          }else if(child.tagName==='OPTION')add(child);
        }
        const empty=document.createElement('p');empty.className='dropdown-empty';empty.textContent='No matching options';empty.setAttribute('role','status');empty.hidden=true;p.empty=empty;list.append(empty);menu.append(list);
        (select.closest('dialog')||document.body).append(menu);button.setAttribute('aria-expanded','true');position();
        focusRow(visible().find(row=>row.option.selected)||visible()[0]);
        menu.onkeydown=e=>{
          if(e.key==='Escape'){e.preventDefault();e.stopPropagation();close(true);return}
          if(e.key==='Tab'){close(true);return}
          if(document.activeElement===p.search&&['Home','End'].includes(e.key))return;
          const rows=visible(),index=rows.findIndex(row=>row.button===document.activeElement);
          if(['ArrowDown','ArrowUp','Home','End'].includes(e.key)){
            e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?rows.length-1:index<0?(e.key==='ArrowDown'?0:rows.length-1):(index+(e.key==='ArrowDown'?1:-1)+rows.length)%rows.length;focusRow(rows[next]);
          }else if(document.activeElement!==p.search&&e.key.length===1&&!e.ctrlKey&&!e.metaKey&&!e.altKey&&e.key!==' '){
            e.preventDefault();if(p.search){p.search.value+=e.key;p.search.focus({preventScroll:true});filter()}else typeahead(e.key);
          }else if(document.activeElement===p.search&&e.key==='Enter'){
            e.preventDefault();if(rows.length)choose(rows[0].option);
          }
        };
      }
      let typed='',typedAt=0;
      function typeahead(key){
        const now=Date.now();typed=now-typedAt<700?typed+key:key;typedAt=now;
        focusRow(visible().find(row=>row.option.label.toLocaleLowerCase().startsWith(typed.toLocaleLowerCase())));
      }
      button.onclick=()=>active===p?close(true):open();
      button.onkeydown=e=>{
        if(['ArrowDown','ArrowUp','Home','End'].includes(e.key)){
          e.preventDefault();if(active!==p)open();const rows=visible();focusRow(e.key==='End'||e.key==='ArrowUp'?rows[rows.length-1]:e.key==='Home'?rows[0]:rows.find(row=>row.option.selected)||rows[0]);
        }else if(e.key==='Escape'&&active===p){e.preventDefault();e.stopPropagation();close(true)}
        else if(e.key.length===1&&e.key!==' '&&!e.ctrlKey&&!e.metaKey&&!e.altKey){
          e.preventDefault();if(active!==p)open();if(p.search){p.search.value=e.key;p.search.focus({preventScroll:true});filter()}else typeahead(e.key);
        }
      };
      select.addEventListener('change',sync);select.addEventListener('invalid',e=>{e.preventDefault();button.focus()});
      new MutationObserver(sync).observe(select,{subtree:true,childList:true,characterData:true,attributes:true,attributeFilter:['disabled','label','selected','value']});
      sync();
    });
  }
  document.addEventListener('pointerdown',e=>{if(active&&!active.wrapper.contains(e.target)&&!active.menu.contains(e.target))close()});
  document.addEventListener('focusin',e=>{if(active&&!e.target.matches('dialog')&&!active.wrapper.contains(e.target)&&!active.menu.contains(e.target))close()});
  document.addEventListener('scroll',e=>{if(active&&!active.menu.contains(e.target)&&e.target!==active.menu)position()},true);
  document.addEventListener('reset',e=>setTimeout(()=>enhance(e.target),0));
  document.addEventListener('close',e=>{if(active&&e.target.contains(active.select))close()},true);
  window.addEventListener('resize',position);window.visualViewport?.addEventListener('resize',position);
  new MutationObserver(records=>{if(records.some(r=>r.target.matches('fieldset,select')))enhance()}).observe(document.documentElement,{subtree:true,attributes:true,attributeFilter:['disabled']});
  return {enhance,close};
})();
