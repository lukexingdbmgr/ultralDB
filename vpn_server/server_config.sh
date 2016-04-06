ip addr add 10.0.4.1/24 dev toto0 # server tun ip
ifconfig toto0 up
route add -net 10.0.5.0/24 dev toto0 
