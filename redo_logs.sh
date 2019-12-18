#!/bin/bash

for i in {1..3}
do
  rm -rf key_value_$i.log
  echo 0 > log_seq_no_$i.txt
done

