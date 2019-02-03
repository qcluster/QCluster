## QCluter in P4 language

### Requirements
- This codes is written for [Tofino Switch](https://barefootnetworks.com/products/brief-tofino/). Please compile and run the codes using Tofino model or a Tofino ASIC.
- The control plane is written in Python 2 using PD-API. To run it, the relevant components should be compiled with `--with-thrift`.

### Usage
- The p4 code does not contain the processing logic for ARP packets, so it cannot behave like a normal switch. Please add the ARP codes or set ARP tables manually during using.
- `qcluster.py` is the control plane code for Tofino ASIC target, based on PD-API.
- For simplicity, the forwarding is based on ipv4 dest address. Please change the `ip2port` variable in `qcluster.py`, and run the code to apply it.
- Example usage of `qcluster.py`:
	- `python qcluster.py --ecn=120`: Set the ECN marking thresholds to 120 packets.
	- `python qcluster.py --set --ports=24,40,56`: Run QCluster algorithm on Port 24, 40, and 56.
