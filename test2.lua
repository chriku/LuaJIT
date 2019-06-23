require"util"
local hdp=require"hdp"
local file=io.open("out.txt")
local v=unpack(file:read("*a"))
local r=hdp.unpersist(v,{user=print})
print(coroutine.resume(r))
local v=hdp.persist(r,{[print]="user"})
local file=io.open("out.txt","w")
file:write(pack(v))
file:close()

