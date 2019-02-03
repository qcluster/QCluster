# QCluster for Fair Queuing
## Installation
Download [Network Simulator (NS) 2.34](https://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.34/) and unzip it.
```
$ tar -zxvf ns-allinone-2.34.tar.gz
```

Copy pFabric.patch to the *top ns-2.34 folder* (```ns-allinone-2.34```) and apply the patch. Then install NS2.
```
$ cd ns-allinone-2.34
$ patch -p1 --ignore-whitespace -i pFabric.patch
$ ./install
```
Copy files in tcp folder to ```ns-allinone-2.34/ns-2.34/tcp/```.

Copy files in queue folder to ```ns-allinone-2.34/ns-2.34/queue/```.

Add ```queue/priority.o, queue/known.o, queue/unknown.o, queue/appro_unknown.o,``` to ```ns-allinone-2.34/ns-2.34/Makefile```.
 
Run ```make``` on ```/ns-allinone-2.34/ns-2.34```.

## Running Large-Scale Simulations
You can find simulation scrips for Fair Queuing in scripts folder. And we will show the usage of some files in the folders.

- spine_empirical.tcl and tcp-common-opt.tcl are NS2 TCL simulation scripts.  
- result.py is used to parse final results.  

There are many parameters to configue in `[transport]Param.py`. Note that you need to modify ```ns_path``` and ```sim_script ``` correspondingly. 

For each simulation, it will create some folders, and each folder contains two files: ```flow.tr``` and ```logFile.tr```. The ```flow.tr``` gives flow completion time results with the following format:
```
number of packets, flow completion time, number of timeouts, src ID, dst ID
```

You can use result.py to parse ```flow.tr``` files as follows:
```
$ python result.py -a -i [path]/flow.tr
```
