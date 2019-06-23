require"util"
local hdp=require"hdp"
local file=io.open("out.txt")
local v=unpack(file:read("*a"))
local r=hdp.unpersist(v)
print(coroutine.resume(r))
local v=hdp.persist(r)
local file=io.open("out.txt","w")
file:write(pack(v))
file:close()

