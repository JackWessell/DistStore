#/bin/bash
echo $1
echo ""
echo "#################  SINGLE NODE GET/PUT ################ " 
echo ""
./build/src/test_app single_set_get $1
echo ""
echo "#################  MULTI NODE GET/PUT (n = 5)################"
echo ""
./build/src/test_app multi_set_get $1
echo ""
echo "#################  MULTI NODE SYSTEM - SINGLE NODE FAIL (n = 3, k = 2) ################"
echo ""
./build/src/test_app single_node_fail $1
echo ""
echo "#################  MULTI NODE SYSTEM - MULTI NODE FAIL (n = 7, k = 3) ################"
echo ""
./build/src/test_app multi_node_fail $1