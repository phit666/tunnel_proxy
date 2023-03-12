# Tunnel Proxy
Tunnel proxy is meant to allow local services like Home Assistant, Remote Desktop etc.. to be available anywhere without having a public IP nor a need to change your local router configurations.
It is coded with c++ and currently can be compiled with Windows and Linux, libevent 2.0.22 and newer is required.

# tunnel
This is console program you will run locally in the same network as your local/internal server, this program will create a network tunnel between the local server and tunnel_proxy.

tunnel parameters
tunnel \<manage ip> \<manage port> \<tunnel port> \<local-server-ip> \<local-server-port>

manage ip: This is a public IP where tunnel_proxy is running.

manage port: Manage port of tunnel_proxy, currently hardcoded with 8020

tunnel port: Tunnel port of tunnel_proxy, currently hardcoded with 8000

local-server-ip: Local IP of the server you wanted to be accessible online.

local-server-port: Port of the local service (Home Assistant default is 8123, Remote Desktop is 3389 ...)

# tunnel_proxy
This is a console program you will run in your external server/cloud, let say a VPS hosted online with a public IP, now to connect to your local/internal server you will simply need to connect in tunnel_proxy public IP.

Currently running tunnel_proxy requires no parameters but the hardcoded ports (8020, 8000 and 8123) are required to be opened in your online/cloud server firewall.

# Example:
Let say you have a Home Assistant server (Debian Linux OS) in your house and you don't have a public IP but you have cheap VPS running in Linux online with public IP 139.88.23.50, now to make your Home Assistant server be accessible in the internet you can run tunnel in the Home Assistant server with the default parameters...

tunnel 139.88.23.50 8020 8000 127.0.0.1 8123

... then run tunnel_proxy in your online VPS (required ports opened)

Now you can access your Home Assistant server with browser or mobile app with address http://139.88.23.50:8123

