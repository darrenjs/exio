#!/bin/bash

host=hp
port=33311

admin $host $port  > /dev/null
result=$?
echo finished at `date` error code $result
