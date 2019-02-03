# QCluster


## Introduction

Flow Scheduling is crucial in data centers, as it directly influences user experience of applications. Many flow scheduling solutions leverage priority queues of commodity switches to enforce their scheduling policies. Given the limited number of priority queues, e.g., 8, flow scheduling solutions should choose the most appropriate queue for each outgoing packet. According to different assumptions and design goals, there are four typical flow scheduling problems: SRPT, LAS, Fair Queuing, and Deadline-Aware Scheduling. Most existing works often address one scheduling problem, and seldom handle a key challenge: how to set an appropriate threshold for each queue. This paper unifies the flow scheduling problem as a clustering problem: how to cluster outgoing packets into k queues? To address this problem, we propose a generic framework, namely QCluster, which can address the above challenge. Based on k-means, QCluster clusters packets with the similar weights into the same queue at line rate. For different flow scheduling problems, packet weight has different definitions (e.g., remaining size, bytes sent). QCluster has two key differences with existing works: 1) QCluster is easy to be deployed with two options: only deployed in programmable switches; or only deployed in end hosts. 2) QCluster can automatically adjust the thresholds of queues for each switch at line rate. Experimental results on testbeds with P4Switch and ns-2 show that QCluster achieves comparable or better performance compared with the best algorithm for each scheduling problem. QCluster can reduce the overall average flow completion time (FCT) up to 21.7%, and reduce the average FCT for short flows up to 56.6% over existing solutions.

## Repository structure

This repository consists of three parts.
Please refer to the readme in each folder for more details about how to use the codes.

### Implementation in P4 language

We implement our QCluster for LAS (least attained service first) in P4 language, and build a testbed with a Tofino switch.
The `p4` folder contains the p4 code and the control plane code for it.

### Implementation on end-hosts

We implement our QCluster for LAS as a linux kernel module. See files in `endhost` about how to compile the codes and how to use it.

### Simulation on ns-2

We also conduct large-scale simulations on ns-2 for all the aforementioned four tasks (SRPT, LAS, Fair Queuing, and Deadline-Aware Scheduling). The `ns2` folder contains our implementation of QCluster and the scripts we used in our experiments.
We implement our QCluster for LAS as a linux kernel module. See files in `endhost` about how to compile the codes and how to use it.
