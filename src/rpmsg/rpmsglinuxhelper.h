//
// Created by manhpd9 on 14/10/2025.
//

#ifndef RPMSG_LINUX_HELPER_HPP
#define RPMSG_LINUX_HELPER_HPP

#include <string>
#include <linux/rpmsg.h>

#define RPMSG_BUS_SYS "/sys/bus/rpmsg"

class RPMsgLinuxHelper {
public:
    static int app_rpmsg_create_ept(int rpfd, struct rpmsg_endpoint_info *eptinfo);

    static char *get_rpmsg_ept_dev_name(const char *rpmsg_char_name,
                                        const char *ept_name,
                                        char *ept_dev_name);

    static int bind_rpmsg_chrdev(const char *rpmsg_dev_name);

    static int get_rpmsg_chrdev_fd(const char *rpmsg_dev_name, char *rpmsg_ctrl_name);

    static int lookup_channel(char *out, struct rpmsg_endpoint_info *pep);

private:
    static void set_src_dst(char *out, struct rpmsg_endpoint_info *pep);
};

#endif // RPMSG_LINUX_HELPER_HPP
