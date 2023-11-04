#!/bin/bash

function qemu_run() {
    qemu-system-arm -machine mps2-an385 \
                    -cpu cortex-m3 \
                    -kernel "output/RTOSDemo.out" \
                    -monitor none \
                    -nographic \
                    -serial stdio
}

function run_and_kill() {
    out=$1

    make clean > /dev/null
    make -j8 > /dev/null

    # run program and then kill it
    qemu_run > $out &
    pid=$!
    # kill after 1 second, try to write test cases so they can pass in 1 second
    sleep 1
    pkill $pid > /dev/null

    # print first line and then remove it
    echo "- $(head -n 1 $out)"
    sed -i "" "1d" $out
    echo -e "\tOutput file: $out"
}

function set_up() {
    # remove test output
    rm test_output/*.out
}

function tear_down() {
    # kill qemu VMs
    echo "Tearing down"
    ps aux | grep "qemu-system-arm" | awk '{print $2}' | xargs kill -9
}
