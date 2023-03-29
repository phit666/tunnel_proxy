# Tunnel Proxy
This will let your local services be online without opening a port in your main host's firewall, it simply means you can access a service like your home's remote desktop anywhere even if your internet is not public and it can also secure a server by not exposing it's IP address as you can let a dummy host handle the request.

# tunnel
This is console program you will run locally in the same network as your local/internal server, this program will create a network tunnel between the local server and tunnel_proxy.

]$ tunnel

*tunnel.yaml*

    Debug Message: true #Show debug messages in console
    Tunnel Servers:
      - Name: "Tunnel 1"
        Manage IP: 135.99.89.14 #Server IP where you run tunnel_proxy
        Manage Port: 4004 #tunnel_proxy manage port
        Tunnel Port: 4005 #tunnel_proxy tunnel port
        Local Server IP: 127.0.0.1 #local IP of service you want to access
        Local Server Port: 3389 #port of the local service
      - Name: "Tunnel 2"
        Manage IP: 135.99.89.15
        Manage Port: 4000
        Tunnel Port: 4001
        Local Server IP: 127.0.0.1
        Local Server Port: 3389



# tunnel_proxy
This is a console program you will run in your external server/cloud, let say a VPS hosted online with a public IP, now to connect to your local/internal server you will simply need to connect in tunnel_proxy public IP.

All ports of tunnel_proxy should be opened in the firewall.

]$ tunnel_proxy

*tunnel_proxy.yaml*

    Debug Message: true
    Proxy Servers:
      - Proxy Name: "Proxy 1" 
        Max Tunnels: 3 #Max count of tunnels to be opened, service like HTTP should have atleast 5 as the web client is doing parallel request to web server.
        Min Tunnels: 1 #Low water mark of opened tunnels, when an open tunnel count is equal or below this then tunnel_proxy will request to open new tunnels to tunnel.
        Manage Port: 4000 #tunnel's manage port
        Tunnel Port: 4001 #tunnel's tunnel port
        Proxy Port: 33891 #Proxy port, this is the port the client will access.
      - Proxy Name: "Proxy 2"
        Max Tunnels: 3
        Min Tunnels: 1
        Manage Port: 4002
        Tunnel Port: 4003
        Proxy Port: 33892

# Example

Let say you have a Home Assistant server (Debian Linux OS) in your house and you don't have a public IP but you have cheap VPS running in Linux online with public IP 139.88.23.50, now to make your Home Assistant server be accessible online...

Run tunnel within your local network preferrably  in the Home Assistant server.

]$ tunnel

*tunnel.yaml*

    Debug Message: false
    Tunnel Servers:
      - Name: "HA Tunnel"
        Manage IP: 139.88.23.50
        Manage Port: 4004
        Tunnel Port: 4005
        Local Server IP: 127.0.0.1
        Local Server Port: 8123


Run tunnel_proxy in your online VPS (required ports opened)

]$ tunnel_proxy

*tunnel_proxy.yaml*

    Debug Message: false
    Proxy Servers:
      - Proxy Name: "HA Proxy"
        Max Tunnels: 10
        Min Tunnels: 5
        Manage Port: 4000
        Tunnel Port: 4001
        Proxy Port: 8123


Now you can access your Home Assistant server with browser or mobile app with address http://139.88.23.50:8123

