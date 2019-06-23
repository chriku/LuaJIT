collectgarbage("stop")
jit.off()
local json=require"dkjson"
local cnt={}
local function rdecode(data,int,hd)
  if type(data)~="table" then
    print(type(data),":",data)
    --print(debug.traceback())
  else
    for k,v in pairs(data) do
      for i=1,int do io.write"  " end
      if type(v)=="table" and int<10 and not hd[v] then
        hd[v]=true
        print(k,v)
        rdecode(v,int+1,hd)
      elseif k=="raw" and type(v)=="string" and v:len()==8 then
        v=v:gsub(".",function(a) return string.format("%02X",string.byte(a)) end)
        print(k,v)
      elseif k=="addr" and type(v)=="string" and v:len()==8 then
        v=v:gsub(".",function(a) return string.format("%02X",string.byte(a)) end)
        print(k,v)
      elseif type(v)=="function" then
        local c=hd[v]
        if not c then
          hd[cnt]=(hd[cnt] or 0)+1
          hd[hd[cnt]]=v
          hd[v]=hd[cnt]
          c=hd[cnt]
        end
        print(k,"function ("..c..")")
      else
        print(k,v)
      end
    end
  end
end
function decode(data)
  print("===== BEGIN =====")
  local hd={}
  rdecode(data,0,hd)
  if hd[cnt] then
    print("===== FUNCS =====")
    for i=1,hd[cnt] do
      print("function ("..i..")")
      require"jit.bc".dump(hd[i])
    end
  end
  print("===== END =====")
end


function pack(v)
  local tables={}
  local function pv(v)
    if type(v)=="table" then
      if tables[v] then return{type="table",data=tables[v]} end
      local idx=(#tables)+1
      local t={}
      tables[idx]=t
      tables[v]=idx
      for k,v in pairs(v) do
        table.insert(t,{key=pv(k),val=pv(v)})
      end
      return {type="table",data=idx}
    else
      return {type=type(v),data=v}
    end
  end
  local val=pv(v)
  for k,v in pairs(tables) do
    if type(k)=="table" then tables[k]=nil end
  end
  return json.encode({tables=tables,value=val})
end
function unpack(v)
  local av=assert(json.decode(v))
  local tables={}
  local function pv(v)
    if v.type=="table" then
      if tables[v.data] then return tables[v.data] end
      local t={}
      tables[v.data]=t
      for _,p in ipairs(av.tables[v.data]) do
        t[pv(p.key)]=pv(p.val)
      end
      return t
    else
      return v.data
    end
  end
  return pv(av.value)
end
