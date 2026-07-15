# SocketSpy
SocketSpy was created to meet the specific need of real-time monitoring of network traffic on Linux systems where packet capture tools like tcpdump are unavailable and root privileges cannot be used.  
This tool operates as a single binary and produces output nearly equivalent to a command that loops the ss command every 1ms.  
This tool is essentially a network version of [pspy](https://github.com/DominicBreuker/pspy). Currently, it only supports IPv4 TCP and UDP.  
## Build 
`gcc -Wall -O3 -static ./socketspy.c -o socketspy`  
## Usage
Without timeout  
`./socketspy 0`  
If you want to run it with a timeout (if you want it to automatically terminate after 10 seconds)  
`./socketspy 10`  
If you want to output the results to a file  (If a file is specified, nothing is displayed on standard output)  
`./socketspy 10 log.txt`
## Output
```
┌──(kali㉿kali)-[~/]
└─$ ./socketspy    
Usage: ./socketspy <timeout_sec> <output_file>                                                                                                                                                                                                                                                 
┌──(kali㉿kali)-[~/]
└─$ ./socketspy 30                                  
[2026-07-15 08:41:17] 127.0.0.1:7687/TCP <---> 0.0.0.0:0/TCP
[2026-07-15 08:41:17] 127.0.0.1:8080/TCP <---> 0.0.0.0:0/TCP
[2026-07-15 08:41:17] 127.0.0.1:7474/TCP <---> 0.0.0.0:0/TCP
[2026-07-15 08:41:17] 0.0.0.0:22/TCP <---> 0.0.0.0:0/TCP
[2026-07-15 08:41:17] 192.168.87.128:49358/TCP <---> 34.107.243.93:443/TCP
[2026-07-15 08:41:17] 192.168.87.128:56468/TCP <---> 34.107.243.93:443/TCP
[2026-07-15 08:41:17] 192.168.87.128:68/UDP <---> 192.168.87.254:67/UDP
[2026-07-15 08:41:22] 192.168.87.128:54178/UDP <---> 192.168.87.2:53/UDP PID:38701 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-07-15 08:41:22] 192.168.87.128:37027/UDP <---> 192.168.87.2:53/UDP PID:581673 CMD:dig google.com 
[2026-07-15 08:41:22] 192.168.87.128:52538/TCP <---> 142.250.21.95:443/TCP PID:38701 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-07-15 08:41:25] 192.168.87.128:33600/UDP <---> 192.168.87.2:53/UDP PID:581720 CMD:dig google.com 
[2026-07-15 08:41:27] 192.168.87.128:43384/TCP <---> 172.217.211.138:80/TCP PID:581739 CMD:curl 172.217.211.138:80 
[2026-07-15 08:41:36] 192.168.87.128:45681/UDP <---> 192.168.87.2:53/UDP PID:581857 CMD:dig bing.com 
[2026-07-15 08:41:45] 192.168.87.128:37252/TCP <---> 150.171.28.10:80/TCP PID:581994 CMD:curl 150.171.28.10:80 
[2026-07-15 08:41:49] 192.168.87.128:46441/UDP <---> 192.168.87.2:53/UDP PID:38701 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-07-15 08:41:49] 192.168.87.128:59778/TCP <---> 34.107.243.93:443/TCP PID:38701 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-07-15 08:41:49] 192.168.87.128:54035/UDP <---> 192.168.87.2:53/UDP
[2026-07-15 08:41:50] 192.168.87.128:59794/TCP <---> 34.107.243.93:443/TCP PID:38701 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-07-15 08:41:50] 192.168.87.128:41300/UDP <---> 192.168.87.2:53/UDP PID:38701 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-07-15 08:41:50] 192.168.87.128:55350/TCP <---> 199.232.149.91:443/TCP PID:38701 CMD:/usr/lib/firefox-esr/firefox-esr 
[2026-07-15 08:41:50] 192.168.87.128:56750/UDP <---> 192.168.87.2:53/UDP
[2026-07-15 08:41:50] 192.168.87.128:50920/TCP <---> 34.160.144.191:443/TCP PID:38701 CMD:/usr/lib/firefox-esr/firefox-esr 
                                                                                                                             
┌──(kali㉿kali)-[~/tools/SocketSpy]
└─$
```
