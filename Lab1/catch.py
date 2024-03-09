from scapy.all import *
import base64

p = rdpcap('lab_tcpdump.pcap')
for i in range(len(p)) :
    try:
        if p[i].ttl>160 and TCP in p[i] :
            answer=str(bytes((p[i].payload))[50:].decode("ascii"))
            print('packet number:', i)
            if len(answer)<200:
                print(base64.b64decode(answer))
                print()
    except:
        pass
        continue