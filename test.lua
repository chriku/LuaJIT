require"util"
local hdp=require"hdp"
local t={x=0,y=0}
local function cf()
  local function f()
    t.x,t.y=t.y,t.x+1
    print("X",t.x,t.y)
    coroutine.yield()
  end
  while true do
    f(2)
  end
end
local co=coroutine.create(cf)
for i=1,4 do
  coroutine.resume(co)
end
local v=hdp.persist(co)
local file=io.open("out.txt","w")
file:write(pack(v))
file:close()
