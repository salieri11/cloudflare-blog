#!/bin/bash

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

declare -A tests_proxy_exec=(
  ["proxy_sockmap"]="~/cloudflare-blog/2019-02-tcp-splice/echo-sockmap-proxy 0.0.0.0:8080 10.128.0.5:8086"
  ["proxy_splice"]="~/cloudflare-blog/2019-02-tcp-splice/echo-splice-proxy 0.0.0.0:8080 10.128.0.5:8086"
  ["proxy_naive"]="~/cloudflare-blog/2019-02-tcp-splice/echo-naive-proxy 0.0.0.0:8080 10.128.0.5:8086 1"
  ["proxy_iosubmit"]="~/cloudflare-blog/2019-02-tcp-splice/echo-iosubmit-proxy 0.0.0.0:8080 10.128.0.5:8086"
  ["ktcp-bandwidth"]=""
  ["ktcpz-bandwidth"]=""
)
declare -A tests_kernels=(
  ["proxy_sockmap"]="0"
  ["proxy_splice"]="0"
  ["proxy_naive"]="0"
  ["proxy_iosubmit"]="0"
  ["ktcp-bandwidth"]="1"
  ["ktcpz-bandwidth"]="1"
)
declare -A tests_file_prefix=(
  ["proxy_sockmap"]="bandwidth"
  ["proxy_splice"]="bandwidth"
  ["proxy_naive"]="bandwidth"
  ["proxy_iosubmit"]="bandwidth"
  ["ktcp-bandwidth"]="bandwidth"
  ["ktcpz-bandwidth"]="bandwidth"
)
declare -A tests_sender_port=(
  ["proxy_sockmap"]="8080"
  ["proxy_splice"]="8080"
  ["proxy_naive"]="8080"
  ["proxy_iosubmit"]="8080"
  ["ktcp-bandwidth"]="8081"
  ["ktcpz-bandwidth"]="8082"
)

for test_name in "${!tests_proxy_exec[@]}"; do
  test_exec="${tests_proxy_exec[$test_name]}"
  for it in `seq 1 1`; do
   echo "[+] starting iteration "$it" for test "$test_name"..."
   kernel_id=${tests_kernels[$test_name]};

   # wait for machines to start
   # wait_for_restart $ip_receiver
   # wait_for_restart $ip_sender
   restart $ip_proxy $kernel_id

   # if test runs on MAIO kernel, first build KTCP binaries, otherwise run one of the proxies binaries
   if [[ $kernel_id -eq 1 ]]; then
     eval $ssh_command$ip_proxy" ~/make_proxy.sh"
   else
     eval $ssh_command$ip_proxy" 'nohup sudo $test_exec > /dev/null 2>&1 &'"
   fi
   echo '[+] proxy is running...'

   # run receiver
   eval $ssh_command$ip_receiver" killall tcp_server_flow.pl"
   eval $ssh_command$ip_receiver" 'nohup ~/ktcp_split/tcping/tcp_server_flow.pl 8086 > 1.txt 2>&1 &'"
   echo '[+] receiver is running...'

   sender_port="${tests_sender_port[$test_name]}"
   # run sender
   eval $ssh_command$ip_sender" killall tcp_client_flow.pl"
   eval $ssh_command$ip_sender" '~/ktcp_split/tcping/tcp_client_flow.pl $sender_port > /dev/null 2>&1 &'"
   echo '[+] sender is running...'

   # wait 7 sec to warm up the traffic
   echo '[+] wait 5 sec to warm up the test...'
   sleep 5

   # run collect and post on proxy
   prefix="${tests_file_prefix[$test_name]}"
   echo '[+] collecting...'
   eval $ssh_command$ip_proxy" '~/collect.sh > $res_folder/$prefix'_'$test_name'_collect_'$it'"
   echo '[+] posting...'
   eval $ssh_command$ip_proxy" 'nohup ~/post.sh > $res_folder/$prefix'_'$test_name'_post_'$it &'"

   echo '[+] killing all...'
   eval $ssh_command$ip_proxy" sudo killall echo-sockmap-proxy"
   eval $ssh_command$ip_receiver" killall tcp_server_flow.pl"
   eval $ssh_command$ip_sender" killall tcp_client_flow.pl"

   echo "[+] done iteration "$it" for test "$test_name"..."

  done
done

echo '[+] Done!'
