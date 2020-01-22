#!/bin/bash

function wait_for_restart () {
        ping -q -c 1 $1
        while [ $? -ne 0 ]
        do
                echo "[+] waiting for "$1" to restart..."
                sleep 30
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

#restart machines
echo '[+] restarting proxy...'
#eval $ssh_command$ip_receiver" 'sudo reboot'"
#eval $ssh_command$ip_sender" 'sudo reboot'"
"radio_test_runner.sh" 68L, 1913C                 4,16-23       Top
#!/bin/bash

function wait_for_restart () {
        ping -q -c 1 $1
        while [ $? -ne 0 ]
        do
                echo "[+] waiting for "$1" to restart..."
                sleep 30
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

#restart machines
echo '[+] restarting proxy...'
#eval $ssh_command$ip_receiver" 'sudo reboot'"
#eval $ssh_command$ip_sender" 'sudo reboot'"
eval $ssh_command$ip_proxy" 'sudo reboot'"

# wait for machines to reboot
echo '[+] waiting 30 sec for reboot...'
sleep 30

# wait for machines to start
#wait_for_restart $ip_receiver
#wait_for_restart $ip_sender
wait_for_restart $ip_proxy


#nohup ./whatever > /dev/null 2>&1 &
# run proxy
eval $ssh_command$ip_proxy" sudo killall echo-sockmap-proxy"
eval $ssh_command$ip_proxy" 'nohup sudo ~/cloudflare-blog/2019-02-tcp-splice/echo-sockmap-proxy 0.0.0.0:8080 10.128.0.5:8080 > /dev/null 2>&1 &'"
echo '[+] proxy is running...'

# run receiver
eval $ssh_command$ip_receiver" killall tcp_server_flow.pl"
eval $ssh_command$ip_receiver" 'nohup ~/ktcp_split/tcping/tcp_server_flow.pl 8080 > 1.txt 2>&1 &'"
echo '[+] receiver is running...'

# run sender
eval $ssh_command$ip_sender" killall tcp_client_flow.pl"
eval $ssh_command$ip_sender" '~/ktcp_split/tcping/tcp_client_flow.pl 8080 > /dev/null 2>&1 &'"
echo '[+] sender is running...'

# wait 7 sec to warm up the traffic
echo '[+] wait 5 sec to warm up the test...'
sleep 5

# run collect and post on proxy
echo '[+] collecting...'
eval $ssh_command$ip_proxy" '~/collect.sh > $res_folder/sockmap_collect_iter_1'"
echo '[+] posting...'
eval $ssh_command$ip_proxy" 'nohup ~/post.sh > $res_folder/sockmap_post_iter_1 &'"


echo '[+] killing all...'
eval $ssh_command$ip_proxy" sudo killall echo-sockmap-proxy"
eval $ssh_command$ip_receiver" killall tcp_server_flow.pl"
eval $ssh_command$ip_sender" killall tcp_client_flow.pl"

echo '[+] Done!'
