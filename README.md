# Tunnel Proxy
Tunnel proxy is meant to allow local services like Home Assistant, Remote Desktop etc.. to be available anywhere without having a public IP nor a need to change your local router configurations.
It is composed of two separate program, tunnel and tunnel_proxy.

# tunnel
This is the program you will run locally in the same network as your local/internal server, this program will create a network tunnel between the local server and tunnel_proxy.

# tunnel_proxy
This is program you will run in your external server/cloud, let say a VPS hosted online with a public IP, now to connect to your local/internal server you will simply need to connect in tunnel_proxy public IP.

