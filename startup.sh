#!/bin/bash

for i in {1..3}
do
  ./storage_server config_master.txt 1 $i -v &
done

