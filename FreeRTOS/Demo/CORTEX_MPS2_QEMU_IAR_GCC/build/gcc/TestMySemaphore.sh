#!/bin/bash

source TestUtilities.sh

SRC=../..
TEST_FILE=$SRC/main_my_sem.c

USE_MY_SEM_STR="#define USE_MY_SEM (0)"
RUNNING_TEST_STR="#define RUNNING_TEST (0)"

USE_MY_SEM="#define USE_MY_SEM (1)"
USE_DEFAULT_SEM="#define USE_MY_SEM (0)"

set_up

for test_name in BINARY_SAME_PRIORITY BINARY_DIFF_PRIORITY \
                 COUNTING_SAME_PRIORITY COUNTING_DIFF_PRIORITY
do
    # set test
    running_test="#define RUNNING_TEST ($test_name)"
    sed -i "" "s/$RUNNING_TEST_STR/$running_test/" $TEST_FILE

    echo "running test for $test_name..."

    # run my sem test
    sed -i "" "s/$USE_MY_SEM_STR/$USE_MY_SEM/" $TEST_FILE
    my_sem_out="test_output/my_sem_$test_name.out"
    run_and_kill $my_sem_out

    # run default sem test
    sed -i "" "s/$USE_MY_SEM/$USE_DEFAULT_SEM/" $TEST_FILE
    default_out="test_output/default_sem_$test_name.out"
    run_and_kill $default_out

    # verify that output files are equal
    if cmp -s $my_sem_out $default_out; then
        echo "[*] PASS"
    else
        echo "[X] FAIL"
    fi
    echo ""

    # undo the edits
    sed -i "" "s/$running_test/$RUNNING_TEST_STR/" $TEST_FILE
done

tear_down
