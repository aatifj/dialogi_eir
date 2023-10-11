# dialogic_eir
an EIR built on dialogic protocol stack
This EIR is built using dialogic Sigtran protocol stack.

Download and Install dialogic sigtran protocol stack for your operating system. https://www.dialogic.com/signaling-and-ss7-components/download/dsi-interface-protocol-stacks
Update config.txt and connect your dialogic SS7 stack to you MSC and SGSNs on SS7 links.
Run gctload binary that will start the dialogic stack
Run this code through C make script .
The code will read Check_IMEI requests from network, extract IMSI, MSISDN and IMEI , write it to a file . It also blocks the IMEI that is defined as blacklisted in the blocklist.in file.


