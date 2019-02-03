#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

// For netfilter
#include <linux/netfilter.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/tcp.h>
#include <linux/ktime.h>
#include <linux/netfilter_ipv4.h>

#include "nf_wrapper.h"
#include "flow.h"
#include "jprobe.h"

const int MAX_NUM_AGING = 2560;
// Borrow code from linux kernel 4.2
u32 qcluster_int_sqrt64(u64 x) {
    u64 b, m, y = 0;

    if (x <= ULONG_MAX)
        return int_sqrt((unsigned long) x);

    m = 1ULL << (fls64(x) & ~1ULL);
    while (m != 0) {
        b = y + m;
        y >>= 1;

        if (x >= b) {
            x -= b;
            y += m;
        }
        m >>= 2;
    }

    return y;
}

struct PIAS_Flow_Table ft;

struct {
    long long bytes_cnt;
    int flow_cnt;
    spinlock_t lock;
} qcluster_queue_info[8];

static int pias_set_operation(const char *val, struct kernel_param *kp);
static int pias_noget(const char *val, struct kernel_param *kp);

static int get_priority(u32 bytes_sent) {
    u64 this_avg = 0;
    if (qcluster_queue_info[0].flow_cnt == 0) return 0;
    this_avg = qcluster_queue_info[0].bytes_cnt / qcluster_queue_info[0].flow_cnt;
    if (this_avg == 0) return 0;  
    for (int i = 0; i < 7; ++i) {
        u64 next_avg = 0;
        if (qcluster_queue_info[i + 1].flow_cnt == 0) return i+1;
    next_avg = qcluster_queue_info[i + 1].bytes_cnt / qcluster_queue_info[i + 1].flow_cnt;
    if (next_avg <= this_avg) return i+1;
        if (bytes_sent <= qcluster_int_sqrt64(this_avg * next_avg)) {
            return i;
        }
        this_avg = next_avg;
    }
    return 7;
}

void print_priority(void) {
    for (int i = 0; i < 7; ++i) {
        u64 average = 0;
        if (qcluster_queue_info[i].flow_cnt != 0)
            average = qcluster_queue_info[i].bytes_cnt / qcluster_queue_info[i].flow_cnt;
        printk(KERN_INFO "Qcluster: %d-th average is %llu\n", i, average);
    }

    u64 this_avg = 0;
    if (qcluster_queue_info[0].flow_cnt != 0) 
        this_avg = qcluster_queue_info[0].bytes_cnt / qcluster_queue_info[0].flow_cnt;
    for (int i = 0; i < 7; ++i) {
        u64 next_avg = 0;
        if (qcluster_queue_info[i + 1].flow_cnt != 0) 
            next_avg = qcluster_queue_info[i + 1].bytes_cnt / qcluster_queue_info[i + 1].flow_cnt;
        
        printk(KERN_INFO "Qcluster: %d-th thresh is %d\n", i + 1, qcluster_int_sqrt64(this_avg * next_avg));
        this_avg = next_avg;
    }
}

/* mark DSCP and enable ECN */
static inline void pias_enable_ecn_dscp(struct sk_buff *skb, u8 dscp)
{
    if (likely(skb && skb_make_writable(skb, sizeof(struct iphdr))))
        ipv4_change_dsfield(ip_hdr(skb), 0x00, (dscp << 2)|INET_ECN_ECT_0);
}

/* Determine whether seq1 is larger than seq2 */
static inline bool pias_is_seq_larger(u32 seq1, u32 seq2)
{
    if (likely(seq1 > seq2 && seq1 - seq2 <= 4294900000))
        return true;
    else if (seq1 < seq2 && seq2 - seq1 > 4294900000)
        return true;
    else
        return false; 
}

/* param_dev: NIC to operate PIAS */
static char *param_dev = NULL;
MODULE_PARM_DESC(param_dev, "Interface to operate PIAS (NULL=all)");
module_param(param_dev, charp, 0);

static int param_port __read_mostly = 0;
MODULE_PARM_DESC(param_port, "Port to match (0=all)");
module_param(param_port, int, 0);

