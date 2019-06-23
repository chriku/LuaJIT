set -e
make -j8
./src/luajit test.lua
echo =====
./src/luajit test2.lua
echo =====
./src/luajit test2.lua
echo =====
./src/luajit test2.lua
echo =====
./src/luajit test2.lua
echo =====
./src/luajit test2.lua
#gdb --batch -ex "run test.lua" -ex "where" ./src/luajit

