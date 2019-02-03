## Description

The kernel module of QCluster endhost version.

## Compiling
 The QCluster endhost module is verified in the Linux kernel Version 4.13.0-36-generic. To compile it:  

<pre><code>$ cd endhost<br/>
$ make</code></pre>

Then the kernel module of QCluster: `qcluster.ko` is built. 

## Installing 
To install it:
<pre><code>$ insmod qcluster.ko<br/>
</code></pre>

To remove it:
<pre><code>$ rmmod qcluster<br/>
</code></pre>

## Usage
QCluster endhost module implements an interface to print the thresholds.

<pre><code>$ echo -n print > /sys/module/qcluster/parameters/param_table_operation<br/>
$ dmesg|tail<br/>
</code></pre>