static NF_CALLBACK(qcluster_process_packet, skb) {
    // const struct net_device *out = state->out;
    // if (!out)
    //     return NF_ACCEPT;
    // if (param_dev && strncmp(out->name, param_dev, IFNAMSIZ) != 0)
    //     return NF_ACCEPT;

    struct iphdr *iph = NULL;
    struct tcphdr *tcph = NULL;
    iph = ip_hdr(skb);

    // We don't process non-TCP packets
    if (unlikely(!iph))
        return NF_ACCEPT;
    if (iph->protocol != IPPROTO_TCP)
        return NF_ACCEPT;

    // printk(KERN_INFO "QCluster: get a TCP packet.\n");
    // Now we can process the TCP packet in skb
    tcph = (struct tcphdr*)((__u32*)iph + iph->ihl);
    if (param_port != 0 && ntohs(tcph->source) != param_port && ntohs(tcph->dest) != param_port)
        return NF_ACCEPT;

    struct PIAS_Flow f;    //PIAS flow structure
    PIAS_Init_Flow(&f);
    f.local_ip = iph->saddr;
    f.remote_ip = iph->daddr;
    f.local_port = (u16)ntohs(tcph->source);
    f.remote_port = (u16)ntohs(tcph->dest);
    ktime_t now = ktime_get();
    int priority = 0;

    if (tcph->syn) {
        // create entry for a new connection
        f.info.last_update_time = now;
        if (!PIAS_Insert_Table(&ft, &f, GFP_ATOMIC))
            printk(KERN_INFO "PIAS: insert fail\n");
    } else if (tcph->fin || tcph->rst) {
        int result = PIAS_Delete_Table(&ft, &f);
        if (result == 0)
            printk(KERN_INFO "PIAS: delete fail\n");
    } else {
        u32 payload_len = ntohs(iph->tot_len) - (iph->ihl << 2) - (tcph->doff << 2);
        struct PIAS_Flow* ptr = PIAS_Search_Table(&ft, &f);
        u32 seq = (u32)ntohl(tcph->seq);
        if (payload_len >= 1)
            seq = seq + payload_len - 1;

        if (ptr) {
            unsigned long flags;
            spin_lock_irqsave(&(ptr->lock), flags);
            s64 idle_time = ktime_us_delta(now, ptr->info.last_update_time); 

            if (pias_is_seq_larger(seq, ptr->info.last_seq)) {
                //Update sequence number
                ptr->info.last_seq = seq;
                //Update bytes sent
                ptr->info.bytes_sent += payload_len;
            }
            ptr->info.last_update_time = now;
            priority = get_priority(ptr->info.bytes_sent);
            int new_flow = (ptr->info.queue_dist[priority] == 0 ? 1 : 0);
            int delta = ptr->info.bytes_sent - ptr->info.queue_dist[priority];
            ptr->info.queue_dist[priority] = ptr->info.bytes_sent;
            spin_unlock_irqrestore(&(ptr->lock), flags);

            // If payload length is small, consider it as the last packet of a message
            // Do not add to the queue table
            if (payload_len < 1400) {
                priority = 0;
            } else {  
                // update queue table based on delta and priority, ideally
                spin_lock_irqsave(&(qcluster_queue_info[priority].lock), flags);
                int div_pkt = 0;
                if (payload_len > 0) div_pkt += 1;
                if (payload_len > 1500) div_pkt += (payload_len - 1) / 1500;
                qcluster_queue_info[priority].flow_cnt += div_pkt;
                qcluster_queue_info[priority].bytes_cnt += ptr->info.bytes_sent * div_pkt;
            
                if (qcluster_queue_info[priority].flow_cnt > MAX_NUM_AGING) {
                    qcluster_queue_info[priority].bytes_cnt /= 2;
                    qcluster_queue_info[priority].flow_cnt /= 2;
                }
                spin_unlock_irqrestore(&(qcluster_queue_info[priority].lock), flags);
            }
        }
    }

    pias_enable_ecn_dscp(skb, (u8)(7 - priority));
    //printk(KERN_INFO "qcluster with priority %d\n", priority);
    
    return NF_ACCEPT;
}


static struct nf_hook_ops qcluster_hook_ops;

// For netfilter register
static int qcluster_netfilter_init(void) {
    qcluster_hook_ops.hook = qcluster_process_packet;
    qcluster_hook_ops.pf = PF_INET;
    qcluster_hook_ops.hooknum = NF_INET_POST_ROUTING;
    qcluster_hook_ops.priority = NF_IP_PRI_FIRST;

    int register_result;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
    register_result = nf_register_net_hook(&init_net, &qcluster_hook_ops);
#else
    register_result = nf_register_hook(qcluster_hook_ops);
#endif
    if (register_result) {
        printk(KERN_INFO "Cannot register Netfilter hook at NF_INET_POST_ROUTING\n");
        return 1;
    }

    return 0;
}

static int qcluster_netfilter_exit(void) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
    nf_unregister_net_hook(&init_net, &qcluster_hook_ops);
#else
    nf_unregister_hook(&qcluster_hook_ops);
#endif
    return 0;
}

static int pias_set_operation(const char *val, struct kernel_param *kp)
{
    if (strncmp(val, "print", 5) == 0)
    {
        print_priority();
    }
    else
        printk(KERN_INFO "PIAS: unrecognized flow table operation\n");

    return 0;
}

static int pias_noget(const char *val, struct kernel_param *kp)
{
    return 0;
}


static int qcluster_module_init(void) {
    // Register nethooks
    
    PIAS_Init_Table(&ft);
    module_param_call(param_table_operation, pias_set_operation, pias_noget, NULL, S_IWUSR); //Write permission by owner
    qcluster_netfilter_init();
    PIAS_JProbe_Init();
    memset(qcluster_queue_info, 0, sizeof(qcluster_queue_info));
    printk(KERN_EMERG "qcluster init\n");

    return 0;
}

static void qcluster_module_exit(void) {

    PIAS_JProbe_Exit();
    qcluster_netfilter_exit();
    PIAS_Exit_Table(&ft);
    printk(KERN_EMERG "qcluster exit\n");
    // return 0;
}

module_init(qcluster_module_init);
module_exit(qcluster_module_exit);

MODULE_LICENSE("GPL");
