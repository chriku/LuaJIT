require"util"
local hdp=require"hdp"
local x=0
local function cf()
  local function f()
    x=x+1
    local y=x
    print("X",x)
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
