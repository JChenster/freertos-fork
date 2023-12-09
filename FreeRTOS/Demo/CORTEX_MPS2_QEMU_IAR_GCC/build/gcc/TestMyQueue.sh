#!/bin/bash

source TestUtilities.sh

SRC=../..
TEST_FILE=$SRC/main_my_queue.c

USE_MY_QUEUE_STR="#define USE_MY_QUEUE (0)"
RUNNING_TEST_STR="#define RUNNING_TEST (0)"

USE_MY_QUEUE="#define USE_MY_QUEUE (1)"
USE_DEFAULT_QUEUE="#define USE_MY_QUEUE (0)"

set_up

for test_name in SIMPLE FAST_SLOW SLOW_FAST SEND_BACK_ISR RECEIVE_ISR
do
    # set test
    running_test="#define RUNNING_TEST ($test_name)"
    sed -i "" "s/$RUNNING_TEST_STR/$running_test/" $TEST_FILE

    echo "running test for $test_name..."

    # run my queue test
    sed -i "" "s/$USE_MY_QUEUE_STR/$USE_MY_QUEUE/" $TEST_FILE
    my_queue_out="test_output/my_queue_$test_name.out"
    run_and_kill $my_queue_out

    # run default queue test
    sed -i "" "s/$USE_MY_QUEUE/$USE_DEFAULT_QUEUE/" $TEST_FILE
    default_out="test_output/default_queue_$test_name.out"
    run_and_kill $default_out

    # verify that output files are equal
    if cmp -s $my_queue_out $default_out; then
        echo "[*] PASS"
    else
        echo "[X] FAIL"
    fi
    echo ""

    # undo the edits
    sed -i "" "s/$running_test/$RUNNING_TEST_STR/" $TEST_FILE
done

tear_down
