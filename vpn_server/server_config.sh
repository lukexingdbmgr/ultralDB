ip addr add 10.0.4.1/24 dev tun0 # server tun ip
ifconfig tun0 up
route add -net 10.0.5.0/24 dev tun0 
