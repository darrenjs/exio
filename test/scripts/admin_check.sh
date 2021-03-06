#!/usr/bin/env bash

server_addr=hp
#server_addr=127.0.0.1
server_port=33311

admincmd="./admin"

number_runs=10

admin_report_dir=/var/tmp/adminlogs
mkdir -p $admin_report_dir


# Run the admin
N=0
while true;
do
    echo Run $N

    export XLOG_FILENAME="/tmp/xlog.admin.${N}.dat"

    adminlog=${admin_report_dir}/admin.log.${N}

    $admincmd -d -d -d -d $server_addr $server_port diags  >  ${adminlog}

    # now check the admin completed ok

    string_to_find="tables"

    nmatches=`fgrep -c -x ${string_to_find}  ${adminlog}`
    if [ $nmatches == "2" ] ;
    then
        if [ $N == "0" ];
        then
            cp ${adminlog} ${adminlog}.initial
        fi
    else
        echo problem with run ${N}
        cp  ${adminlog}  ${adminlog}.problem
        mv  ${adminlog}.problem .
        cp   ${XLOG_FILENAME} ${XLOG_FILENAME}.problem
        mv   ${XLOG_FILENAME}.problem .
        break;
    fi

    if [ -e core ];
    then
        echo core dump, run ${N}
        mv   core core.run.${N}
        cp  ${adminlog}  ${adminlog}.coredump
        mv  ${adminlog}.coredump .
        cp  ${XLOG_FILENAME} ${XLOG_FILENAME}.coredump
        mv  ${XLOG_FILENAME}.coredump .
        break;
    fi

    # under normal conditions, we remove the log
    rm -f ${adminlog}
    rm -f ${XLOG_FILENAME}
    N=`expr $N + 1`

done

