ip addr add 10.0.5.1/24 dev toto0 
ifconfig toto0 up
route add -net 10.0.4.0/24 dev toto0
