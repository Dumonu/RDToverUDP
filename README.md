# RDToverUDP
UDP based implementation of Reliable Data Transfer (RDT) Protocols

## Collaborators
- Jacob Cherry
- Eric Kang
- Seshen Fraser

## RDT Header Field Documentation
### Header Fields
RDT Header | TCP Header | Reasoning
-----------|------------|----------
8-bit Sequence Number | 32-bit Sequence Number | Project requirements list 8-bit sequence numbers. Required for robust RDT.
8-bit Acknowledgement Number | 32-bit Acknowledgement Number | Matches the sequence number. Used for ACKing packets.
FIN Flag | FIN Flag | Used to teardown the connection
SYN Flag | SYN Flag | Used to establish the connection
RST Flag | RST Flag | Used to abort connection on an error
ACK Flag | ACK Flag | Used to acknowledge packets
8-bit Receiver Window | 16-bit Receiver Window | Decided to use the receiver window to count packets, rather than bytes, so that a smaller number can be used. It's also 8-bit so that the header divides into 16-bit words evenly for checksum calculation.
16-bit Checksum | 16-bit Checksum | Used for error detection, necessary for RDT.

### TCP Fields not in RDT Header
TCP Header | Reasoning
-----------|----------
Source Port | This will be received from UDP through `recvfrom()`.
Dest Port | Any message arriving at the UDP socket will have a known destination port.
Header Length | Fixed length headers are easier, so this header is fixed length and therefore doesn't require an embedded length field.
PSH Flag | This isn't necessary for our requirements.
URG Flag | We assume that packets will never need to be marked as urgent
Urgent Pointer | We don't use the URG flag, so it is irrelevant.