#!/usr/bin/env bash


echo one $1
echo two $2

if [ -z "$3" ];
then
    echo please provide:  ADDR PORT NUMBER_RUNS
else

    server_addr=$1
    server_port=$2
    number_runs=$3

    echo launching $number_runs runs, connecting to  $server_addr $server_port

   admincmd="admin"

    admin_report_dir=/var/tmp/adminlogs

    mkdir -p $admin_report_dir

    N=0
    while true;
    do
        R=`expr $N \>= $number_runs`

        if [ $R == '1' ];
        then
            break
        fi

        N=`expr $N + 1`
        $admincmd $server_addr $server_port > /dev/null &

        pid=$!
        echo instance $N started with pid $pid

        sleep 1
    done
fi













