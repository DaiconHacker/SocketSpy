# SocketSpy
SocketSpy was created to meet the specific need of real-time monitoring of network traffic on Linux systems where packet capture tools like tcpdump are unavailable and root privileges cannot be used.
This tool operates as a single binary and produces output nearly equivalent to a command that loops the ss command every 1ms.
This tool is essentially a network version of pspy. Currently, it only supports IPv4 TCP.
## Build
`gcc -Wall -O2 -static ./socketspy.c -o socketspy`
## Usage
Without timeout
`./socketspy 0`
If you want to run it with a timeout (if you want it to automatically terminate after 10 seconds)
`./socketspy 10`
## Output
```
┌──(kali㉿kali)-[~/]
└─$ ./socketspy    
usage  : ./socketspy [timeout seconds]
example: ./socketspy 10 <--- execute 10 seconds
example: ./socketspy 0  <--- no timeout
                                                                                                                                                                                                                                                             
┌──(kali㉿kali)-[~/ACD_Tools]
└─$ ./socketspy 10
[2026-04-15 09:16:51] 0.0.0.0:22 <---> 0.0.0.0:0
[2026-04-15 09:16:54] 192.168.87.128:43286 <---> 34.160.144.191:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:54] 192.168.87.128:43290 <---> 34.160.144.191:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:54] 192.168.87.128:58148 <---> 146.75.113.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:58158 <---> 146.75.113.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:56050 <---> 142.250.21.95:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:42764 <---> 34.107.243.93:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:58170 <---> 146.75.113.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:42768 <---> 34.107.243.93:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:33698 <---> 34.107.221.82:80 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:58178 <---> 146.75.113.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:58192 <---> 146.75.113.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:58196 <---> 146.75.113.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:58204 <---> 146.75.113.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:40188 <---> 151.101.1.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:40198 <---> 151.101.1.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:40202 <---> 151.101.1.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:40218 <---> 151.101.1.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:40230 <---> 151.101.1.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-04-15 09:16:55] 192.168.87.128:40238 <---> 151.101.1.91:443 PID:135463 CMD:/usr/lib/firefox-esr/firefox-esr 
                                                                                                                                                                                                                                                             
┌──(kali㉿kali)-[~/]
└─$
```
