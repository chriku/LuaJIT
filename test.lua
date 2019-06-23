require"util"
local hdp=require"hdp"
local t={x=0,y=0}
local op=print
local print=print
local function cf()
  local z=0
  local function f()
    t.x,t.y=t.y,t.x+1
    print("X",t.x,t.y,z)
  end
  while true do
    z=z+5
    f(2)
    coroutine.yield()
  end
end
local co=coroutine.create(cf)
for i=1,4 do
  coroutine.resume(co)
end
local st={}
local v=hdp.persist(co,{[print]="user"})
local file=io.open("out.txt","w")
file:write(pack(v))
file:close()
