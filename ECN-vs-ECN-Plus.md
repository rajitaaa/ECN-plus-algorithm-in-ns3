## ECN  Explicit congestion control
   It is an algorithm to notify the server/sender that the network has experienced congestion in the network. When the AQM algorithms predict that queue length is more then min_thresh. The router sets Congestion Experienced bit. When the receiver receives this packet, it understands that congestion was experienced it sets ECN Echo bit in the ACK packet.If the Echo bit is set the sender understands that it has to reduce its Congestion Window Size. Thus this reduces packet drops when congestion is predicted. If the AQM queue length passes max_thresh the packets are dropped. 
<br />
 TCP SYN and TCP SYN+ACK are used for establishing ECN and hence don't have ECN capability. Clearly TCP SYN and TCP SYN+ACK Packets get dropped during congestion. For small transfers this adds to a lot of waiting.
<br />
## ECN+
So ECN+ algorithm suggests that we should use ECN in TCP control messages too. But we should note that if we use ECN for TCP SYN , TCP SYN Attacks becomes dangerous and gives a lot of scope for malicious attackers. If TCP syn is given ECN the packets get prolonged before getting dropped and spreading the attack. So we can use ECN for SYN+ACK as if a server is initiating a SYN+ACK attack the host will drop the packet as it didn't send a SYN. 

## ECN+/Wait
 While using ECN+ algorithm if we receive a SYN-ACK reply with CE bit set in IP header we infer that congestion has experienced. When such a packet is received we wait for 1 RTT hoping that the congestion in the network will get reduced. We then reduce the ssthresh or the congestion_window to 1 and reply with an ACK and the 3-way handshake is completed.

## ECN set-up in ECN
ECN negotiation happens during the TCP connection setup phase. The ECN-related bits are (i) ECN-Capable (ECT) and (ii) Congestion Experienced (ECN/CE) bits in the IP header, and (iii) ECN-Echo bit in the TCP header.
The client first sets the
ECN-Echo bit in the TCP header of a TCP SYN packet and sends this packet to the receiver. For a SYN packet, the ECN-Echo bit is defined not as a return indication of congestion, but instead as an
indication that the sending TCP is ECN-capable. On receiving the TCP SYN packet, the server sets the ECN-Echo bit in the SYN-ACK packet’s TCP header, and sends this packet back to the client.
When the client receives the above SYN ACK packet, the ECN
capability is negotiated, and both endpoints start an ECN-capable
transport by setting the ECT field in the IP header of data packets.
<br/>
<br/>
Marking (instead of dropping) TCP SYN ACK packets, while leaving the treatment of
the initial TCP SYN packet unchanged from current practice, can only improve performance without causing a threat for system security or stability.
<br />
Using ECN in control packets approach raises several concerns. We should not set ECT field in TCP SYN packet because here are no guarantees that the other endpoint (web server in our scenario) is ECN-
capable, or that it would be able to understand and react if the ECN/CE bits were set by a congested router.Also the ECT field in TCP SYN packets may be misused by malicious clients to improve the well-known TCP SYN attack.By setting the ECT bit in TCP SYN packet’s headers, a malicious client would be able to easily inject a large number of TCP SYN packets through a potentially congested ECN-enabled router. 
We can use ECN in the SYN+ACK .If ECN-Echo bit is set for a incoming packet and if the server is also ECN-capable, there are no obstacles to immediately applying ECN, and setting the ECT bit in the SYN ACK packet.The ”SYN ACK flood” attack would be greatly inefficient [not explained here] so there are no security issues too.

## Response to TCP SYN
  When server receives a TCP SYN with ECN Echo bit set we make the TCP SYN ACK packet ECN capable and set flags appropriately. 
#### Scenario 1: No congestion experienced by the packets. 
 When a packet is received and the CE[Congestion Experienced] bit is 0. The TCP ACK packet [piggybacked with certain data] is sent ,this packet is made ECN capable.
#### Scenario : Congestion Experienced by the packet in the network.
When a TCP SYN ACK packet has the CE bit set. We reduce the Congestion Window and make a TCP ACK[piggybacked with certain data] and wait for 1 RTT and then send the packet into the network.

