#!/bin/bash

declare -A tests_proxy_exec=(
 ["proxy_sockmap"]="~/cloudflare-blog/2019-02-tcp-splice/echo-sockmap-proxy 0.0.0.0:"
 ["proxy_splice"]="~/cloudflare-blog/2019-02-tcp-splice/echo-splice-proxy 0.0.0.0:"
 ["proxy_naive"]="~/cloudflare-blog/2019-02-tcp-splice/echo-naive-proxy 0.0.0.0:"
 ["proxy_iosubmit"]="~/cloudflare-blog/2019-02-tcp-splice/echo-iosubmit-proxy 0.0.0.0:"
 ["ktcp"]=""
 ["ktcpz"]=""
)

declare -A tests_endpoint_sink=(
  ["ktcp"]=""
  ["ktcpz"]=""
)


declare -A tests_kernels=(
  ["proxy_sockmap"]="0"
  ["proxy_splice"]="0"
  ["proxy_naive"]="0"
  ["proxy_iosubmit"]="0"
  ["ktcp"]="1"
  ["ktcpz"]="1"
)

declare -A ports_proxy=(
["ktcp"]="8081"
["ktcpz"]="8082"
)

sink_endpoint="10.128.0.5:8085"

function restart () {
  echo "[+] restarting proxy with kernel "$2
  eval $ssh_command$ip_proxy" sudo grub-reboot $2"
  eval $ssh_command$ip_proxy" sudo reboot"

  #give enough time to reboot
  sleep 120

  ping -q -c 1 $1
  while [ $? -ne 0 ]
    do
    echo "[+] waiting for "$1" to restart..."
    sleep 10
    ping -q -c 1 $1

  done
  sleep 10
  echo "[+] "$1" restarted..."
}

ip_sender="10.128.0.9"
ip_proxy="10.128.0.10"
ip_receiver="10.128.0.5"
ssh_command="ssh -i ~/.ssh/id_rsa pvenugopal@"

# create output folder on the proxy
res_folder=$(eval $ssh_command$ip_proxy" './get_name.sh'")
res_folder="~/Results/"$res_folder
echo '[+] created results folder "'$res_folder'"...'

for it in `seq 1 10`; do
  for test_name in "${!tests_proxy_exec[@]}"; do
   test_exec="${tests_proxy_exec[$test_name]}"
   echo "[+] starting iteration "$it" for test "$test_name"..."
   kernel_id=${tests_kernels[$test_name]};

   # wait for machines to start
   restart $ip_proxy $kernel_id

   # if test runs on MAIO kernel, first build KTCP binaries, otherwise run one of the proxies binaries
   if [[ $kernel_id -eq 1 ]]; then
     eval $ssh_command$ip_proxy" ~/make_proxy.sh"
   fi

   command=""
   base_port=7000
   if [[ ! -z $test_exec ]]; then # if not empty then run 16 proxies
     for t in `seq 1 16`; do
       echo $base_port
       echo "sudo "$test_exec$base_port" "$sink_endpoint
       eval $ssh_command$ip_proxy" 'sudo nohup '$test_exec$base_port' '$sink_endpoint' > /dev/null 2>&1 &'"
       let "base_port++"
     done
     base_port=7000
   else
     base_port=${ports_proxy[$test_name]};
   fi

   echo '[+] 16 proxies are running...'
   sleep 5

   for t in `seq 1 16`; do
     eval $ssh_command$ip_sender" 'echo '$base_port' > /proc/io_client/tcp_client'"
     echo "'echo '$base_port' > /proc/io_client/tcp_client'"
     if [[ ! -z $test_exec ]]; then
       let "base_port++"
     fi
   done
   echo "[+] 16 clients are running..."

   # wait 5 sec to warm up the traffic
   echo '[+] wait 5 sec to warm up the test...'
   sleep 5

   # run collect and post on proxy
   echo '[+] collecting...'
   eval $ssh_command$ip_proxy" '~/collect.sh > $res_folder'/bandwidth_multi_'$test_name'_collect_'$it'"
   echo '[+] posting...'
   eval $ssh_command$ip_proxy" 'sudo /home/xlr8vgn/ubuntu-bionic/tools/perf/perf report -n --stdio --no-child -i /tmp/perf.data > $res_folder'/bandwidth_multi_'$test_name'_post_'$it'"

   echo "[+] done iteration "$it" for test "$test_name"..."

  done
done

echo '[+] Done!'
