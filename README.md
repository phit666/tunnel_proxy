# Tunnel Proxy
This will let your local services be online without opening a port in your main host's firewall, it simply means you can access a service like your home's remote desktop anywhere even if your internet is not public and it can also secure a server by not exposing it's IP address as you can let a dummy host handle the request.

LOCAL Service 


# tunnel
This is console program you will run locally in the same network as your local/internal server, this program will create a network tunnel between the local server and tunnel_proxy.

*tunnel parameters*\
**tunnel \<manage ip> \<manage port> \<tunnel port> \<local-server-ip> \<local-server-port>**

- manage ip: This is a public IP where tunnel_proxy is running.
- manage port: Manage port of tunnel_proxy.
- tunnel port: Tunnel port of tunnel_proxy.
- local-server-ip: Local IP of the server you wanted to be accessible online.
- local-server-port: Port of the local service (Home Assistant default is 8123, Remote Desktop is 3389 ...).

# tunnel_proxy
This is a console program you will run in your external server/cloud, let say a VPS hosted online with a public IP, now to connect to your local/internal server you will simply need to connect in tunnel_proxy public IP.

All ports of tunnel_proxy should be opened in the firewall.

*tunnel_proxy parameters*\
**tunnel_proxy \<max tunnels> \<min tunnels> \<manage port> \<tunnel port> \<proxy port>**

- max tunnels: The maximum tunnel connections to be opened and ready for use, this is useful for browser client as it tends to do a parralel connections in a session.
- min tunnels: This is the tunnel low water mark, when the available tunnel connections is below this then the tunnel_proxy will request the tunnel to create new tunnel(s).
- manage port: The port use by tunnel and tunnel_proxy for internal communication like opening tunnel request.
- tunnel port: The port where all tunnels will be connected.
- proxy port: The port where the users will connect.

# Example
**How to make a local device service be accessible online**

Let say you have a Home Assistant server (Debian Linux OS) in your house and you don't have a public IP but you have cheap VPS running in Linux online with public IP 139.88.23.50, now to make your Home Assistant server be accessible online, you can run tunnel in the Home Assistant server with the default parameters...

- tunnel 139.88.23.50 8020 8000 127.0.0.1 8123

then run tunnel_proxy in your online VPS (inbound ports 8020,8000 and 8123 should be allowed in VPS firewall)

- tunnel_proxy 20 15 8020 8000 8123

Now you can access your Home Assistant server with browser or mobile app with address http://139.88.23.50:8123

**How to secure a server**

Let say you have an online main server with IP 139.88.23.50 and linux VPS with IP 139.88.1.20, now you can secure your main server by not opening the RDP port and not exposing it's IP.

Run tunnel in your main server with parameters...

- tunnel 139.88.1.20 4000 4001 127.0.0.1 3389

Run tunnel_proxy in VPS with parameters...

- tunnel_proxy 1 0 4000 4001 3389

Now you can connect to your main server's remote desktop with IP 139.88.1.20 and port 3389
