#!/bin/bash

declare -A tests_proxy_exec=(
 ["msg_zerocopy"]="~/cloudflare-blog/2019-02-tcp-splice/test-burst-zerocopy 10.128.0.5:8085 200000 300000"
 ["naive"]="~/cloudflare-blog/2019-02-tcp-splice/test-burst 10.128.0.5:8085 200 300000"
 ["ktcp"]="echo 8085 > /proc/io_client/tcp_client"
)

declare -A tests_kernels=(
  ["msg_zerocopy"]="0"
  ["naive"]="0"
  ["ktcp"]="1"
)

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

num_of_processes=3
num_of_iterations=1
for it in `seq 1 $num_of_iterations`; do
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

   # run 16 proxies
   for t in `seq 1 $num_of_processes`; do
     if [[ $kernel_id -eq 0 ]]; then
       eval $ssh_command$ip_proxy" 'sudo nohup '$test_exec' > /dev/null 2>&1 &'"
     else
       eval $ssh_command$ip_proxy" 'echo 8085 > /proc/io_client/tcp_client'"
     fi
   done

   echo '[+] '$num_of_processes' senders are running...'

   # wait 5 sec to warm up the traffic
   echo '[+] wait 5 sec to warm up the test...'
   sleep 5

   # run collect and post on proxy
   echo '[+] collecting...'
   eval $ssh_command$ip_proxy" '~/collect.sh > $res_folder'/bandwidth_2_multi_'$test_name'_collect_'$it'"
   echo '[+] posting...'
   eval $ssh_command$ip_proxy" 'sudo /home/xlr8vgn/ubuntu-bionic/tools/perf/perf report -n --stdio --no-child -i /tmp/perf.data > $res_folder'/bandwidth_2_multi_'$test_name'_post_'$it'"

   echo "[+] done iteration "$it" for test "$test_name"..."

  done
done

echo '[+] Done!'
